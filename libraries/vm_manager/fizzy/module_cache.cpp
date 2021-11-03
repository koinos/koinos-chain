#include <koinos/vm_manager/fizzy/module_cache.hpp>

namespace koinos::vm_manager::fizzy {

module_cache::module_cache( std::size_t size ) : _cache_size( size ) {}

module_cache::~module_cache()
{
   for ( const auto& [hash, module_entry] : _module_map )
   {
      fizzy_free_module( module_entry.first );
   }
}

const FizzyModule* module_cache::get_module( const std::string& id )
{
   auto itr = _module_map.find( id );
   if ( itr == _module_map.end() )
      return nullptr;

   // Erase the entry from the list and push front
   itr->second.second->erase();
   _lru_list.push_front( id );
   auto module_ptr = itr->second.first;
   _module_map[ id ] = std::make_pair( module_ptr, _lru_list.begin() );

   return fizzy_clone_module( module_ptr );
}

void module_cache::put_module( const std::string& id, const FizzyModule* module )
{
   // If the cache is pull, remove the last entry from the map and pop back
   if ( _lru_list.size() >= _cache_size )
   {
      _module_map.erase( _lru_list.back() );
      _lru_list.pop_back();
   }

   _module_map[ id ] = std::make_pair( fizzy_clone_module( module ), _lru_list.begin() );
}

} // koinos::vm_manager::fizzy
