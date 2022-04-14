#include <koinos/state_db/backends/rocksdb/object_cache.hpp>

#include <cassert>

namespace koinos::state_db::backends::rocksdb {

object_cache::object_cache( std::size_t size ) : _cache_max_size( size ) {}

object_cache::~object_cache() {}

std::shared_ptr< const object_cache::value_type > object_cache::get( const key_type& k )
{
   auto itr = _object_map.find( k );
   if ( itr == _object_map.end() )
      return std::shared_ptr< value_type >();

   // Erase the entry from the list and push front
   _lru_list.erase( itr->second.second );
   _lru_list.push_front( k );
   auto val = itr->second.first;

   _object_map[ k ] = std::make_pair( val, _lru_list.begin() );

   assert( _object_map.size() == _lru_list.size() );

   return val;
}

std::shared_ptr< const object_cache::value_type > object_cache::put( const key_type& k, const value_type& v )
{
   remove( k );

   // If the cache is full, remove the last entry from the map and pop back
   while ( _cache_size + v.size() > _cache_max_size )
      remove( _lru_list.back() );

   auto val_ptr = std::make_shared< const value_type >( v );
   _lru_list.push_front( k );
   _object_map[ k ] = std::make_pair( val_ptr, _lru_list.begin() );
   _cache_size += v.size();

   assert( _object_map.size() == _lru_list.size() );

   return val_ptr;
}

void object_cache::remove( const key_type& k )
{
   auto itr = _object_map.find( k );
   if ( itr != _object_map.end() )
   {
      _cache_size -= itr->second.first->size();
      _lru_list.erase( itr->second.second );
      _object_map.erase( itr );
   }

   assert( _object_map.size() == _lru_list.size() );
}

void object_cache::remove( const ::rocksdb::Slice& k )
{
   remove( std::string( k.data(), k.size() ) );
}

void object_cache::clear()
{
   _object_map.clear();
   _lru_list.clear();
}

std::mutex& object_cache::get_mutex()
{
   return _mutex;
}

} // koinos::state_db::backends::rocksdb
