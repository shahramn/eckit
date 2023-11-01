/*
 * (C) Copyright 2020 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#pragma once

#include <vector>

#include "eckit/codec/Data.h"
#include "eckit/codec/Exceptions.h"
#include "eckit/codec/Metadata.h"
#include "eckit/codec/detail/TypeTraits.h"
#include "eckit/codec/types/array/ArrayMetadata.h"
#include "eckit/codec/types/array/ArrayReference.h"

namespace std {

//---------------------------------------------------------------------------------------------------------------------

template <typename T, atlas::io::enable_if_array_datatype<T> = 0>
void interprete(const std::vector<T>& vector, atlas::io::ArrayReference& out) {
    using atlas::io::ArrayReference;
    out = ArrayReference{vector.data(), {int(vector.size())}};
}

//---------------------------------------------------------------------------------------------------------------------

template <typename T, atlas::io::enable_if_array_datatype<T> = 0>
void decode(const atlas::io::Metadata& m, const atlas::io::Data& encoded, std::vector<T>& out) {
    atlas::io::ArrayMetadata array(m);
    if (array.datatype().kind() != atlas::io::ArrayMetadata::DataType::kind<T>()) {
        std::stringstream err;
        err << "Could not decode " << m.json() << " into std::vector<" << atlas::io::demangle<T>() << ">. "
            << "Incompatible datatypes: " << array.datatype().str() << " and " << atlas::io::ArrayMetadata::DataType::str<T>() << ".";
        throw atlas::io::Exception(err.str(), Here());
    }
    const T* data = static_cast<const T*>(encoded.data());
    out.assign(data, data + array.size());
}

//---------------------------------------------------------------------------------------------------------------------

}  // end namespace std
