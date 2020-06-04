#pragma once
#include <mira/index_converter.hpp>
#include <mira/iterator_adapter.hpp>

namespace mira {

enum index_type
{
   mira = 0,
   bmic = 1
};

template< typename MultiIndexAdapterType, int N = 0 >
struct index_adapter
{
   typedef typename MultiIndexAdapterType::value_type                                              value_type;
//   typedef typename std::remove_reference< decltype(
//      (((typename MultiIndexAdapterType::mira_type*)nullptr)->template get<N>()) ) >::type         mira_type;
//*
   typedef typename std::remove_reference< decltype(
      (((typename MultiIndexAdapterType::mira_type*)nullptr)->template get<
         boost::mpl::size<
            typename MultiIndexAdapterType::bmic_type::index_type_list
         >::value - N - 1
      >()) ) >::type                                                                               mira_type;
//*/
   typedef typename std::remove_reference< decltype(
      (((typename MultiIndexAdapterType::bmic_type*)nullptr)->template get<N>()) ) >::type         bmic_type;
/*
   typedef typename std::remove_reference< decltype(
      (((typename MultiIndexAdapterType::bmic_type*)nullptr)->template get<
         boost::mpl::size<
            typename MultiIndexAdapterType::mira_type::index_type_list
         >::value - N - 1
      >()) ) >::type                                                                               bmic_type;
//*/

   typedef decltype( (((mira_type*)nullptr)->begin()) )     mira_iter_type;
   typedef decltype( (((bmic_type*)nullptr)->begin()) )     bmic_iter_type;

   typedef iterator_adapter<
      value_type,
      mira_iter_type,
      bmic_iter_type >                                                                            iter_type;
   typedef boost::reverse_iterator< iter_type >                                                   rev_iter_type;

   typedef boost::variant< mira_type*, bmic_type* > index_variant;

   private:
      index_adapter() {}

      struct erase_visitor : public boost::static_visitor< iter_type >
      {
         iter_type& _pos;

         erase_visitor( iter_type& pos ) : _pos( pos ) {}

         iter_type operator()( mira_type* idx_ptr ) const
         {
            return iter_type( idx_ptr->erase( _pos.template get< mira_iter_type >() ) );
         }

         iter_type operator()( bmic_type* idx_ptr ) const
         {
            return iter_type( idx_ptr->erase( _pos.template get< bmic_iter_type >() ) );
         }
      };

   public:
      index_adapter( const mira_type& mira_index )
      {
         _index = const_cast< mira_type* >( &mira_index );
      }

      index_adapter( const bmic_type& bmic_index )
      {
         _index = const_cast< bmic_type* >( &bmic_index );
      }

      index_adapter( const index_adapter& other ) :
         _index( other._index )
      {}

      index_adapter( index_adapter&& other ) :
         _index( std::move( other._index ) )
      {}

      iter_type erase( iter_type position )
      {
         return boost::apply_visitor( erase_visitor( position ), _index );
      }

      iter_type iterator_to( const value_type& v )const
      {
         return boost::apply_visitor(
            [&]( auto* index ){ return iter_type( index->iterator_to( v ) ); },
            _index
         );
      }

      iter_type find( const value_type& v )const
      {
         return boost::apply_visitor(
            [&]( auto* index ){ return iter_type( index->find( index->key_extractor()( v ) ) ); },
            _index
         );
      }

      template< typename CompatibleKey >
      iter_type find( const CompatibleKey& k )const
      {
         return boost::apply_visitor(
            [&k]( auto* index ){ return iter_type( index->find( k ) ); },
            _index
         );
      }

      iter_type lower_bound( const value_type& v )const
      {
         return boost::apply_visitor(
            [&v]( auto* index ){ return iter_type( index->lower_bound( index->key_extractor()( v ) ) ); },
            _index
         );
      }

      template< typename CompatibleKey >
      iter_type lower_bound( const CompatibleKey& k )const
      {
         return boost::apply_visitor(
            [&k]( auto* index ){ return iter_type( index->lower_bound( k ) ); },
            _index
         );
      }

      iter_type upper_bound( const value_type& v )const
      {
         return boost::apply_visitor(
            [&v]( auto* index ){ return iter_type( index->upper_bound( index->key_extractor()( v ) ) ); },
            _index
         );
      }

      template< typename CompatibleKey >
      iter_type upper_bound( const CompatibleKey& k )const
      {
         return boost::apply_visitor(
            [&k]( auto* index ){ return iter_type( index->upper_bound( k ) ); },
            _index
         );
      }

      template< typename CompatibleKey >
      std::pair< iter_type, iter_type > equal_range( const CompatibleKey& k )const
      {
         return boost::apply_visitor(
            [&k]( auto* index )
            {
               auto result = index->equal_range( k );
               return std::pair< iter_type, iter_type >(
                  std::move( result.first ),
                  std::move( result.second ) );
            },
            _index
         );
      }

      iter_type begin()const
      {
         return boost::apply_visitor(
            []( auto* index ){ return iter_type( index->begin() ); },
            _index
         );
      }

      iter_type end()const
      {
         return boost::apply_visitor(
            []( auto* index ){ return iter_type( index->end() ); },
            _index
         );
      }

      rev_iter_type rbegin()const
      {
         return boost::make_reverse_iterator( end() );
      }

      rev_iter_type rend()const
      {
         return boost::make_reverse_iterator( begin() );
      }

      bool empty()const
      {
         return boost::apply_visitor(
            []( auto* index ){ return index->empty(); },
            _index
         );
      }

      size_t size()const
      {
         return boost::apply_visitor(
            []( auto* index ){ return index->size(); },
            _index
         );
      }

   private:
      index_variant _index;
};

template< typename Value, typename Serializer, typename IndexSpecifierList, typename Allocator = std::allocator< Value > >
struct multi_index_adapter
{
   typedef Value                                                                                           value_type;
   typedef typename multi_index::multi_index_container< Value, Serializer, IndexSpecifierList, Allocator > container_type ;
   typedef typename index_converter< container_type >::mira_type                                           mira_type;
   typedef typename mira_type::primary_iterator                                                            mira_iter_type;
   typedef typename index_converter< container_type >::bmic_type                                           bmic_type;
   typedef typename bmic_type::iterator                                                                    bmic_iter_type;
   typedef typename value_type::id_type id_type;

   typedef iterator_adapter<
      value_type,
      mira_iter_type,
      bmic_iter_type >                                                                                   iter_type;
   typedef boost::reverse_iterator< iter_type >                                                          rev_iter_type;

   typedef typename bmic_type::allocator_type                                                            allocator_type;

   typedef boost::variant<
      mira_type,
      bmic_type > index_variant;

   typedef index_type type_enum;

   typedef multi_index_adapter< Value, Serializer, IndexSpecifierList, Allocator >                       adapter_type;

   multi_index_adapter()
   {
      _index = mira_type();
   }

   multi_index_adapter( allocator_type& a )
   {
      _index = mira_type();
   }

   multi_index_adapter( index_type type ) :
      _type( type )
   {
      switch( _type )
      {
         case mira:
            _index = mira_type();
            break;
         case bmic:
            _index = bmic_type();
            break;
      }
   }

   multi_index_adapter( index_type type, allocator_type& a ) :
      _type( type )
   {
      switch( _type )
      {
         case mira:
            _index = mira_type();
            break;
         case bmic:
            _index = bmic_type( a );
            break;
      }
   }

   ~multi_index_adapter() {}

   template< typename Tag >
   struct get_index_number
   {
      typedef typename mira_type::template index< Tag >::iter iter;

      BOOST_STATIC_CONSTANT(
         int, value=(boost::mpl::size< typename bmic_type::index_type_list >::value -
                        boost::mpl::distance< typename bmic_type::template index< Tag >::iter,
                                              typename boost::mpl::end< typename bmic_type::index_type_list >::type >::value));
   };

   template< typename IndexedBy >
   index_adapter< adapter_type, get_index_number< IndexedBy >::value > mutable_get()
   {
      return boost::apply_visitor(
         []( auto& index )
         {
            return index_adapter< adapter_type, get_index_number< IndexedBy >::value >(
               index.template get< IndexedBy >()
            );
         },
         _index
      );
   }

   template< typename IndexedBy >
   const index_adapter< adapter_type, get_index_number< IndexedBy >::value > get() const
   {
      return boost::apply_visitor(
         []( const auto& index )
         {
            return index_adapter< adapter_type, get_index_number< IndexedBy >::value >(
               index.template get< IndexedBy >()
            );
         },
         _index
      );
   }

   template< int N >
   index_adapter< adapter_type, N > mutable_get()
   {
      typedef typename boost::mpl::size< typename bmic_type::index_type_list > tag_size_type;

      if( _type == index_type::mira )
      {
         return index_adapter< adapter_type, N >(
            boost::get< mira_type >( _index ).template get< tag_size_type::value - N - 1 >()
         );
      }
      else
      {
         return index_adapter< adapter_type, N >(
            boost::get< bmic_type >( _index ).template get< N >()
         );
      }
   }

   template< int N >
   const index_adapter< adapter_type, N > get()const
   {
      typedef typename boost::mpl::size< typename bmic_type::index_type_list > tag_size_type;

      if( _type == index_type::mira )
      {
         return index_adapter< adapter_type, N >(
            boost::get< mira_type >( _index ).template get< tag_size_type::value - N - 1 >()
         );
      }
      else
      {
         return index_adapter< adapter_type, N >(
            boost::get< bmic_type >( _index ).template get< N >()
         );
      }
   }

   void set_index_type( index_type type, const boost::filesystem::path& p, const boost::any& cfg )
   {
      if( type == _type ) return;

      index_variant new_index;

      auto id = next_id();
      auto rev = revision();

      {
         auto first = begin();
         auto last = end();

         switch( type )
         {
            case mira:
               new_index = std::move( mira_type( first, last, p, cfg ) );
               break;
            case bmic:
               new_index = std::move( bmic_type( first, last ) );
               break;
         }
      }
      close();
      wipe( p );

      _index = std::move( new_index );
      _type = type;

      set_revision( rev );
      set_next_id( id );
   }

   id_type next_id()
   {
      return boost::apply_visitor(
         []( auto& index ){ return index.next_id(); },
         _index
      );
   }

   void set_next_id( id_type id )
   {
      return boost::apply_visitor(
         [=]( auto& index ){ return index.set_next_id( id ); },
         _index
      );
   }

   uint64_t revision()
   {
      return boost::apply_visitor(
         []( auto& index ){ return index.revision(); },
         _index
      );
   }

   uint64_t set_revision( uint64_t rev )
   {
      return boost::apply_visitor(
         [&rev]( auto& index ){ return index.set_revision( rev ); },
         _index
      );
   }

   template< typename Constructor >
   std::pair< iter_type, bool >
   emplace( Constructor&& con, allocator_type alloc = allocator_type() )
   {
      return boost::apply_visitor(
         [&]( auto& index )
         {
            auto result = index.emplace( std::forward< Constructor >( con ), alloc );
            return std::pair< iter_type, bool >( iter_type( std::move( result.first ) ), result.second );
         },
         _index
      );
   }

   template< typename Modifier >
   bool modify( iter_type& position, Modifier&& mod )
   {
      bool result = false;

      switch( _type )
      {
         case mira:
            result = boost::get< mira_type >( _index ).modify( position.template get< mira_iter_type >(), mod );
            break;
         case bmic:
            result = boost::get< bmic_type >( _index ).modify( position.template get< bmic_iter_type >(), mod );
            break;
      }

      return result;
   }

   template< typename Modifier >
   bool modify( iter_type&& position, Modifier&& mod )
   {
      return modify( position, mod );
   }

   iter_type erase( iter_type& position )
   {
      iter_type result;

      switch( _type )
      {
         case mira:
            result = boost::get< mira_type >( _index ).erase( position.template get< mira_iter_type >() );
            break;
         case bmic:
            result = boost::get< bmic_type >( _index ).erase( position.template get< bmic_iter_type >() );
            break;
      }

      return result;
   }

   iter_type erase( iter_type&& position )
   {
      return erase( position );
   }

   void begin_bulk_load()
   {
      return boost::apply_visitor(
         [&]( auto& index )
         {
            index.begin_bulk_load();
         },
         _index
      );
   }

   void end_bulk_load()
   {
      return boost::apply_visitor(
         [&]( auto& index )
         {
            index.end_bulk_load();
         },
         _index
      );
   }

   void flush_bulk_load()
   {
      return boost::apply_visitor(
         [&]( auto& index )
         {
            index.end_bulk_load();
         },
         _index
      );
   }

   template< typename Lambda >
   void bulk_load( Lambda&& l )
   {
      return boost::apply_visitor(
         [&]( auto& index ){ index.bulk_load( l ); },
         _index
      );
   }

   iter_type iterator_to( const value_type& v )
   {
      return boost::apply_visitor(
         [&]( auto& index ){ return iter_type( index.iterator_to( v ) ); },
         _index
      );
   }

   template< typename CompatibleKey >
   iter_type find( const CompatibleKey& k )const
   {
      return boost::apply_visitor(
         [&k]( auto& index ){ return iter_type( index.find( k ) ); },
         _index
      );
   }

   template< typename CompatibleKey >
   iter_type lower_bound( const CompatibleKey& k )const
   {
      return boost::apply_visitor(
         [&k]( auto& index ){ return iter_type( index.lower_bound( k ) ); },
         _index
      );
   }

   template< typename CompatibleKey >
   iter_type upper_bound( const CompatibleKey& k )const
   {
      return boost::apply_visitor(
         [&k]( auto& index ){ return iter_type( index.upper_bound( k ) ); },
         _index
      );
   }

   template< typename CompatibleKey >
   std::pair< iter_type, iter_type > equal_range( const CompatibleKey& k )const
   {
      return boost::apply_visitor(
         [&k]( auto& index )
         {
            auto result = index.equal_range( k );
            return std::pair< iter_type, iter_type >(
               std::move( result.first ),
               std::move( result.second ) );
         },
         _index
      );
   }

   iter_type begin()const
   {
      return boost::apply_visitor(
         []( auto& index ){ return iter_type( index.begin() ); },
         _index
      );
   }

   iter_type end()const
   {
      return boost::apply_visitor(
         []( auto& index ){ return iter_type( index.end() ); },
         _index
      );
   }

   rev_iter_type rbegin()const
   {
      return boost::make_reverse_iterator( end() );
   }

   rev_iter_type rend()const
   {
      return boost::make_reverse_iterator( begin() );
   }

   bool open( const boost::filesystem::path& p, const boost::any& o, index_type type = index_type::mira )
   {
      if( type != _type )
      {
         index_variant new_index;

         switch( type )
         {
            case mira:
               new_index = std::move( mira_type( p, o ) );
               break;
            case bmic:
               new_index = std::move( bmic_type() );
               break;
         }

         _index = std::move( new_index );
         _type = type;
      }

      return boost::apply_visitor(
         [&p, &o]( auto& index ){ return index.open( p, o ); },
         _index
      );
   }

   void close()
   {
      boost::apply_visitor(
         []( auto& index ){ index.close(); },
         _index
      );
   }

   void wipe( const boost::filesystem::path& p )
   {
      boost::apply_visitor(
         [&]( auto& index ){ index.wipe( p ); },
         _index
      );
   }

   void clear()
   {
      boost::apply_visitor(
         []( auto& index ){ index.clear(); },
         _index
      );
   }

   void flush()
   {
      boost::apply_visitor(
         []( auto& index ){ index.flush(); },
         _index
      );
   }

   size_t size()const
   {
      return boost::apply_visitor(
         []( auto& index ){ return index.size(); },
         _index
      );
   }

   allocator_type get_allocator()const
   {
      return allocator_type();
   }

   template< typename MetaKey, typename MetaValue >
   bool put_metadata( const MetaKey& k, const MetaValue& v )
   {
      return boost::apply_visitor(
         [&k, &v]( auto& index ){ return index.put_metadata( k, v ); },
         _index
      );
   }

   template< typename MetaKey, typename MetaValue >
   bool get_metadata( const MetaKey& k, MetaValue& v )
   {
      return boost::apply_visitor(
         [&k, &v]( auto& index ){ return index.get_metadata( k, v ); },
         _index
      );
   }

   size_t get_cache_usage()const
   {
      return boost::apply_visitor(
         []( auto& index ){ return index.get_cache_usage(); },
         _index
      );
   }

   size_t get_cache_size()const
   {
      return boost::apply_visitor(
         []( auto& index ){ return index.get_cache_size(); },
         _index
      );
   }

   void dump_lb_call_counts()
   {
      boost::apply_visitor(
         []( auto& index ){ index.dump_lb_call_counts(); },
         _index
      );
   }

   void trim_cache()
   {
      boost::apply_visitor(
         []( auto& index ){ index.trim_cache(); },
         _index
      );
   }

   void print_stats()const
   {
      boost::apply_visitor(
         []( auto& index ){ index.print_stats(); },
         _index
      );
   }

   private:
      index_variant  _index;
      index_type     _type = mira;
};

}
