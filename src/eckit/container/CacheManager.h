/*
 * (C) Copyright 1996-2017 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @author Tiago Quintino
/// @author Baudouin Raoult
/// @date   May 2015

#ifndef eckit_container_CacheManager_h
#define eckit_container_CacheManager_h

#include <sys/stat.h>

#include <string>
#include <functional>
#include <string>

#include "eckit/config/LibEcKit.h"
#include "eckit/config/Resource.h"
#include "eckit/eckit.h"
#include "eckit/filesystem/PathExpander.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/io/FileLock.h"
#include "eckit/memory/NonCopyable.h"
#include "eckit/memory/ScopedPtr.h"
#include "eckit/os/AutoUmask.h"
#include "eckit/os/Semaphore.h"
#include "eckit/parser/StringTools.h"
#include "eckit/parser/Tokenizer.h"
#include "eckit/thread/AutoLock.h"
#include "eckit/types/FixedString.h"
#include "eckit/utils/MD5.h"

namespace eckit {

template<class K, class V, int S, class L>
class BTree;

class BTreeLock;

//----------------------------------------------------------------------------------------------------------------------

/// Filesystem Cache Manager

class CacheManagerBase : private NonCopyable {

public: // methods

    CacheManagerBase(const std::string& loaderName,
                     size_t maxCacheSize,
                     const std::string& extension);
    ~CacheManagerBase();

    std::string loader() const;

protected:

    void touch(const PathName&) const;

private: // members

    std::string loaderName_;
    size_t maxCacheSize_;
    std::string extension_;

    typedef FixedString<MD5_DIGEST_LENGTH * 2> cache_key_t;

    struct cache_entry_t {
        size_t size_;
        size_t count_;
        time_t last_;
    };

    typedef BTree<cache_key_t, cache_entry_t, 64 * 1024, BTreeLock> cache_btree_t;

    mutable eckit::ScopedPtr<cache_btree_t> btree_;
};


//----------------------------------------------------------------------------------------------------------------------


class CacheManagerNoLock {
public:
    CacheManagerNoLock() {}
    void lock() {}
    void unlock() {}
};

//----------------------------------------------------------------------------------------------------------------------

class CacheManagerFileSemaphoreLock {

    PathName path_;
    eckit::Semaphore lock_;

public:
    CacheManagerFileSemaphoreLock(const std::string& path);
    void lock();
    void unlock();
};

//----------------------------------------------------------------------------------------------------------------------

class CacheManagerFileFlock {

    eckit::FileLock lock_;

public:
    CacheManagerFileFlock(const std::string& path);
    void lock();
    void unlock();
};

//----------------------------------------------------------------------------------------------------------------------


template <class Traits>
class CacheManager : public CacheManagerBase {

public: // methods

    typedef typename Traits::value_type value_type;

    class CacheContentCreator {
    public:
        virtual void create(const PathName&, value_type& value, bool& saved) = 0;
    };

    typedef std::string key_t;

public: // methods

    explicit CacheManager(const std::string& loaderName,
                          const std::string& roots,
                          bool throwOnCacheMiss,
                          size_t maxCacheSize);

    PathName getOrCreate(const key_t& key,
                         CacheContentCreator& creator,
                         value_type& value) const;

private: // methods


    bool get(const key_t& key, PathName& path) const;

    PathName stage(const key_t& key, const PathName& root) const;

    bool commit(const key_t& key, const PathName& path, const PathName& root) const;

    PathName entry(const key_t& key, const std::string& root) const;


private: // members

    std::vector<PathName> roots_;
    bool throwOnCacheMiss_;
};



//----------------------------------------------------------------------------------------------------------------------

// NOTE : this should be in the .cc but we have the non-template CacheManagerBase there not to have duplicate symbols

template<class Traits>
CacheManager<Traits>::CacheManager(const std::string& loaderName,
                                   const std::string& roots,
                                   bool throwOnCacheMiss,
                                   size_t maxCacheSize) :
    CacheManagerBase(loaderName, maxCacheSize, Traits::extension()),
    throwOnCacheMiss_(throwOnCacheMiss) {

    eckit::Tokenizer parse(":");
    std::vector<std::string> v;
    parse(roots, v);

    for (std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i) {

        std::string path = *i;

        // entries with e.g. {CWDFS}/cache will be expanded with PathExpander factory CWDFS

        StringList vl = StringTools::listVariables(path);
        for (StringList::const_iterator var = vl.begin(); var != vl.end(); ++var) {
            path = PathExpander::expand(*var, path);
        }

        roots_.push_back(path);
    }

    Log::debug<LibEcKit>() << "CacheManager roots " << roots_ << std::endl;

}

template<class Traits>
bool CacheManager<Traits>::get(const key_t& key, PathName& v) const {

    for (std::vector<PathName>::const_iterator j = roots_.begin(); j != roots_.end(); ++j) {
        PathName p = entry(key, *j);
        if (p.exists()) {
            v = p;
            Log::debug<LibEcKit>() << "CacheManager found path " << p << std::endl;

            if (j == roots_.begin()) {
                // Only update first cache
                touch(p);
            }

            return true;
        }
    }

    if (throwOnCacheMiss_) {
        std::ostringstream oss;
        oss << "CacheManager cache miss: key=" << key << ", tried:";

        const char* sep = " ";
        for (std::vector<PathName>::const_iterator j = roots_.begin(); j != roots_.end(); ++j) {
            PathName p = entry(key, *j);
            oss << sep << p;
            sep = ", ";
        }

        throw UserError(oss.str());
    }

    return false;
}

template<class Traits>
PathName CacheManager<Traits>::entry(const key_t &key, const std::string& root) const {
    std::ostringstream oss;
    oss <<  root
        << "/"
        << Traits::name()
        << "/"
        << Traits::version()
        << "/"
        << key
        << Traits::extension();
    return PathName(oss.str());
}

template<class Traits>
PathName CacheManager<Traits>::stage(const key_t& key, const PathName& root) const {

    PathName p = entry(key, root);
    AutoUmask umask(0);
    // FIXME: mask does not seem to affect first level directory
    p.dirName().mkdir(0777);  // ensure directory exists
    Log::info() << "CacheManager creating file " << p << std::endl;
    // unique file name avoids race conditions on the file from multiple processes
    return PathName::unique(p);
}

template<class Traits>
bool CacheManager<Traits>::commit(const key_t& key, const PathName& tmpfile, const PathName& root) const
{
    PathName file = entry(key, root);
    try {
        SYSCALL(::chmod(tmpfile.asString().c_str(), 0444));
        PathName::rename( tmpfile, file );
    } catch ( FailedSystemCall& e ) { // ignore failed system call -- another process nay have created the file meanwhile
        Log::debug() << "Failed rename of cache file -- " << e.what() << std::endl;
        return false;
    }
    return true;
}

template<class Traits>
PathName CacheManager<Traits>::getOrCreate(const key_t& key,
        CacheContentCreator& creator,
        value_type& value) const {

    PathName path;

    if (get(key, path)) {
        eckit::Log::debug() << "Loading cache file "
                            << path
                            << std::endl;

        Traits::load(*this, value, path);
        return path;
    }
    else {

        for (std::vector<PathName>::const_iterator j = roots_.begin(); j != roots_.end(); ++j) {

            eckit::Log::info() << "Cache file "
                               << entry(key, *j)
                               << " does not exist"
                               << std::endl;

            try {

                typename Traits::Locker locker(entry(key, *j));
                eckit::AutoLock<typename Traits::Locker> lock(locker);

                // Some
                if (!get(key, path)) {
                    eckit::Log::info() << "Creating cache file "
                                       << entry(key, *j)
                                       << std::endl;

                    eckit::PathName tmp = stage(key, *j);
                    bool saved = false; // The creator may decide to save

                    creator.create(tmp, value, saved);
                    if (!saved) {
                        Traits::save(*this, value, tmp);
                    }
                    ASSERT(commit(key, tmp, *j));

                    // We reload from cache so we use the proper loader
                    // e.g. mmap of shared-mem...
                    // ASSERT(get(key, path));
                    Traits::load(*this, value, path);

                    if (j == roots_.begin()) {
                        // Only update first cache
                        touch(path);
                    }

                }
                else {
                    // touch() is done in the get() above
                    eckit::Log::debug() << "Loading cache file "
                                        << entry(key, *j)
                                        << " (created by another process)"
                                        << std::endl;
                    Traits::load(*this, value, path);
                }

                // ASSERT(get(key, path));

                return path;

            } catch (FailedSystemCall& e) {
                eckit::Log::error() << "Error creating cache file: "
                                    << entry(key, *j)
                                    << " (" << e.what() << ")" << std::endl;
            }

        }

    }

    std::ostringstream oss;
    oss << "CacheManager cannot create key=" << key << ", tried:";

    const char* sep = " ";
    for (std::vector<PathName>::const_iterator j = roots_.begin(); j != roots_.end(); ++j) {
        PathName p = entry(key, *j);
        oss << sep << p;
        sep = ", ";
    }

    throw UserError(oss.str());

}


//----------------------------------------------------------------------------------------------------------------------


}  // namespace eckit

#endif
