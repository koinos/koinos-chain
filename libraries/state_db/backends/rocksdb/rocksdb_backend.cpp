#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>

#include <koinos/log.hpp>

namespace koinos::state_db::backends::rocksdb {

namespace constants {
   constexpr std::size_t cache_size = 64 << 20; // 64 MB
} // constants

rocksdb_backend::rocksdb_backend() : _cache( std::make_shared< object_cache >( constants::cache_size ) )
{
   ::rocksdb::Options options;
   options.create_if_missing = true;
   ::rocksdb::DB* db;
   auto status = ::rocksdb::DB::Open( options, "/tmp/testdb", &db );
   if ( !status.ok() )
      LOG(error) << status.ToString();

   _db = std::shared_ptr< ::rocksdb::DB >( db );
}

rocksdb_backend::~rocksdb_backend() {}

iterator rocksdb_backend::begin()
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );
   itr->_iter->SeekToFirst();

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::end()
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

bool rocksdb_backend::put( const key_type& k, const value_type& v )
{
   ::rocksdb::Slice slices[2];
   slices[0] = ::rocksdb::Slice( k.first );
   slices[1] = ::rocksdb::Slice( k.second );

   ::rocksdb::SliceParts parts( slices, sizeof(slices) );
   std::string slice_buffer;
   ::rocksdb::Slice key_slice( parts, &slice_buffer );

   auto status = _db->Put( _wopts, nullptr, key_slice, ::rocksdb::Slice( v ) );

   if ( status.ok() )
   {
      std::lock_guard lock( _cache->get_mutex() );
      _cache->put( key_slice, v );
   }

   return status.ok();
}

bool rocksdb_backend::erase( const key_type& k )
{
   ::rocksdb::Slice slices[2];
   slices[0] = ::rocksdb::Slice( k.first );
   slices[1] = ::rocksdb::Slice( k.second );

   ::rocksdb::SliceParts parts( slices, sizeof(slices) );
   std::string slice_buffer;
   ::rocksdb::Slice key_slice( parts, &slice_buffer );

   auto status = _db->Delete( _wopts, nullptr, key_slice );

   if ( status.ok() )
   {
      std::lock_guard lock( _cache->get_mutex() );
      _cache->remove( key_slice );
   }

   return status.ok();
}

iterator rocksdb_backend::find( const key_type& k )
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   auto itr_ptr = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   ::rocksdb::Slice slices[2];
   slices[0] = ::rocksdb::Slice( k.first );
   slices[1] = ::rocksdb::Slice( k.second );

   ::rocksdb::SliceParts parts( slices, sizeof(slices) );
   std::string slice_buffer;
   ::rocksdb::Slice key_slice( parts, &slice_buffer );

   itr_ptr->Seek( key_slice );

   if ( itr_ptr->Valid() )
   {
      auto found_key_slice = itr_ptr->key();

      if ( key_slice.size() == found_key_slice.size()
         && memcmp( key_slice.data(), found_key_slice.data(), key_slice.size() ) == 0 )
      {
         itr->_iter = std::move( itr_ptr );
      }
   }

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::lower_bound( const key_type& k )
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   ::rocksdb::Slice slices[2];
   slices[0] = ::rocksdb::Slice( k.first );
   slices[1] = ::rocksdb::Slice( k.second );

   ::rocksdb::SliceParts parts( slices, sizeof(slices) );
   std::string slice_buffer;
   ::rocksdb::Slice key_slice( parts, &slice_buffer );

   itr->_iter->Seek( key_slice );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::upper_bound( const key_type& k )
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   ::rocksdb::Slice slices[2];
   slices[0] = ::rocksdb::Slice( k.first );
   slices[1] = ::rocksdb::Slice( k.second );

   ::rocksdb::SliceParts parts( slices, sizeof(slices) );
   std::string slice_buffer;
   ::rocksdb::Slice key_slice( parts, &slice_buffer );

   itr->_iter->SeekForPrev( key_slice );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

} // koinos::state_db::backends::rocksdb
