#include <koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp>

#include <koinos/state_db/backends/rocksdb/exceptions.hpp>

namespace koinos::state_db::backends::rocksdb {

rocksdb_iterator::rocksdb_iterator(
   std::shared_ptr< ::rocksdb::DB > db,
   std::shared_ptr< ::rocksdb::ColumnFamilyHandle > handle,
   std::shared_ptr< const ::rocksdb::ReadOptions > opts,
   std::shared_ptr< object_cache > cache
) :
   _db( db ),
   _handle( handle ),
   _opts( opts ),
   _cache( cache )
{}

rocksdb_iterator::rocksdb_iterator( const rocksdb_iterator& other ) :
   _db( other._db ),
   _handle( other._handle ),
   _opts( other._opts ),
   _cache( other._cache ),
   _cache_value( other._cache_value )
{
   if ( other._iter )
   {
      _iter.reset( _db->NewIterator( *_opts, &*_handle ) );

      if( other._iter->Valid() )
      {
         _iter->Seek( other._iter->key() );
      }
   }
}

rocksdb_iterator::~rocksdb_iterator() {}

const rocksdb_iterator::value_type& rocksdb_iterator::operator*() const
{
   KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );

   if ( !_cache_value )
   {
      update_cache_value();
   }

   return *_cache_value;
}

const rocksdb_iterator::key_type& rocksdb_iterator::key() const
{
   KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );

   if ( !_key )
   {
      update_cache_value();
   }

   return *_key;
}

abstract_iterator& rocksdb_iterator::operator++()
{
   KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );

   _iter->Next();
   KOINOS_ASSERT( _iter->status().ok(), iterator_exception, "iterator operation is invalid" );

   update_cache_value();

   return *this;
}

abstract_iterator& rocksdb_iterator::operator--()
{
   if ( !valid() )
   {
      _iter.reset( _db->NewIterator( *_opts, &*_handle ) );
      _iter->SeekToLast();
   }
   else
   {
      _iter->Prev();
      KOINOS_ASSERT( _iter->status().ok(), iterator_exception, "iterator operation is invalid" );
   }

   update_cache_value();

   return *this;
}

bool rocksdb_iterator::valid() const
{
   return _iter && _iter->Valid();
}

std::unique_ptr< abstract_iterator > rocksdb_iterator::copy() const
{
   return std::make_unique< rocksdb_iterator >( *this );
}

void rocksdb_iterator::update_cache_value() const
{
   if ( valid() )
   {
      auto key_slice = _iter->key();
      auto key = std::make_shared< std::string >( key_slice.data(), key_slice.size() );
      std::lock_guard< std::mutex > lock( _cache->get_mutex() );
      auto [cache_hit, ptr] = _cache->get( *key );

      KOINOS_ASSERT( cache_hit, rocksdb_internal_exception, "iterator erroneosly hit null value in cache" );

      if ( !ptr )
      {
         auto value_slice = _iter->value();
         ptr = _cache->put( *key, std::make_shared< const object_cache::value_type >( value_slice.data(), value_slice.size() ) );
      }

      _cache_value = ptr;
      _key = key;
   }
   else
   {
      _cache_value.reset();
      _key.reset();
   }
}

} // koinos::state_db::backends::rocksdb
