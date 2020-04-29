#pragma once

#include <boost/core/demangle.hpp>

#include <cstdint>
#include <cstdlib>

#include <iostream>

namespace chainbase
{

/**
*  Object ID type that includes the type of the object it references
*/
template<typename T>
class oid
{
public:
   oid( int64_t i = 0 ):_id(i){}

   oid& operator++() { ++_id; return *this; }

   //operator size_t () const
   //{
   //   return _id;
   //}

   friend bool operator < ( const oid& a, const oid& b ) { return a._id < b._id; }
   friend bool operator > ( const oid& a, const oid& b ) { return a._id > b._id; }
   friend bool operator == ( const oid& a, const oid& b ) { return a._id == b._id; }
   friend bool operator != ( const oid& a, const oid& b ) { return a._id != b._id; }
   friend std::ostream& operator<<(std::ostream& s, const oid& id) {
            s << boost::core::demangle(typeid(oid<T>).name()) << '(' << id._id << ')'; return s;
         }

   int64_t _id = 0;
};

} /// namespace chainbase
