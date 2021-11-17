#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/random.hpp>

#include <rocksdb/convenience.h>

namespace koinos::state_db::backends::rocksdb {

namespace constants {
   constexpr std::size_t cache_size = 64 << 20; // 64 MB
   constexpr std::size_t max_open_files = 64;

   const std::string objects_column_name       = "objects";
   constexpr std::size_t objects_column_index  = 0;
   const std::string metadata_column_name      = "metadata";
   constexpr std::size_t metadata_column_index = 1;

   const std::string size_key     = "size";
   const std::string revision_key = "revision";
} // constants

rocksdb_backend::rocksdb_backend() :
   _cache( std::make_shared< object_cache >( constants::cache_size ) ),
   _ropts( std::make_shared< ::rocksdb::ReadOptions >() )
{}

rocksdb_backend::~rocksdb_backend()
{
   close();
}

bool rocksdb_backend::maybe_create_columns( const std::filesystem::path& p )
{
   std::vector< ::rocksdb::ColumnFamilyDescriptor > column_definitions;
   column_definitions.emplace_back(
      constants::objects_column_name,
      ::rocksdb::ColumnFamilyOptions() );
   column_definitions.emplace_back(
      constants::metadata_column_name,
      ::rocksdb::ColumnFamilyOptions() );

   ::rocksdb::DB* db = nullptr;
   std::vector< ::rocksdb::ColumnFamilyHandle* > handles;
   ::rocksdb::Options options;

   auto status = ::rocksdb::DB::OpenForReadOnly( options, p.string(), column_definitions, &handles, &db );

   if ( status.ok() )
   {
      for ( auto* h : handles )
         delete h;
      delete db;
      return true;
   }

   handles.clear();
   options.create_if_missing = true;

   status = ::rocksdb::DB::Open( options, p.string(), &db );

   if ( !status.ok() )
      return false;

   std::shared_ptr< ::rocksdb::DB > db_ptr( db );

   status = db->CreateColumnFamilies( column_definitions, &handles );
   std::vector< std::shared_ptr< ::rocksdb::ColumnFamilyHandle > > handle_ptrs;
   for( auto* h : handles )
   {
      handle_ptrs.emplace_back( h );
   }

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db->Put(
      _wopts,
      handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::size_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( uint64_t( 0 ) ) )
   );

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db->Put(
      _wopts,
      handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::revision_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( uint64_t( 0 ) ) )
   );

   handle_ptrs.clear();
   db_ptr.reset();

   if ( !status.ok() )
      return false;

   return true;
}

void rocksdb_backend::open( const std::filesystem::path& p )
{
   KOINOS_ASSERT( p.is_absolute(), koinos::exception, "" );

   KOINOS_ASSERT( maybe_create_columns( p ), koinos::exception, "" );

   std::vector< ::rocksdb::ColumnFamilyDescriptor > column_definitions;
   column_definitions.emplace_back(
      constants::objects_column_name,
      ::rocksdb::ColumnFamilyOptions() );
   column_definitions.emplace_back(
      constants::metadata_column_name,
      ::rocksdb::ColumnFamilyOptions() );

   std::vector< ::rocksdb::ColumnFamilyHandle* > handles;

   ::rocksdb::Options options;
   options.create_if_missing = true;
   options.max_open_files = constants::max_open_files;
   ::rocksdb::DB* db;

   std::filesystem::create_directory( p );
   auto status = ::rocksdb::DB::Open( options, p.string(), column_definitions, &handles, &db );

   KOINOS_ASSERT( status.ok(), koinos::exception, "" );

   _db = std::shared_ptr< ::rocksdb::DB >( db );

   for ( auto* h : handles )
      _handles.emplace_back( h );
}

void rocksdb_backend::close()
{
   if ( _db )
   {
      auto status = _db->Put(
         _wopts,
         _handles[ constants::metadata_column_index ],
         ::rocksdb::Slice( constants::size_key ),
         ::rocksdb::Slice( util::converter::as< std::string >( uint64_t( 0 ) ) )
      );

      KOINOS_ASSERT( status.ok(), koinos::exception, "" );

      status = _db->Put(
         _wopts,
         _handles[ constants::metadata_column_index ],
         ::rocksdb::Slice( constants::size_key ),
         ::rocksdb::Slice( util::converter::as< std::string >( uint64_t( 0 ) ) )
      );

      KOINOS_ASSERT( status.ok(), koinos::exception, "" );

      ::rocksdb::CancelAllBackgroundWork( &(*_db), true );
      _handles.clear();
      _db.reset();
      _cache->clear();
   }
}

rocksdb_backend::size_type rocksdb_backend::revision()const
{
   return _revision;
}

void rocksdb_backend::set_revision( size_type rev )
{
   _revision = rev;

}

iterator rocksdb_backend::begin()
{
   KOINOS_ASSERT( _db, koinos::exception, "" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );
   itr->_iter->SeekToFirst();

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::end()
{
   KOINOS_ASSERT( _db, koinos::exception, "" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

void rocksdb_backend::put( const key_type& k, const value_type& v )
{
   KOINOS_ASSERT( _db, koinos::exception, "" );
   bool exists = find( k ) != end();

   auto status = _db->Put(
      _wopts,
      &(*_handles[ constants::objects_column_index ]),
      ::rocksdb::Slice( k ),
      ::rocksdb::Slice( v ) );

   KOINOS_ASSERT( status.ok(), koinos::exception, "" );

   if ( !exists )
   {
      _size++;
   }

   std::lock_guard lock( _cache->get_mutex() );
   _cache->put( ::rocksdb::Slice( k ), v );
}

void rocksdb_backend::erase( const key_type& k )
{
   KOINOS_ASSERT( _db, koinos::exception, "" );

   bool exists = find( k ) != end();
   auto status = _db->Delete( _wopts, &(*_handles[ constants::objects_column_index ]), ::rocksdb::Slice( k ) );

   KOINOS_ASSERT( status.ok(), koinos::exception, "" );

   if ( exists )
   {
      _size--;
   }

   std::lock_guard lock( _cache->get_mutex() );
   _cache->remove( ::rocksdb::Slice( k ) );
}

void rocksdb_backend::clear()
{
   KOINOS_ASSERT( _db, koinos::exception, "" );

   for ( auto h : _handles )
   {
      _db->DropColumnFamily( &*h );
   }

   _cache->clear();
}

rocksdb_backend::size_type rocksdb_backend::size()const
{
   return _size;
}

iterator rocksdb_backend::find( const key_type& k )
{
   KOINOS_ASSERT( _db, koinos::exception, "" );

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
   KOINOS_ASSERT( _db, koinos::exception, "" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts ) );

   itr->_iter->Seek( ::rocksdb::Slice( k ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

} // koinos::state_db::backends::rocksdb
