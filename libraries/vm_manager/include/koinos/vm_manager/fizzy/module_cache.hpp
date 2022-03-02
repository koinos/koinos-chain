#include <fizzy/fizzy.h>

#include <list>
#include <map>
#include <mutex>
#include <string>

namespace koinos::vm_manager::fizzy {

class module_cache
{
   private:
      using lru_list_type = std::list< std::string >;

      using module_map_type =
         std::map<
            std::string,
            std::pair<
               const FizzyModule*,
               typename lru_list_type::iterator
            >
         >;

      lru_list_type     _lru_list;
      module_map_type   _module_map;
      std::mutex        _mutex;
      const std::size_t _cache_size;

   public:
      module_cache( std::size_t size );
      ~module_cache();

      const FizzyModule* get_module( const std::string& id );
      void put_module( const std::string& id, const FizzyModule* module );
};

} // koinos::vm_manager::fizzy
