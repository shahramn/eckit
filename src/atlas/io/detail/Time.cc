/*
 * (C) Copyright 2020 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#include "Time.h"

#if __GNUC__ && __GNUC__ < 5
// Some older compilers, e.g. intel 17 rely on GCC 4, which does not have std::put_time implemented yet, even though it is in C++11.
// So implement it here.

#include <ios>       // std::ios_base
#include <iterator>  // std::ostreambuf_iterator
#include <locale>    // std::use_facet, std::time_put
#include <ostream>   // std::basic_ostream

namespace std {
template <typename CharT>
struct _put_time {
    const std::tm* time;
    const char* fmt;
};

template <typename CharT>
inline _put_time<CharT> put_time( const std::tm* time, const CharT* fmt ) {
    return {time, fmt};
}

template <typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<( std::basic_ostream<CharT, Traits>& os, _put_time<CharT> f ) {
    typedef typename std::ostreambuf_iterator<CharT, Traits> Iter;
    typedef std::time_put<CharT, Iter> TimePut;

    const CharT* const fmt_end = f.fmt + Traits::length( f.fmt );
    const TimePut& mp          = std::use_facet<TimePut>( os.getloc() );

    std::ios_base::iostate err = std::ios_base::goodbit;
    try {
        if ( mp.put( Iter( os.rdbuf() ), os, os.fill(), f.time, f.fmt, fmt_end ).failed() )
            err |= std::ios_base::badbit;
    }
    catch ( ... ) {
        err |= std::ios_base::badbit;
    }

    if ( err )
        os.setstate( err );

    return os;
}
}  // namespace std

#endif


#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "eckit/log/JSON.h"

#include "atlas/runtime/Log.h"

namespace atlas {
namespace io {

namespace {

static std::time_t to_time_t( Time time ) {
    return std::time_t( time.tv_sec );
}

static Time from_time_point( std::chrono::time_point<std::chrono::system_clock> t ) {
    using namespace std::chrono;
    auto since_epoch      = t.time_since_epoch();
    auto sec_since_epoch  = duration_cast<seconds>( since_epoch );
    auto nsec_since_epoch = duration_cast<nanoseconds>( since_epoch );
    auto extra_nsec       = duration_cast<nanoseconds>( nsec_since_epoch - sec_since_epoch );

    Time time;
    time.tv_sec  = static_cast<std::uint64_t>( sec_since_epoch.count() );
    time.tv_nsec = static_cast<std::uint64_t>( extra_nsec.count() );
    return time;
}

}  // namespace


Time Time::now() {
    return from_time_point( std::chrono::system_clock::now() );
}

void Time::print( std::ostream& out ) const {
    // Will print time-date in ISO 8601 format: 1970-01-01T00:00:00.123456789Z
    auto time = to_time_t( *this );
    out << std::put_time( ::gmtime( &time ), "%FT%T" ) << "." << tv_nsec << "Z";
}

std::ostream& operator<<( std::ostream& out, const Time& time ) {
    time.print( out );
    return out;
}

eckit::JSON& operator<<( eckit::JSON& out, const Time& time ) {
    std::stringstream s;
    s << time;
    out << s.str();
    return out;
}

std::string Time::str() const {
    std::stringstream s;
    print( s );
    return s.str();
}

}  // namespace io
}  // namespace atlas
