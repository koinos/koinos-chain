#include <koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp>

#include <koinos/exception.hpp>

namespace koinos::state_db::backends::rocksdb {

rocksdb_iterator::rocksdb_iterator( std::shared_ptr< ::rocksdb::DB > db, std::shared_ptr< const ::rocksdb::ReadOptions > opts, std::shared_ptr< object_cache > cache ) :
   _db( db ),
   _opts( opts ),
   _cache( cache )
   {}

rocksdb_iterator::rocksdb_iterator( const rocksdb_iterator& other ) :
   _db( other._db ),
   _opts( other._opts ),
   _cache( other._cache ),
   _cache_value( other._cache_value )
{
   if ( other._iter )
   {
      _iter.reset( _db->NewIterator( *_opts ) );

      if( other._iter->Valid() )
      {
         _iter->Seek( other._iter->key() );
      }
   }
}

rocksdb_iterator::~rocksdb_iterator() {}

const rocksdb_iterator::value_type& rocksdb_iterator::operator*()const
{
   KOINOS_ASSERT( valid(), koinos::exception, "" );

   if ( !_cache_value )
   {
      const_cast< rocksdb_iterator* >( this )->update_cache_value();
   }

   return *_cache_value;
}

abstract_iterator& rocksdb_iterator::operator++()
{
   KOINOS_ASSERT( valid(), koinos::exception, "" );

   _iter->Next();
   KOINOS_ASSERT( _iter->status().ok(), koinos::exception, "" );

   update_cache_value();

   return *this;
}

abstract_iterator& rocksdb_iterator::operator--()
{
   if ( !valid() )
   {
      _iter.reset( _db->NewIterator( *_opts ) );
      _iter->SeekToLast();
   }
   else
   {
      _iter->Prev();
   }

   update_cache_value();

   return *this;
}

bool rocksdb_iterator::valid()const
{
   return _iter && _iter->Valid();
}

std::unique_ptr< abstract_iterator > rocksdb_iterator::copy()const
{
   return std::make_unique< rocksdb_iterator >( *this );
}

void rocksdb_iterator::update_cache_value()
{
   auto key_slice = _iter->key();
   std::string key( key_slice.data(), key_slice.size() );
   std::lock_guard< std::mutex > lock( _cache->get_mutex() );
   auto ptr = _cache->get( key );

   if ( !ptr )
   {
      auto value_slice = _iter->value();
      ptr = _cache->put( key, std::string( value_slice.data(), value_slice.size() ) );
   }

   _cache_value = ptr;
}

} // koinos::state_db::backends::rocksdb
