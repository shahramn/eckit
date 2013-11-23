/*
 * (C) Copyright 1996-2013 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @file Exp.h
/// @author Baudouin Raoult
/// @author Tiago Quintino
/// @date November 2013

#ifndef eckit_maths_Exp_h
#define eckit_maths_Exp_h

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include "eckit/eckit.h"

/// @todo look into currying / binding -- add2 = curry ( add , 2 )
/// @todo list --  list 1 2
/// @todo map
/// @todo fold
/// @todo comparison operator Equal()(v,2) -- returns what?
/// @todo unary operator
/// @todo operator returning scalar
/// @todo operator returning multiple outputs
/// @todo how to support multiple implementations ( MKL, CuBLAS, etc. )
/// @todo create a expression tree Visitor class that takes an operation as parameter

//--------------------------------------------------------------------------------------------

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include <map>
#include <utility>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "eckit/exception/Exceptions.h"

//--------------------------------------------------------------------------------------------

namespace eckit_test { // For unit tests....
    class TestExp;
}

namespace eckit {
namespace maths {

//--------------------------------------------------------------------------------------------

#if 0
#define DBG     std::cout << Here() << std::endl;
#define DBGX(x) std::cout << Here() << " " << #x << " -> " << x << std::endl;
#else
#define DBG
#define DBGX(x)
#endif

//--------------------------------------------------------------------------------------------

class Value;
class List;
class Expr;
class Context;

typedef double scalar_t;

typedef boost::shared_ptr<Value>      ValPtr;
typedef boost::shared_ptr<List>       ListPtr;
typedef boost::shared_ptr<Expr> ExpPtr;

typedef std::vector< ExpPtr > args_t;


//--------------------------------------------------------------------------------------------

/// Expr Error
class Error: public eckit::Exception {
public:
    Error(const eckit::CodeLocation& loc, const string& s) : Exception( s, loc ) {}
};

//--------------------------------------------------------------------------------------------


class Expr :
    public boost::enable_shared_from_this<Expr>,
    private boost::noncopyable {

public: // methods

    static std::string class_name() { return "Exp"; }

    /// Empty contructor is usually used by derived classes that
    /// handle the setup of the parameters themselves
    Expr();

    /// Contructor taking a list of parameters
    /// handle the setup of the parameters themselves
    Expr( const args_t& args );

    virtual ~Expr();

    ExpPtr self() { return shared_from_this(); }

    template< typename T >
    boost::shared_ptr<T> as()
    {
        return boost::dynamic_pointer_cast<T,Expr>( shared_from_this() );
    }

    friend std::ostream& operator<<( std::ostream& os, const Expr& v)
    {
        v.print(os);
        return os;
    }

    ValPtr eval();
    ValPtr eval( ExpPtr );
    ValPtr eval( ExpPtr, ExpPtr );
    ValPtr eval( const args_t& );
    ValPtr eval( Context& );

    size_t arity() const { return args_.size(); }

    ExpPtr param( size_t i) const;
    ExpPtr param( size_t i, Context& ctx ) const;

    void param(size_t i, ExpPtr p );

    std::string str() const;

public: // virtual methods

    virtual std::string type_name() const = 0;
    virtual ExpPtr clone() = 0;
    virtual std::string signature() const = 0;
    virtual std::string ret_signature() const = 0;

    virtual ExpPtr optimise() = 0;


protected: // members



    args_t args_;     ///< parameters of this expression

private:

    virtual void print( std::ostream& ) const = 0;
    virtual ValPtr evaluate( Context& ) = 0;

    // For unit tests....
    friend class eckit_test::TestExp;

};


//--------------------------------------------------------------------------------------------

} // namespace maths
} // namespace eckit

#endif
