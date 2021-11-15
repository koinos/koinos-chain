#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/util/random.hpp>

namespace koinos::state_db::backends::rocksdb {

namespace constants {
   constexpr std::size_t cache_size = 64 << 20; // 64 MB
} // constants

rocksdb_backend::rocksdb_backend() :
   _cache( std::make_shared< object_cache >( constants::cache_size ) ),
   _ropts( std::make_shared< ::rocksdb::ReadOptions >() )
{
   ::rocksdb::Options options;
   options.create_if_missing = true;
   ::rocksdb::DB* db;

   auto temp = std::filesystem::temp_directory_path() / koinos::util::random_alphanumeric( 8 );
   std::filesystem::create_directory( temp );

   auto status = ::rocksdb::DB::Open( options, temp.string(), &db );
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
   auto status = _db->Put( _wopts, nullptr, ::rocksdb::Slice( k ), ::rocksdb::Slice( v ) );

   if ( status.ok() )
   {
      std::lock_guard lock( _cache->get_mutex() );
      _cache->put( ::rocksdb::Slice( k ), v );
   }

   return status.ok();
}

void rocksdb_backend::erase( const key_type& k )
{
   auto status = _db->Delete( _wopts, nullptr, ::rocksdb::Slice( k ) );

   KOINOS_ASSERT( status.ok(), koinos::exception, "" );

   std::lock_guard lock( _cache->get_mutex() );
   _cache->remove( ::rocksdb::Slice( k ) );
}

iterator rocksdb_backend::find( const key_type& k )
{
   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   auto itr_ptr = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   itr_ptr->Seek( ::rocksdb::Slice( k ) );

   if ( itr_ptr->Valid() )
   {
      auto key_slice = itr_ptr->key();

      if ( k.size() == key_slice.size()
         && memcmp( k.data(), key_slice.data(), k.size() ) == 0 )
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

   itr->_iter->Seek( ::rocksdb::Slice( k ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

} // koinos::state_db::backends::rocksdb
