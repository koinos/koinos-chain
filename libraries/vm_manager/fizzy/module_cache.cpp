#include <koinos/vm_manager/fizzy/module_cache.hpp>
#include <koinos/vm_manager/fizzy/exceptions.hpp>

namespace koinos::vm_manager::fizzy {

module_cache::module_cache( std::size_t size ) : _cache_size( size ) {}

module_cache::~module_cache()
{
   std::lock_guard< std::mutex > lock( _mutex );

   for ( const auto& [ hash, module_entry ] : _module_map )
      fizzy_free_module( module_entry.first );
}

const FizzyModule* module_cache::get_module( const std::string& id )
{
   std::lock_guard< std::mutex > lock( _mutex );

   auto itr = _module_map.find( id );
   if ( itr == _module_map.end() )
      return nullptr;

   // Erase the entry from the list and push front
   _lru_list.erase( itr->second.second );
   _lru_list.push_front( id );
   auto module_ptr = itr->second.first;
   _module_map[ id ] = std::make_pair( module_ptr, _lru_list.begin() );

   auto cloned_module = fizzy_clone_module( module_ptr );
   KOINOS_ASSERT( cloned_module, module_clone_exception, "failed to clone module" );

   return cloned_module;
}

void module_cache::put_module( const std::string& id, const FizzyModule* module )
{
   std::lock_guard< std::mutex > lock( _mutex );

   // If the cache is full, remove the last entry from the map, free the fizzy module, and pop back
   if ( _lru_list.size() >= _cache_size )
   {
      const auto key = _lru_list.back();
      auto it = _module_map.find( key );
      if ( it != _module_map.end() )
      {
         fizzy_free_module( it->second.first );
         _module_map.erase( it );
      }
      _lru_list.pop_back();
   }

   _lru_list.push_front( id );

   auto cloned_module = fizzy_clone_module( module );
   KOINOS_ASSERT( cloned_module, module_clone_exception, "failed to clone module" );
   _module_map[ id ] = std::make_pair( cloned_module, _lru_list.begin() );
}

} // koinos::vm_manager::fizzy
