#pragma once
#include <koinos/pack/rt/binary_fwd.hpp>
#include <koinos/pack/rt/exceptions.hpp>
#include <koinos/pack/rt/reflect.hpp>
#include <koinos/pack/rt/typename.hpp>
#include <koinos/pack/rt/util/variant_helpers.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/interprocess/streams/vectorstream.hpp>

#include <iostream>
#include <type_traits>
#include <utility>

#define KOINOS_DEFINE_NATIVE_INT_SERIALIZER( int_type )               \
template< typename Stream >                                           \
inline void to_binary( Stream& s, int_type t )                        \
{                                                                     \
   boost::endian::native_to_big_inplace( t );                         \
   s.write( (const char*)&t, sizeof(t) );                             \
}                                                                     \
template< typename Stream >                                           \
inline void from_binary( Stream& s, int_type& t, uint32_t depth )     \
{                                                                     \
   s.read( (char*)&t, sizeof(t) );                                    \
   KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );\
   boost::endian::big_to_native_inplace( t );                         \
}

#define KOINOS_DEFINE_BOOST_INT_SERIALIZER( int_type, hi_type, low_type, low_size ) \
template< typename Stream >                                                  \
inline void to_binary( Stream& s, const int_type& t )                        \
{                                                                            \
   to_binary( s, static_cast< hi_type >( t >> low_size ) );                  \
   to_binary( s, static_cast< low_type >( t ) );                             \
}                                                                            \
template< typename Stream >                                                  \
inline void from_binary( Stream& s, int_type& t, uint32_t depth )            \
{                                                                            \
   hi_type hi;                                                               \
   low_type low;                                                             \
   from_binary( s, hi, depth );                                              \
   from_binary( s, low, depth );                                             \
   t = hi;                                                                   \
   t <<= low_size;                                                           \
   t |= low;                                                                 \
}

namespace koinos::pack {

namespace detail {
   template< typename T >
   constexpr bool is_trivial_array = std::is_scalar< T >::value == true && std::is_pointer< T >::value == false;

   template< typename Stream, typename Itr >
   inline void pack_itr( Stream& s, Itr start, Itr end );
} // detail

/* Integer Types:
 *
 * Network Byte Order (Big-Endian) for the fixed length integer size.
 *
 * Signed/Unsigned types for 8, 16, 32, 64, 128, 160, and 256 bit length integers
 */

KOINOS_DEFINE_NATIVE_INT_SERIALIZER( int8_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( uint8_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( int16_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( uint16_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( int32_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( uint32_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( int64_t )
KOINOS_DEFINE_NATIVE_INT_SERIALIZER( uint64_t )

KOINOS_DEFINE_BOOST_INT_SERIALIZER( int128_t,  int64_t,   int64_t,   64 );
KOINOS_DEFINE_BOOST_INT_SERIALIZER( uint128_t, uint64_t,  uint64_t,  64 );
KOINOS_DEFINE_BOOST_INT_SERIALIZER( int160_t,  int32_t,   int128_t,  128 );
KOINOS_DEFINE_BOOST_INT_SERIALIZER( uint160_t, uint32_t,  uint128_t, 128 );
KOINOS_DEFINE_BOOST_INT_SERIALIZER( int256_t,  int128_t,  int128_t,  128 );
KOINOS_DEFINE_BOOST_INT_SERIALIZER( uint256_t, uint128_t, uint128_t, 128 );

/* Bool:
 *
 * An 8 bit integer. For simplicity, only 0x00 and 0x01 are valid values when a bool is explicitly specified.
 */

template< typename Stream >
inline void to_binary( Stream& s, bool b )
{
   uint8_t v = b ? uint8_t( 1 ) : uint8_t( 0 );
   to_binary( s, v );
}

template< typename Stream >
inline void from_binary( Stream& s, bool& b, uint32_t depth )
{
   uint8_t v;
   from_binary( s, v );
   KOINOS_ASSERT( !(v&0xFE), parse_error, "Bool value must only be 0 or 1", () );

   b = bool(v);
}

/* Varint:
 *
 * Varint's can hold a maximum of 64 bits of data (increased from 32 for BitShares/Steem).
 * The first bit of an octet signs if the varint continues or not (1 for continuation, 0 for stop).
 * To encode signed varints, a transfomation to zigzag encoding (used by protobufs) is first
 * applied `(n<<1)^(n>>64)` before encoding the now unsigned value as a varint. The max wire
 * size of a varint can be 80 bits total.
 *
 * This implementation is derived from Bitcoin.
 *
 * Copyright (c) 2009-2010 Satoshi Nakamoto
 * Copyright (c) 2009-2015 The Bitcoin Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

template< typename Stream >
inline void to_binary( Stream& s, const unsigned_int& v )
{
   char tmp[(sizeof(v.value)*8+6)/7];
   uint64_t n = v.value;
   uint32_t len = 0;
   while(true)
   {
      tmp[len] = (n & 0x7F) | (len ? 0x80 : 0x00);
      if (n <= 0x7F)
         break;
      n = (n >> 7);
      len++;
   }

   do
   {
      s.write( tmp + len, sizeof(char) );
   } while( len-- );
}

template< typename Stream >
inline void from_binary( Stream& s, unsigned_int& v,  uint32_t depth )
{
   v.value = 0;
   char chData = 0;

   do
   {
      s.get(chData);
      KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );
      v.value = (v.value << 7) | (chData & 0x7F);
   } while( chData & 0x80 );
}

template< typename Stream >
inline void to_binary( Stream& s, const signed_int& v )
{
   unsigned_int val = 0;
   val.value = uint64_t( (v.value << 1) ^ (v.value >> 63) );
   to_binary( s, val );
}

template< typename Stream >
inline void from_binary( Stream& s, signed_int& v, uint32_t depth )
{
   unsigned_int val;
   from_binary( s, val, depth );
   bool neg = val.value & 0x01;
   v.value = ((val.value >> 1) ^ (val.value >> 63)) + (val.value & 0x01);
   if( neg ) v.value = -v.value;
}

/* Vector< T >:
 *
 * A varint size of the vector, followed by that many objects in the vector using T's binary encoding.
 */

template< typename Stream, typename T >
inline void to_binary( Stream& s, const std::vector< T >& v )
{
   to_binary( s, unsigned_int( (uint64_t)v.size()) );
   detail::pack_itr( s, v.begin(), v.end() );
}

template< typename Stream, typename T >
inline void from_binary( Stream& s, std::vector< T >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );

   unsigned_int size;
   from_binary( s, size );
   KOINOS_ASSERT( size.value*sizeof(T) < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Vector allocation exceeded", () );

   v.clear();
   v.reserve( size.value );
   for( size_t i = 0; i < size.value; ++i )
   {
      T tmp;
      from_binary( s, tmp, depth );
      v.emplace_back( std::move( tmp ) );
   }
}

// Set< T > uses an identical serialization to Vector< T > but is implicitly in order and
// cannot contain duplicates.
/*
template< typename Stream, typename T, typename Compare >
inline void to_binary( Stream& s, const std::set< T, Compare >& v )
{
   to_binary( s, unsigned_int( v.size() ) );
   detail::pack_itr( s, v.begin(), v.end() );
}

template< typename Stream, typename T, typename Compare >
inline void from_binary( Stream& s, std::set< T, Compare >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );

   unsigned_int size;
   from_binary( s, size );
   KOINOS_ASSERT( size.value*sizeof(T) < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Vector allocation exceeded", () );

   v.clear();
   for( size_t i = 0; i < size.value; ++i )
   {
      T tmp;
      from_binary( s, tmp, depth );
      auto res = v.emplace( std::move( tmp ) );
      KOINOS_ASSERT( res.second, parse_error, "Duplicate value detected unpacking set", () );
   }
}
*/

// Variable Length Blob serializes identically to vector< char >

template< typename Stream >
inline void to_binary( Stream& s, const vl_blob& v )
{
   to_binary( s, unsigned_int( v.data.size() ) );
   s.write( v.data.data(), v.data.size() );
}

template< typename Stream >
inline void from_binary( Stream& s, vl_blob& v, uint32_t depth )
{
   unsigned_int size;
   from_binary( s, size );
   KOINOS_ASSERT( size.value < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Vector allocation exceeded", () );

   v.data.resize( size.value );
   s.read( v.data.data(), size.value );
   KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );

}

/* Array< T, N >:
 *
 * A fixed length container of N items serialized in T's binary encoding.
 *
 * Trivial array pack/unpack can be optimized for direct access.
 */

template< typename Stream, typename T, size_t N >
inline void to_binary( Stream& s, const std::array< T, N >& v )
{
   detail::pack_itr( s, v.begin(), v.end() );
}

template< typename Stream, typename T, size_t N >
inline void from_binary( Stream& s, std::array< T, N >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );
   static_assert( N*sizeof(T) < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, "Array allocation exceeded" );

   for( size_t i = 0; i < N; ++i )
   {
      from_binary( s, v.data()[i], depth );
   }
}

// Fixed Length Blob serializes identically to array< uint8_t, N >

template< typename Stream, size_t N >
inline void to_binary( Stream& s, const fl_blob< N >& v )
{
   s.write( v.data.data(), N );
}

template< typename Stream, size_t N >
inline void from_binary( Stream& s, fl_blob< N >& v, uint32_t depth )
{
   s.read( v.data.data(), N );
   KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );
}

/* Map< K, V >:
 *
 * A varint size of the map followed by (K, V) pairs serialized their binary format.
 */
/*
template< typename Stream, typename K, typename V >
inline void to_binary( Stream& s, const std::pair< K, V >& v )
{
   to_binary( s, v.first );
   to_binary( s, v.second );
}

template< typename Stream, typename K, typename V >
inline void from_binary( Stream& s, std::pair< K, V >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );
   from_binary( s, v.first );
   from_binary( s, v.second );
}

template< typename Stream, typename K, typename V >
inline void to_binary( Stream& s, const std::map< K, V >& v )
{
   to_binary( s, unsigned_int( (uint64_t)v.size()) );
   detail::pack_itr( s, v.begin(), v.end() );
}

template< typename Stream, typename K, typename V >
inline void from_binary( Stream& s, std::map< K, V >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );

   unsigned_int size;
   from_binary( s, size );
   KOINOS_ASSERT( size.value*sizeof(std::pair<K,V>) < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Vector allocation exceeded", () );

   v.clear();
   v.reserve( size.value );
   for( size_t i = 0; i < size.value; ++i )
   {
      std::pair<K,V> tmp;
      from_binary( s, tmp, depth );
      v.insert( std::move( tmp ) );
   }
}
*/

/* Variant< T >:
 *
 * A varint key, followed by the binary encoding of whatever object is held by that key.
 */
template< typename Stream, typename... T >
inline void to_binary( Stream& s, const std::variant< T... >& v )
{
   to_binary( s, unsigned_int( v.index() ) );
   std::visit( [&]( auto& arg ){ to_binary( s, arg ); }, v );
}

template< typename Stream, typename... T >
inline void from_binary( Stream& s, std::variant< T... >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );

   unsigned_int index;
   from_binary( s, index, depth );
   util::variant_helper< T... >::init_variant( v, index.value );
   std::visit( [&]( auto& arg ){ from_binary( s, arg, depth ); }, v );
}

/* Optional< T >:
 *
 * A bool specifying if the optional is valid. If true, it is followed by the binary
 * serialization of the contained type.
 */

template< typename Stream, typename T >
inline void to_binary( Stream& s, const std::optional< T >& v )
{
   to_binary( s, v.has_value() );
   if( v.has_value() ) to_binary( s, *v );
}

template< typename Stream, typename T >
inline void from_binary( Stream& s, std::optional< T >& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );

   bool b;
   from_binary( s, b, depth );
   if( b )
   {
      v = T();
      from_binary( s, *v, depth );
   }
}

/* Multihash:
 *
 * A varint hash function (key), followed by a varint digest size in bytes,
 * followed by Network Byte Order hash for the specified length.
 */

template< typename Stream >
inline void to_binary( Stream& s, const multihash_type& v )
{
   to_binary( s, unsigned_int( v.hash_id ) );
   to_binary( s, v.digest );
}

template< typename Stream >
inline void from_binary( Stream& s, multihash_type& v, uint32_t depth )
{
   unsigned_int id, size;
   from_binary( s, id );
   from_binary( s, size );

   KOINOS_ASSERT( size.value < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Array allocation exceeded" );

   v.digest.data.resize( size.value );
   if( size.value )
   {
      s.read( v.digest.data.data(), size.value );
      KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );
   }

   v.hash_id = id.value;
}

/* Multihash Vector:
 *
 * A varint hash function (key), followed by a varint digest size in bytes, followed by a varint
 * size of the vector, followed by that many digests each of the specified digest size.
 */

template< typename Stream >
inline void to_binary( Stream& s, const multihash_vector& v )
{
   size_t size = v.digests.size() ? v.digests[0].data.size() : 0;
   for( size_t i = 0; i < v.digests.size(); ++i )
   {
      KOINOS_ASSERT( v.digests[i].data.size() == size, parse_error, "Multihash vector digest size mismatch when packing" );
   }

   to_binary( s, unsigned_int( v.hash_id ) );
   to_binary( s, unsigned_int( size ) );
   to_binary( s, unsigned_int( v.digests.size() ) );
   for( size_t i = 0; i < v.digests.size(); ++i )
   {
      s.write( &(v.digests[i].data.front()), size );
   }
}

template< typename Stream >
inline void from_binary( Stream& s, multihash_vector& v, uint32_t depth )
{
   unsigned_int id, digest_size, num_digests;
   from_binary( s, id );
   from_binary( s, digest_size );
   from_binary( s, num_digests );

   KOINOS_ASSERT( uint128_t( digest_size.value ) * num_digests.value < KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE, allocation_violation, "Array allocation exceeded" );

   v.hash_id = id.value;
   v.digests.reserve( num_digests.value );

   for( size_t i = 0; i < num_digests.value; ++i )
   {
      v.digests.emplace_back( vl_blob() );
      v.digests[i].data.resize( digest_size.value );
      s.read( v.digests[i].data.data(), digest_size.value );
      KOINOS_ASSERT( s.good(), stream_error, "Error reading from stream" );
   }
}

namespace detail
{
   // Minimal vectorstream implementations for internal serialization to a vl_blob
   // Boost vectorstream has to own the underlying vector. This implementation utilizes
   // a reference for efficiency.
   class output_blobstream
   {
      private:
         vl_blob& _data;
         size_t   _write_pos = 0;

      public:
         output_blobstream( vl_blob& d ) : _data(d) {}

         output_blobstream& write( const char* s, size_t n )
         {
            _data.data.resize( _write_pos + n );

            memcpy( _data.data.data() + _write_pos, s, n );
            _write_pos += n;

            return *this;
         }
   };

   template< typename Blob >
   class input_blobstream
   {
      private:
         const Blob& _data;
         bool        _error = false;
         size_t      _read_pos = 0;

      public:
         input_blobstream( const Blob& d ) : _data(d) {}

         input_blobstream& read( char* s, size_t n )
         {
            _error = false;

            size_t to_read = std::min( n, _data.data.size() - _read_pos );
            if( to_read < n ) _error = true;

            memcpy( s, _data.data.data() + _read_pos, to_read );
            _read_pos += to_read;

            return *this;
         }

         input_blobstream& get( char& c )
         {
            _error = false;

            if( _read_pos < _data.data.size() )
            {
               c = _data.data[ _read_pos ];
               _read_pos++;
            }
            else
            {
               _error = true;
            }

            return *this;
         }

         bool good() const
         {
            return !_error;
         }
   };

   // Minimal stringstream wrapper implementations for internal serialization to a c string
   class output_stringstream
   {
      private:
         char*    _buffer;
         size_t   _length;
         size_t   _write_pos = 0;

      public:
         output_stringstream( char* c, size_t l ) : _buffer(c), _length(l) {}

         output_stringstream& write( const char* s, size_t n )
         {
            KOINOS_ASSERT( _write_pos + n <= _length, stream_error,
               "Buffer overflow when serializing to a c string.", () );

            memcpy( _buffer + _write_pos, s, n );
            _write_pos += n;

            return *this;
         }
   };

   class input_stringstream
   {
      private:
         const char* _buffer;
         size_t      _length;
         bool        _error = false;
         size_t      _read_pos = 0;

      public:
         input_stringstream( const char* c, size_t l ) : _buffer(c), _length(l) {}

         input_stringstream& read( char* s, size_t n )
         {
            _error = false;

            size_t to_read = std::min( n, _length - _read_pos );
            if( to_read < n ) _error = true;

            memcpy( s, _buffer + _read_pos, to_read );
            _read_pos += to_read;

            return *this;
         }

         input_stringstream& get( char& c )
         {
            _error = false;

            if( _read_pos < _length )
            {
               c = _buffer[ _read_pos ];
               _read_pos++;
            }
            else
            {
               _error = true;
            }

            return *this;
         }

         bool good() const
         {
            return !_error;
         }
   };

   template< typename Stream, typename Itr >
   inline void pack_itr( Stream& s, Itr start, Itr end )
   {
      while( start != end )
      {
         to_binary( s, *start );
         ++start;
      }
   }

   namespace binary {

      template< typename T > std::true_type is_class_helper( void(T::*)() );
      template< typename T > std::false_type is_class_helper( ... );

      template<typename T>
      struct is_class { typedef decltype( is_class_helper<T>(0) ) type; enum value_enum { value = type::value }; };

      template< typename Stream, typename Class >
      struct to_binary_object_visitor
      {
         to_binary_object_visitor( const Class& _c, Stream& _s ) :
            c(_c),
            s(_s)
         {}

         template< typename T, typename C, T(C::*p) >
         void operator()( const char* name )const
         {
            to_binary( s, c.*p );
         }

         private:
            const Class& c;
            Stream&      s;
      };

      template< typename Stream, typename Class >
      struct from_binary_object_visitor
      {
         from_binary_object_visitor( Class& _c, Stream& _s ) :
            c(_c),
            s(_s)
         {}

         template< typename T, typename C, T(C::*p) >
         inline void operator()( const char* name )const
         {
            from_binary( s, c.*p );
         }

         private:
            Class&  c;
            Stream& s;
      };

      // if_enum is only called if type is reflected
      template< typename IsEnum = std::false_type >
      struct if_enum
      {
         template< typename Stream, typename T >
         static inline void to_binary( Stream& s, const T& v )
         {
            reflector< T >::visit( to_binary_object_visitor< Stream, T >( v, s ) );
         }

         template< typename Stream, typename T >
         static inline void from_binary( Stream& s, T& v, uint32_t )
         {
            reflector< T >::visit( from_binary_object_visitor< Stream, T >( v, s ) );
         }
      };

      template<>
      struct if_enum< std::true_type >
      {
         template< typename Stream, typename T >
         static inline void to_binary( Stream& s, const T& v )
         {
            to_binary( s, (int64_t)v );
         }

         template< typename Stream, typename T >
         static inline void from_binary( Stream& s, T& v, uint32_t depth )
         {
            depth++;
            KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );
            int64_t temp;
            from_binary(s, temp, depth);
            v = (T)temp;
         }
      };

} } // detail::binary

template< typename Stream, typename T >
inline void to_binary( Stream& s, const T& v )
{
   detail::binary::if_enum< typename reflector< T >::is_enum >::to_binary( s, v );
}

template< typename Stream, typename T >
inline void from_binary( Stream& s, T& v, uint32_t depth )
{
   depth++;
   KOINOS_ASSERT( depth <= KOINOS_PACK_MAX_RECURSION_DEPTH, depth_violation, "Unpack depth exceeded", () );
   detail::binary::if_enum< typename reflector< T >::is_enum >::from_binary( s, v, depth );
}

inline void to_vl_blob( vl_blob& v, const std::string& s )
{
   v.data.clear();
   v.data.insert( v.data.end(), s.begin(), s.end() );
}

inline void to_vl_blob( vl_blob& v, const std::string&& s )
{
   to_vl_blob( v, s );
}

inline vl_blob to_vl_blob( const std::string& s )
{
   vl_blob v;
   to_vl_blob( v, s );
   return v;
}

inline vl_blob to_vl_blob( const std::string&& s )
{
   vl_blob v;
   to_vl_blob( v, s );
   return v;
}

template< typename T >
inline void to_vl_blob( vl_blob& v, const T& t )
{
   v.data.clear();
   detail::output_blobstream vs( v );
   to_binary( vs, t );
}

template< typename T >
inline vl_blob to_vl_blob( const T& t )
{
   vl_blob v;
   to_vlob( v, t );
   return v;
}

inline void from_vl_blob( const vl_blob& v, std::string& s )
{
   s = std::string( v.data.data(), v.data.size() );
}

template< typename T >
inline void from_vl_blob( const vl_blob& v, T& t )
{
   detail::input_blobstream< vl_blob > vs( v );
   from_binary( vs, t );
}

template< typename T >
inline T from_vl_blob( const vl_blob& v )
{
   T t;
   from_vl_blob( v, t );
   return t;
}

template<>
inline std::string from_vl_blob< std::string >( const vl_blob& v )
{
   std::string s;
   from_vl_blob( v, s );
   return s;
}

template< typename T, size_t N >
inline void from_fl_blob( const fl_blob< N >& f, T& t )
{
   detail::input_blobstream< fl_blob< N > > bs( f );
   from_binary( bs, t );
}

template< typename T, size_t N >
inline T from_fl_blob( const fl_blob< N >& f )
{
   T t;
   from_fl_blob( f, t );
   return t;
}

template< typename T >
inline void to_c_str( char* c, size_t l, const T& t )
{
   detail::output_stringstream ss( c, l );
   to_binary( ss, t );
}

template< typename T >
inline void from_c_str( const char* c, size_t l, T& t )
{
   detail::input_stringstream ss( c, l );
   from_binary( ss, t );
}

template< typename T >
inline T from_c_str( const char* c, size_t l )
{
   T t;
   from_c_str( c, l, t );
   return t;
}

} // koinos::pack

#undef KOINOS_NATIVE_INT_SERIALIZER
#undef KOINOS_BOOST_INT_SERIALIZER
