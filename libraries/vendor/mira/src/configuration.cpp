#include <boost/core/demangle.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <mira/configuration.hpp>

#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <rocksdb/statistics.h>
#include <rocksdb/rate_limiter.h>
#include <rocksdb/convenience.h>

namespace mira {

mira_config_error::mira_config_error( std::string&& msg ) : _msg( msg ) {}
mira_config_error::~mira_config_error() {}

const char* mira_config_error::what() const noexcept
{
   return _msg.c_str();
}

// Base configuration for an index
#define BASE                             "base"

// Global options
#define GLOBAL                           "global"
#define SHARED_CACHE                     "shared_cache"
#define WRITE_BUFFER_MANAGER             "write_buffer_manager"
#define OBJECT_COUNT                     "object_count"
#define STATISTICS                       "statistics"

// Write buffer manager options
#define WRITE_BUFFER_SIZE                "write_buffer_size"

// Shared cache options
#define CAPACITY                         "capacity"
#define NUM_SHARD_BITS                   "num_shard_bits"

// Database options
#define ALLOW_MMAP_READS                 "allow_mmap_reads"
#define WRITE_BUFFER_SIZE                "write_buffer_size"
#define MAX_BYTES_FOR_LEVEL_BASE         "max_bytes_for_level_base"
#define TARGET_FILE_SIZE_BASE            "target_file_size_base"
#define MAX_WRITE_BUFFER_NUMBER          "max_write_buffer_number"
#define MAX_BACKGROUND_COMPACTIONS       "max_background_compactions"
#define MAX_BACKGROUND_FLUSHES           "max_background_flushes"
#define MIN_WRITE_BUFFER_NUMBER_TO_MERGE "min_write_buffer_number_to_merge"
#define OPTIMIZE_LEVEL_STYLE_COMPACTION  "optimize_level_style_compaction"
#define INCREASE_PARALLELISM             "increase_parallelism"
#define BLOCK_BASED_TABLE_OPTIONS        "block_based_table_options"
#define BLOCK_SIZE                       "block_size"
#define BLOOM_FILTER_POLICY              "bloom_filter_policy"
#define BITS_PER_KEY                     "bits_per_key"
#define USE_BLOCK_BASED_BUILDER          "use_block_based_builder"
#define CACHE_INDEX_AND_FILTER_BLOCKS    "cache_index_and_filter_blocks"

static std::shared_ptr< rocksdb::Cache > global_shared_cache;
static std::shared_ptr< rocksdb::WriteBufferManager > global_write_buffer_manager;

static std::map< std::string, std::function< void( ::rocksdb::Options&, nlohmann::json& ) > > global_database_option_map {
   { ALLOW_MMAP_READS,                  []( ::rocksdb::Options& o, nlohmann::json& j ) { o.allow_mmap_reads = j.template get< bool >(); }                 },
   { WRITE_BUFFER_SIZE,                 []( ::rocksdb::Options& o, nlohmann::json& j ) { o.write_buffer_size = j.template get< uint64_t >(); }            },
   { MAX_BYTES_FOR_LEVEL_BASE,          []( ::rocksdb::Options& o, nlohmann::json& j ) { o.max_bytes_for_level_base = j.template get< uint64_t >(); }     },
   { TARGET_FILE_SIZE_BASE,             []( ::rocksdb::Options& o, nlohmann::json& j ) { o.target_file_size_base = j.template get< uint64_t >(); }        },
   { MAX_WRITE_BUFFER_NUMBER,           []( ::rocksdb::Options& o, nlohmann::json& j ) { o.max_write_buffer_number = j.template get< int >(); }           },
   { MAX_BACKGROUND_COMPACTIONS,        []( ::rocksdb::Options& o, nlohmann::json& j ) { o.max_background_compactions = j.template get< int >(); }        },
   { MAX_BACKGROUND_FLUSHES,            []( ::rocksdb::Options& o, nlohmann::json& j ) { o.max_background_flushes = j.template get< int >(); }            },
   { MIN_WRITE_BUFFER_NUMBER_TO_MERGE,  []( ::rocksdb::Options& o, nlohmann::json& j ) { o.min_write_buffer_number_to_merge = j.template get< int >(); }  },
   { OPTIMIZE_LEVEL_STYLE_COMPACTION,   []( ::rocksdb::Options& o, nlohmann::json& j )
      {
         if ( j.template get< bool >() )
            o.OptimizeLevelStyleCompaction();
      }
   },
   { INCREASE_PARALLELISM, []( ::rocksdb::Options& o, nlohmann::json& j )
      {
         if ( j.template get< bool >() )
            o.IncreaseParallelism();
      }
   },
   { BLOCK_BASED_TABLE_OPTIONS, []( ::rocksdb::Options& o, nlohmann::json& j )
      {
         ::rocksdb::BlockBasedTableOptions table_options;
         if( !j.is_object() )
            throw mira_config_error( "Expected '" BLOCK_BASED_TABLE_OPTIONS "' to be an object" );

         table_options.block_cache = global_shared_cache;
         if( j.is_object() )
         {
            if ( j.contains( BLOCK_SIZE ) )
            {
               if( !j[ BLOCK_SIZE ].is_number() )
                  throw mira_config_error( "Expected '" BLOCK_SIZE "' to be an unsigned integer" );

               table_options.block_size = j[ BLOCK_SIZE ].template get< uint64_t >();
            }

            if ( j.contains( CACHE_INDEX_AND_FILTER_BLOCKS ) )
            {
               if( !j[ CACHE_INDEX_AND_FILTER_BLOCKS ]. is_boolean() )
                  throw mira_config_error( "Expected '" CACHE_INDEX_AND_FILTER_BLOCKS "' to be a boolean" );

               table_options.cache_index_and_filter_blocks = j[ CACHE_INDEX_AND_FILTER_BLOCKS ].template get< bool >();
            }

            if ( j.contains( BLOOM_FILTER_POLICY ) )
            {
               if( !j[ BLOOM_FILTER_POLICY ].is_object() )
                  throw mira_config_error( "Expected '" BLOOM_FILTER_POLICY "' to be an object" );

               auto& filter_policy = j[ BLOOM_FILTER_POLICY ];
               size_t bits_per_key;

               // Bits per key is required for the bloom filter policy
               if( !filter_policy.contains( BITS_PER_KEY ) )
                  throw mira_config_error( "Expected '" BLOOM_FILTER_POLICY "' to contain '" BITS_PER_KEY "'" );

               if( !filter_policy[ BITS_PER_KEY ].is_number() )
                  throw mira_config_error( "Expected '" BITS_PER_KEY "' to be an unsigned integer" );

               bits_per_key = filter_policy[ BITS_PER_KEY ].template get< uint64_t >();

               if( filter_policy.contains( USE_BLOCK_BASED_BUILDER ) )
               {
                  if( !filter_policy[ USE_BLOCK_BASED_BUILDER ]. is_boolean() )
                     throw mira_config_error( "Expected '" USE_BLOCK_BASED_BUILDER "' to be a boolean" );

                  table_options.filter_policy.reset( rocksdb::NewBloomFilterPolicy( bits_per_key, filter_policy[ USE_BLOCK_BASED_BUILDER ].template get< bool >() ) );
               }
               else
               {
                  table_options.filter_policy.reset( rocksdb::NewBloomFilterPolicy( bits_per_key ) );
               }
            }
         }
         else
         {
            throw mira_config_error( "Expected json object" );
         }

         o.table_factory.reset( ::rocksdb::NewBlockBasedTableFactory( table_options ) );
      }
   }
};

void apply_configuration_overlay( nlohmann::json& base, const nlohmann::json& overlay )
{
   if( !base.is_object() )
      throw mira_config_error( "Expected '" BASE "' configuration to be an object" );

   if( !overlay.is_object() )
      throw mira_config_error( "Expected database overlay configuration to be an object" );

   // Iterate through the overlay overriding the base values
   for( auto it = overlay.begin(); it != overlay.end(); ++it )
      base[ it.key() ] = it.value();
}

nlohmann::json& retrieve_global_configuration( nlohmann::json& j )
{
   if( !j.contains( GLOBAL ) )
      throw mira_config_error( "Does not contain object '" GLOBAL "'" );

   if( !j[ GLOBAL ].is_object() )
      throw mira_config_error( "Expected '" GLOBAL "' configuration to be an object" );

   return j[ GLOBAL ];
}

nlohmann::json& retrieve_active_configuration( nlohmann::json& j, std::string type_name )
{
   std::vector< std::string > split_v;
   boost::split( split_v, type_name, boost::is_any_of( ":" ) );
   const auto index_name = *(split_v.rbegin());

   if( !j.contains( BASE ) )
      throw mira_config_error( "Does not contain object '" BASE "'" );

   if( !j[ BASE ].is_object() )
      throw mira_config_error( "Expected '" BASE "' configuration to be an object" );

   // We look to apply an index configuration overlay
   if ( j.find( index_name ) != j.end() )
      apply_configuration_overlay( j[ BASE ], j[ index_name ] );

   return j[ BASE ];
}

size_t configuration::get_object_count( const std::any& cfg )
{
   size_t object_count = 0;

   auto j = std::any_cast< nlohmann::json >( cfg );
   if( !j.is_object() )
      throw mira_config_error( "Expected database configuration to be an object" );

   const nlohmann::json& global_config = retrieve_global_configuration( j );

   if( !global_config.contains( OBJECT_COUNT ) )
      throw mira_config_error( "Expected '" GLOBAL "' configuration to contain '" OBJECT_COUNT "'" );

   if( !global_config[ OBJECT_COUNT ].is_number() )
      throw mira_config_error( "Expected '" OBJECT_COUNT "' to be an unsigned integer" );

   object_count = global_config[ OBJECT_COUNT ].get< uint64_t >();

   return object_count;
}

bool configuration::gather_statistics( const std::any& cfg )
{
   bool statistics = false;

   auto j = std::any_cast< nlohmann::json >( cfg );
   if( !j.is_object() )
      throw mira_config_error( "Expected database configuration to be an object" );

   const nlohmann::json& global_config = retrieve_global_configuration( j );

   if( !global_config.contains( STATISTICS ) )
      throw mira_config_error( "Expected '" GLOBAL "' configuration to contain '" STATISTICS "'" );

   if( !global_config[ STATISTICS ].is_boolean() )
      throw mira_config_error( "Expected '" STATISTICS "' to be a boolean value" );

   statistics = global_config[ STATISTICS ].get< bool >();

   return statistics;
}

::rocksdb::Options configuration::get_options( const std::any& cfg, std::string type_name )
{
   ::rocksdb::Options opts;

   auto j = std::any_cast< nlohmann::json >( cfg );
   if( !j.is_object() )
      throw mira_config_error( "Expected database configuration to be an object" );

   if( global_shared_cache == nullptr )
   {
      size_t capacity = 0;
      int num_shard_bits = 0;

      const nlohmann::json& global_config = retrieve_global_configuration( j );

      if( !global_config.contains( SHARED_CACHE ) )
         throw mira_config_error( "Expected '" GLOBAL "' configuration to contain '" SHARED_CACHE "'" );

      if( !global_config[ SHARED_CACHE ].is_object() )
         throw mira_config_error( "Expected '" SHARED_CACHE "' to be an object" );

      auto& shared_cache_obj = global_config[ SHARED_CACHE ];

      if( !shared_cache_obj.contains( CAPACITY ) )
         throw mira_config_error( "Expected '" SHARED_CACHE "' configuration to contain '" CAPACITY "'" );

      if( !shared_cache_obj[ CAPACITY ].is_number() )
         throw mira_config_error( "Expected '" CAPACITY "' to be a an unsigned integer" );

      capacity = shared_cache_obj[ CAPACITY ].get< uint64_t >();

      if ( shared_cache_obj.contains( NUM_SHARD_BITS ) )
      {
         if( !shared_cache_obj[ NUM_SHARD_BITS ].is_number() )
            throw mira_config_error( "Expected '" NUM_SHARD_BITS "' to be an unsigned integer" );

         num_shard_bits = shared_cache_obj[ NUM_SHARD_BITS ].get< int >();

         global_shared_cache = rocksdb::NewLRUCache( capacity, num_shard_bits );
      }
      else
      {
         global_shared_cache = rocksdb::NewLRUCache( capacity );
      }
   }

   if ( global_write_buffer_manager == nullptr )
   {
      size_t write_buf_size = 0;

      const nlohmann::json& global_config = retrieve_global_configuration( j );

      if( !global_config.contains( WRITE_BUFFER_MANAGER ) )
         throw mira_config_error( "Expected '" GLOBAL "' configuration to contain '" WRITE_BUFFER_MANAGER "'" );

      if( !global_config[ WRITE_BUFFER_MANAGER ].is_object() )
         throw mira_config_error( "Expected '" WRITE_BUFFER_MANAGER "' to be an object" );

      auto& write_buffer_mgr_obj = global_config[ WRITE_BUFFER_MANAGER ];

      if( !write_buffer_mgr_obj.contains( WRITE_BUFFER_SIZE ) )
         throw mira_config_error( "Expected '" WRITE_BUFFER_MANAGER "' configuration to contain '" WRITE_BUFFER_SIZE "'" );

      if( !write_buffer_mgr_obj[ WRITE_BUFFER_SIZE ].is_number() )
         throw mira_config_error( "Expected '" WRITE_BUFFER_SIZE "' to be a an unsigned integer" );

      write_buf_size = write_buffer_mgr_obj[ WRITE_BUFFER_SIZE ].get< uint64_t >();

      global_write_buffer_manager = std::make_shared< ::rocksdb::WriteBufferManager >( write_buf_size, global_shared_cache );
   }

   // We assign the global write buffer manager to all databases
   opts.write_buffer_manager = global_write_buffer_manager;

   nlohmann::json& config = retrieve_active_configuration( j, type_name );

   for( auto it = config.begin(); it != config.end(); ++it )
   {
      if ( global_database_option_map.find( it.key() ) != global_database_option_map.end() )
         global_database_option_map[ it.key() ]( opts, it.value() );
   }

   return opts;
}

} // mira
