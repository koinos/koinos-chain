#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>

#include <koinos/state_db/backends/rocksdb/exceptions.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/random.hpp>

#include <rocksdb/convenience.h>
#include <rocksdb/filter_policy.h>

namespace koinos::state_db::backends::rocksdb {

namespace constants {
   constexpr std::size_t cache_size = 64 << 20; // 64 MB
   constexpr std::size_t max_open_files = 64;

   constexpr std::size_t default_column_index  = 0;
   const std::string objects_column_name = "objects";
   constexpr std::size_t objects_column_index  = 1;
   const std::string metadata_column_name = "metadata";
   constexpr std::size_t metadata_column_index = 2;

   const std::string size_key = "size";
   const std::string revision_key = "revision";
   const std::string id_key = "id";
   const std::string merkle_root_key = "merkle_root";
   const std::string block_header_key = "block_header";

   constexpr rocksdb_backend::size_type size_default = 0;
   constexpr rocksdb_backend::size_type revision_default = 0;
   const crypto::multihash id_default = crypto::multihash::zero( crypto::multicodec::sha2_256 );
   const crypto::multihash merkle_root_default = crypto::multihash::zero( crypto::multicodec::sha2_256 );
   const protocol::block_header block_header_default = protocol::block_header();
} // constants

bool setup_database( const std::filesystem::path& p )
{
   std::vector< ::rocksdb::ColumnFamilyDescriptor > defs;
   defs.emplace_back(
      constants::objects_column_name,
      ::rocksdb::ColumnFamilyOptions() );
   defs.emplace_back(
      constants::metadata_column_name,
      ::rocksdb::ColumnFamilyOptions() );

   ::rocksdb::Options options;
   options.create_if_missing = true;

   ::rocksdb::DB* db;
   auto status = ::rocksdb::DB::Open( options, p.string(), &db );

   KOINOS_ASSERT( status.ok(), rocksdb_open_exception, "unable to open rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   auto db_ptr = std::shared_ptr< ::rocksdb::DB >( db );

   std::vector< ::rocksdb::ColumnFamilyHandle* > handles;
   status = db->CreateColumnFamilies( defs, &handles );

   if ( !status.ok() )
   {
      return false;
   }

   std::vector< std::shared_ptr< ::rocksdb::ColumnFamilyHandle > > handle_ptrs;

   for ( auto* h : handles )
      handle_ptrs.emplace_back( h );

   ::rocksdb::WriteOptions wopts;

   status = db_ptr->Put(
      wopts,
      &*handle_ptrs[ 1 ],
      ::rocksdb::Slice( constants::size_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( constants::size_default ) )
   );

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db_ptr->Put(
      wopts,
      &*handle_ptrs[ 1 ],
      ::rocksdb::Slice( constants::revision_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( constants::revision_default ) )
   );

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db_ptr->Put(
      wopts,
      &*handle_ptrs[ 1 ],
      ::rocksdb::Slice( constants::id_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( constants::id_default ) )
   );

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db_ptr->Put(
      wopts,
      &*handle_ptrs[ 1 ],
      ::rocksdb::Slice( constants::merkle_root_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( constants::merkle_root_default ) )
   );

   if ( !status.ok() )
   {
      handle_ptrs.clear();
      db_ptr.reset();
      return false;
   }

   status = db_ptr->Put(
      wopts,
      &*handle_ptrs[ 1 ],
      ::rocksdb::Slice( constants::block_header_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( constants::block_header_default ) )
   );

   handle_ptrs.clear();
   db_ptr.reset();

   return status.ok();
}

rocksdb_backend::rocksdb_backend() :
   _cache( std::make_shared< object_cache >( constants::cache_size ) ),
   _ropts( std::make_shared< ::rocksdb::ReadOptions >() )
{}

rocksdb_backend::~rocksdb_backend()
{
   close();
}

void rocksdb_backend::open( const std::filesystem::path& p )
{
   KOINOS_ASSERT( p.is_absolute(), rocksdb_open_exception, "path must be absolute, ${p}", ("p", p.string()) );
   KOINOS_ASSERT( std::filesystem::exists( p ), rocksdb_open_exception, "path does not exist, ${p}", ("p", p.string()) );

   std::vector< ::rocksdb::ColumnFamilyDescriptor > defs;
   defs.emplace_back(
      ::rocksdb::kDefaultColumnFamilyName,
      ::rocksdb::ColumnFamilyOptions() );
   defs.emplace_back(
      constants::objects_column_name,
      ::rocksdb::ColumnFamilyOptions() );
   defs.emplace_back(
      constants::metadata_column_name,
      ::rocksdb::ColumnFamilyOptions() );

   std::vector< ::rocksdb::ColumnFamilyHandle* > handles;

   ::rocksdb::Options options;
   options.max_open_files = constants::max_open_files;
   ::rocksdb::DB* db;

   auto status = ::rocksdb::DB::Open( options, p.string(), defs, &handles, &db );

   if ( !status.ok() )
   {
      KOINOS_ASSERT( setup_database( p ), rocksdb_setup_exception, "unable to configure rocksdb database" );

      status = ::rocksdb::DB::Open( options, p.string(), defs, &handles, &db );
      KOINOS_ASSERT( status.ok(), rocksdb_open_exception, "unable to open rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
   }

   _db = std::shared_ptr< ::rocksdb::DB >( db );

   for ( auto* h : handles )
      _handles.emplace_back( h );

   try
   {
      load_metadata();
   }
   catch ( ... )
   {
      _handles.clear();
      _db.reset();
      throw;
   }
}

void rocksdb_backend::close()
{
   if ( _db )
   {
      store_metadata();
      flush();

      ::rocksdb::CancelAllBackgroundWork( &*_db, true );
      _handles.clear();
      _db.reset();
      std::lock_guard lock( _cache->get_mutex() );
      _cache->clear();
   }
}

void rocksdb_backend::flush()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   static const ::rocksdb::FlushOptions flush_options;

   _db->Flush( flush_options, &*_handles[ constants::objects_column_index ] );
   _db->Flush( flush_options, &*_handles[ constants::metadata_column_index ] );
}

void rocksdb_backend::start_write_batch()
{
   KOINOS_ASSERT( !_write_batch, rocksdb_session_in_progress, "session already in progress" );
   _write_batch.emplace();
}

void rocksdb_backend::end_write_batch()
{
   if ( _write_batch )
   {
      auto status = _db->Write( _wopts, &*_write_batch );
      KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write session to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
      _write_batch.reset();
   }
}

rocksdb_backend::size_type rocksdb_backend::revision() const
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   return _revision;
}

void rocksdb_backend::set_revision( size_type rev )
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   _revision = rev;
}

const crypto::multihash& rocksdb_backend::id() const
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   return _id;
}

void rocksdb_backend::set_id( const crypto::multihash& id )
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   _id = id;
}

const crypto::multihash& rocksdb_backend::merkle_root() const
{
   return _merkle_root;
}

void rocksdb_backend::set_merkle_root( const crypto::multihash& merkle_root )
{
   _merkle_root = merkle_root;
}

iterator rocksdb_backend::begin()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );
   itr->_iter->SeekToFirst();

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::end()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

void rocksdb_backend::put( const key_type& k, const value_type& v )
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );
   bool exists = get( k );

   ::rocksdb::Status status;

   if ( _write_batch )
   {
      status = _write_batch->Put(
         &*_handles[ constants::objects_column_index ],
         ::rocksdb::Slice( k ),
         ::rocksdb::Slice( v ) );
   }
   else
   {
      status = _db->Put(
         _wopts,
         &*_handles[ constants::objects_column_index ],
         ::rocksdb::Slice( k ),
         ::rocksdb::Slice( v ) );
   }

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   if ( !exists )
   {
      _size++;
   }

   std::lock_guard lock( _cache->get_mutex() );
   _cache->put( k, v );
}

const rocksdb_backend::value_type* rocksdb_backend::get( const key_type& k ) const
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   std::lock_guard lock( _cache->get_mutex() );
   auto ptr = _cache->get( k );
   if ( ptr )
   {
      return &*ptr;
   }

   value_type value;
   auto status = _db->Get(
      *_ropts,
      &*_handles[ constants::objects_column_index ],
      ::rocksdb::Slice( k ),
      &value
   );

   if ( status.ok() )
   {
      return &*_cache->put( k, value );
   }

   return nullptr;
}

void rocksdb_backend::erase( const key_type& k )
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   bool exists = get( k );
   auto status = _db->Delete(
      _wopts,
      &*_handles[ constants::objects_column_index ],
      ::rocksdb::Slice( k ) );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   if ( exists )
   {
      _size--;
   }

   std::lock_guard lock( _cache->get_mutex() );
   _cache->remove( ::rocksdb::Slice( k ) );
}

void rocksdb_backend::clear()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   for ( auto h : _handles )
   {
      _db->DropColumnFamily( &*h );
   }

   _handles.clear();
   _db.reset();
   std::lock_guard lock( _cache->get_mutex() );
   _cache->clear();
}

rocksdb_backend::size_type rocksdb_backend::size() const
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   return _size;
}

iterator rocksdb_backend::find( const key_type& k )
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
   auto itr_ptr = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

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
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   auto itr = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
   itr->_iter = std::unique_ptr< ::rocksdb::Iterator >( _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

   itr->_iter->Seek( ::rocksdb::Slice( k ) );

   return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

const protocol::block_header& rocksdb_backend::block_header() const
{
   return _header;
}

void rocksdb_backend::set_block_header( const protocol::block_header& header )
{
   _header = header;
}

void rocksdb_backend::load_metadata()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   std::string value;
   auto status = _db->Get(
      *_ropts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::size_key ),
      &value );

   KOINOS_ASSERT( status.ok(), rocksdb_read_exception, "unable to read from rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   _size = util::converter::to< size_type >( value );

   status = _db->Get(
      *_ropts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::revision_key ),
      &value );

   KOINOS_ASSERT( status.ok(), rocksdb_read_exception, "unable to read from rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   _revision = util::converter::to< size_type >( value );

   status = _db->Get(
      *_ropts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::id_key ),
      &value );

   KOINOS_ASSERT( status.ok(), rocksdb_read_exception, "unable to read from rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   _id = util::converter::to< crypto::multihash >( value );

   status = _db->Get(
      *_ropts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::merkle_root_key ),
      &value );

   KOINOS_ASSERT( status.ok(), rocksdb_read_exception, "unable to read from rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   _merkle_root = util::converter::to< crypto::multihash >( value );

   status = _db->Get(
      *_ropts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::block_header_key ),
      &value );

   KOINOS_ASSERT( status.ok(), rocksdb_read_exception, "unable to read from rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   _header = util::converter::to< protocol::block_header >( value );
}

void rocksdb_backend::store_metadata()
{
   KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );

   auto status = _db->Put(
      _wopts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::size_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( _size ) )
   );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   status = _db->Put(
      _wopts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::revision_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( _revision ) )
   );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   status = _db->Put(
      _wopts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::id_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( _id ) )
   );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   status = _db->Put(
      _wopts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::merkle_root_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( _merkle_root ) )
   );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

   status = _db->Put(
      _wopts,
      &*_handles[ constants::metadata_column_index ],
      ::rocksdb::Slice( constants::block_header_key ),
      ::rocksdb::Slice( util::converter::as< std::string >( _header ) )
   );

   KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "unable to write to rocksdb database" + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
}

} // koinos::state_db::backends::rocksdb
