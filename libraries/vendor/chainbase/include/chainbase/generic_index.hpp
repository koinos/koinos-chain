#pragma once
#include <chainbase/undo_state.hpp>
#include <chainbase/merge_iterator.hpp>

namespace chainbase {

   /**
    *  The value_type stored in the multiindex container must have a integer field with the name 'id'.  This will
    *  be the primary key and it will be assigned and managed by generic_index.
    */
   template< typename MultiIndexType >
   class generic_index
   {
      public:
         typedef MultiIndexType                                         index_type;
         typedef typename index_type::value_type                        value_type;
         typedef typename value_type::id_type                           id_type;
         typedef undo_state< index_type >                               undo_state_type;

         boost::container::deque< std::shared_ptr< undo_state_type > > _deque;

         generic_index() {}

         template<typename Constructor>
         const value_type& emplace( Constructor&& c )
         {
            require_open();
            auto emplace_result = _deque.back()->emplace( c );

            if( !emplace_result.second )
               BOOST_THROW_EXCEPTION( std::logic_error( "could not insert object, most likely a uniqueness constraint was violated" ) );

            return *emplace_result.first;
         }

         template< typename Modifier >
         void modify( const value_type& obj, Modifier&& m )
         {
            require_open();
            bool ok = false;

            ok = _deque.back()->modify( obj, m );

            if( !ok ) BOOST_THROW_EXCEPTION( std::logic_error( "Could not modify object, most likely a uniqueness constraint was violated" ) );
         }

         void remove( const value_type& obj )
         {
            require_open();
            _deque.back()->erase( obj );
         }

         template< typename IndexedByType, typename CompatibleKey >
         const value_type* find( CompatibleKey&& key ) const
         {
            require_open();
            return _deque.back()->template find< IndexedByType >( key );
         }

         const value_type* find( id_type key = id_type() ) const
         {
            require_open();
            return _deque.back()->find( key );
         }

         template< typename CompatibleKey >
         const value_type& get( CompatibleKey&& key ) const
         {
            auto ptr = find( key );
            if( !ptr ) BOOST_THROW_EXCEPTION( std::out_of_range( "key not found" ) );
            return *ptr;
         }

         template< typename IndexedByType, typename CompatibleKey >
         const value_type& get( CompatibleKey&& key ) const
         {
            auto ptr = find< IndexedByType >( key );
            if( !ptr ) BOOST_THROW_EXCEPTION( std::out_of_range( "key not found" ) );
            return *ptr;
         }

         void clear()
         {
            require_open();
            mutable_indices()->clear();
            _deque.clear();
         }

         void open( const boost::filesystem::path& p, const boost::any& o )
         {
            if( _deque.size() )
               BOOST_THROW_EXCEPTION( std::runtime_error( "Index is already open" ) );

            _deque.push_back( std::make_shared< undo_state_type >( p, o ) );
         }

         void close()
         {
            require_open();
            _deque.clear();
         }

         void wipe( const boost::filesystem::path& dir )
         {
            require_open();
            mutable_indices()->wipe( dir );
         }

         void flush()
         {
            require_open();
            mutable_indices()->flush();
         }

         size_t get_cache_usage() const
         {
            return indices()->get_cache_usage();
         }

         size_t get_cache_size() const
         {
            return indices()->get_cache_size();
         }

         void dump_lb_call_counts()
         {
            mutable_indices()->dump_lb_call_counts();
         }

         void trim_cache()
         {
            mutable_indices()->trim_cache();
         }

         size_t size() const
         {
            require_open();
            return _deque.back()->size();
         }

//         void begin_bulk_load()
//         {
//            indices()->begin_bulk_load();
//         }
//
//         void end_bulk_load()
//         {
//            indices()->end_bulk_load();
//         }
//
//         void flush_bulk_load()
//         {
//            indices()->flush_bulk_load();
//         }

         class session
         {
            public:
               session( session&& mv )
               :_index(mv._index),_apply(mv._apply){ mv._apply = false; }

               ~session() {
                  if( _apply ) {
                     _index.undo();
                  }
               }

               /** leaves the UNDO state on the stack when session goes out of scope */
               void push()   { _apply = false; }
               /** combines this session with the prior session */
               void squash() { if( _apply ) _index.squash(); _apply = false; }
               void undo()   { if( _apply ) _index.undo();  _apply = false; }

               session& operator = ( session&& mv )
               {
                  if( this == &mv ) return *this;
                  if( _apply ) _index.undo();
                  _apply = mv._apply;
                  mv._apply = false;
                  return *this;
               }

               int64_t revision()const { return _index.revision(); }

            private:
               friend class generic_index;

               session( generic_index& idx, int64_t revision ) :
                  _index( idx ),
                  _revision( revision )
               {
                  if( _index.revision() == -1 )
                     _apply = false;
               }

               generic_index& _index;
               bool           _apply = true;
               int64_t        _revision = 0;
         };

         session start_undo_session()
         {
            _deque.push_back( std::make_shared< undo_state_type >( _deque.back() ) );
            return session( *this, revision() );
         }

         /**
          *  Restores the state to how it was prior to the current session discarding all changes
          *  made between the last revision and the current revision.
          */
         void undo()
         {
            if( !undo_enabled() ) return;
            _deque.pop_back();
         }

         /**
          *  This method works similar to git squash, it merges the change set from the two most
          *  recent revision numbers into one revision number (reducing the head revision number)
          *
          *  This method does not change the state of the index, only the state of the undo buffer.
          */
         void squash()
         {
            if( !undo_enabled() ) return;
            _deque.back()->squash();
            _deque.pop_back();
         }

         /**
          * Discards all undo history prior to revision
          */
         void commit( int64_t revision )
         {
            // Keep a copy of the pointer to the front state to keep it alive for the duration of the call.
            std::shared_ptr< undo_state_type > front = _deque.front();

            while( _deque.size() && _deque.front()->revision() < revision )
            {
               _deque.pop_front();
            }

            _deque.front()->commit();
         }

         /**
          * Unwinds all undo states
          */
         void undo_all()
         {
            while( _deque.size() > 1 )
               _deque.pop_back();
         }

         void set_revision( int64_t revision )
         {
            require_open();
            if( _deque.size() > 1 )
               BOOST_THROW_EXCEPTION( std::logic_error("cannot set revision while there is an existing undo stack") );
            mutable_indices()->set_revision( revision );
         }

         int64_t revision() const
         {
            require_open();
            return _deque.back()->revision();
         }

         template< typename IndexedByType >
         class secondary_index
         {
            public:
               typedef decltype( ((index_type*)nullptr)->template get< IndexedByType >() ) by_index_type;
               typedef typename index_type::value_type                                     value_type;
               typedef merge_iterator< index_type, IndexedByType >                         iterator_type;

               secondary_index( const generic_index< index_type >& index ) : _index( index ) {}

               template< typename CompatibleKey >
               iterator_type lower_bound( CompatibleKey&& key ) const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     return idx.lower_bound( key );
                  });
               }

               template< typename CompatibleKey >
               iterator_type upper_bound( CompatibleKey&& key ) const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     return idx.upper_bound( key );
                  });
               }

               template< typename CompatibleKey >
               std::pair< iterator_type, iterator_type > equal_range( CompatibleKey&& key ) const
               {
                  return std::make_pair< iterator_type, iterator_type >(
                     lower_bound( key ), upper_bound( key ) );
               }

               iterator_type begin() const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     return idx.begin();
                  });
               }

               iterator_type end() const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     return idx.end();
                  });
               }

               template< typename CompatibleKey >
               iterator_type find( CompatibleKey&& key ) const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     return idx.find( key );
                  });
               }

               iterator_type iterator_to( const value_type& v ) const
               {
                  return iterator_type( _index._deque, [&]( by_index_type& idx )
                  {
                     auto itr = idx.iterator_to( v );
                     return itr != idx.end() ? itr : idx.upper_bound( v );
                  });
               }

               size_t size() const
               {
                  return _index.size();
               }

            private:
               const generic_index< index_type >& _index;
         };

         template< typename IndexedByType >
         const secondary_index< IndexedByType > get_secondary_index() const
         {
            return secondary_index< IndexedByType >( *this );
         }

      private:
         void require_open() const
         {
            if( !_deque.size() )
               BOOST_THROW_EXCEPTION( std::runtime_error( "Index is not open" ) );
         }

         bool undo_enabled()const
         {
            return _deque.size() > 1;
         }

         std::shared_ptr< undo_state_type >& mutable_indices()
         {
            return _deque.front();
         }

         const std::shared_ptr< undo_state_type >& indices() const
         {
            return _deque.front();
         }
   };

} // chainbase
