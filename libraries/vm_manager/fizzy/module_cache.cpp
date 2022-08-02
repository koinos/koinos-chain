#include <koinos/vm_manager/fizzy/module_cache.hpp>
#include <koinos/vm_manager/fizzy/exceptions.hpp>

namespace koinos::vm_manager::fizzy {

module_cache::module_cache( std::size_t size ) : _cache_size( size ) {}

module_cache::~module_cache()
{
   std::lock_guard< std::mutex > lock( _mutex );
   _module_map.clear();
}

module_ptr module_cache::get_module( const std::string& id )
{
   std::lock_guard< std::mutex > lock( _mutex );

   auto itr = _module_map.find( id );
   if ( itr == _module_map.end() )
      return module_ptr();

   // Erase the entry from the list and push front
   _lru_list.erase( itr->second.second );
   _lru_list.push_front( id );
   auto ptr = itr->second.first;
   _module_map[ id ] = std::make_pair( ptr, _lru_list.begin() );

   return ptr;
}

void module_cache::put_module( const std::string& id, module_ptr module )
{
   std::lock_guard< std::mutex > lock( _mutex );

   // If the cache is full, remove the last entry from the map, free the fizzy module, and pop back
   if ( _lru_list.size() >= _cache_size )
   {
      const auto key = _lru_list.back();
      auto it = _module_map.find( key );
      if ( it != _module_map.end() )
         _module_map.erase( it );

      _lru_list.pop_back();
   }

   _lru_list.push_front( id );
   _module_map[ id ] = std::make_pair( module, _lru_list.begin() );
}

} // koinos::vm_manager::fizzy
