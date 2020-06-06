#include <mira/database_configuration.hpp>

#include <nlohmann/json.hpp>

#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
#define GB(x)   ((size_t) (x) << 30)

namespace mira {

namespace utilities {

nlohmann::json default_database_configuration()
{
   nlohmann::json j;

   // global
   j["global"]["object_count"] = 62500; // 4GB heaviest usage
   j["global"]["statistics"] = false;   // Incurs severe performance degradation when true

   // global::shared_cache
   j["global"]["shared_cache"]["capacity"] = GB(5);

   // global::write_buffer_manager
   j["global"]["write_buffer_manager"]["write_buffer_size"] = GB(1); // Write buffer manager is within the shared cache

   // base
   j["base"]["optimize_level_style_compaction"] = true;
   j["base"]["increase_parallelism"] = true;

   // base::block_based_table_options
   j["base"]["block_based_table_options"]["block_size"] = KB(8);
   j["base"]["block_based_table_options"]["cache_index_and_filter_blocks"] = true;

   // base::block_based_table_options::bloom_filter_policy
   j["base"]["block_based_table_options"]["bloom_filter_policy"]["bits_per_key"] = 10;
   j["base"]["block_based_table_options"]["bloom_filter_policy"]["use_block_based_builder"] = false;

   return j;
}

} // utilities

} // mira
