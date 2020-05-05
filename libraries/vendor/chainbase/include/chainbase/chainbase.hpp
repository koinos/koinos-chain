#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/flat_map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <boost/any.hpp>
#include <boost/chrono.hpp>
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/throw_exception.hpp>

#include <chainbase/allocators.hpp>
#include <chainbase/generic_index.hpp>
#include <chainbase/util/object_id.hpp>

#include <array>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>

#ifndef CHAINBASE_NUM_RW_LOCKS
   #define CHAINBASE_NUM_RW_LOCKS 10
#endif

#ifdef CHAINBASE_CHECK_LOCKING
   #define CHAINBASE_REQUIRE_READ_LOCK(m, t) require_read_lock(m, typeid(t).name())
   #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t) require_write_lock(m, typeid(t).name())
#else
   #define CHAINBASE_REQUIRE_READ_LOCK(m, t)
   #define CHAINBASE_REQUIRE_WRITE_LOCK(m, t)
#endif

namespace helpers
{
   struct index_statistic_info
   {
      std::string _value_type_name;
      size_t      _item_count = 0;
      size_t      _item_sizeof = 0;
      /// Additional (ie dynamic container) allocations held in stored items
      size_t      _item_additional_allocation = 0;
      /// Additional memory used for container internal structures (like tree nodes).
      size_t      _additional_container_allocation = 0;
   };

   template <class IndexType>
   void gather_index_static_data(const IndexType& index, index_statistic_info* info)
   {
      info->_value_type_name = boost::core::demangle(typeid(typename IndexType::value_type).name());
      info->_item_count = index.size();
      info->_item_sizeof = sizeof(typename IndexType::value_type);
      info->_item_additional_allocation = 0;
   }

   template <class IndexType>
   class index_statistic_provider
   {
   public:
      index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
      {
         index_statistic_info info;
         gather_index_static_data(index, &info);
         return info;
      }
   };
} /// namespace helpers

namespace chainbase {

   namespace bip = boost::interprocess;
   namespace bfs = boost::filesystem;
   using std::unique_ptr;
   using std::vector;

   enum open_flags
   {
      skip_nothing               = 0,
      skip_env_check             = 1 << 0 // Skip environment check on db open
   };

   template< uint16_t TypeNumber, typename Derived >
   struct object
   {
      typedef oid< Derived > id_type;
      static const uint16_t type_id = TypeNumber;
   };

   /** this class is ment to be specified to enable lookup of index type by object type using
    * the SET_INDEX_TYPE macro.
    **/
   template<typename T >
   struct get_index_type {};

   /**
    *  This macro must be used at global scope and OBJECT_TYPE and INDEX_TYPE must be fully qualified
    */
   #define CHAINBASE_SET_INDEX_TYPE( OBJECT_TYPE, INDEX_TYPE )  \
   namespace chainbase { template<> struct get_index_type<OBJECT_TYPE> { typedef INDEX_TYPE type; }; }

   #define CHAINBASE_DEFAULT_CONSTRUCTOR( OBJECT_TYPE ) \
   template< typename Constructor > \
   OBJECT_TYPE( Constructor&& c ) { c(*this); }

   /**
    * The code we want to implement is this:
    *
    * ++target; try { ... } finally { --target }
    *
    * In C++ the only way to implement finally is to create a class
    * with a destructor, so that's what we do here.
    */
   class int_incrementer
   {
      public:
         int_incrementer( int32_t& target ) : _target(target)
         { ++_target; }

         int_incrementer( int_incrementer& ii ) : _target( ii._target )
         { ++_target; }

         ~int_incrementer()
         { --_target; }

         int32_t get()const
         { return _target; }

      private:
         int32_t& _target;
   };

   class abstract_session {
      public:
         virtual ~abstract_session(){};
         virtual void push()             = 0;
         virtual void squash()           = 0;
         virtual void undo()             = 0;
         virtual int64_t revision()const  = 0;
   };

   template<typename SessionType>
   class session_impl : public abstract_session
   {
      public:
         session_impl( SessionType&& s ):_session( std::move( s ) ){}

         virtual void push() override  { _session.push();  }
         virtual void squash() override{ _session.squash(); }
         virtual void undo() override  { _session.undo();  }
         virtual int64_t revision()const override  { return _session.revision();  }
      private:
         SessionType _session;
   };

   class index_extension
   {
      public:
         index_extension() {}
         virtual ~index_extension() {}
   };

   typedef std::vector< std::shared_ptr< index_extension > > index_extensions;

   class abstract_index
   {
      public:
         typedef helpers::index_statistic_info statistic_info;

         abstract_index( void* i ):_idx_ptr(i){}
         virtual ~abstract_index(){}

         virtual unique_ptr<abstract_session> start_undo_session() = 0;

         virtual void    undo()const = 0;
         virtual void    squash()const = 0;
         virtual void    commit( int64_t revision )const = 0;
         virtual void    undo_all()const = 0;
         virtual uint32_t type_id()const  = 0;

         virtual int64_t revision()const = 0;
         virtual void    set_revision( int64_t revision ) = 0;
//         virtual int64_t next_id()const = 0;
//         virtual void    set_next_id( int64_t next_id ) = 0;

//         virtual statistic_info get_statistics(bool onlyStaticInfo) const = 0;
//         virtual size_t size() const = 0;
         virtual void clear() = 0;
         virtual void open( const bfs::path&, const boost::any& ) = 0;
         virtual void close() = 0;
         virtual void wipe( const bfs::path& dir ) = 0;
         virtual void flush() = 0;
         virtual size_t get_cache_usage() const = 0;
         virtual size_t get_cache_size() const = 0;
         virtual void dump_lb_call_counts() = 0;
         virtual void trim_cache() = 0;
//         virtual void print_stats() const = 0;
//         virtual void begin_bulk_load() = 0;
//         virtual void end_bulk_load() = 0;
//         virtual void flush_bulk_load() = 0;

         void add_index_extension( std::shared_ptr< index_extension > ext )  { _extensions.push_back( ext ); }
         const index_extensions& get_index_extensions()const  { return _extensions; }
         void* get()const { return _idx_ptr; }
      protected:
         void*              _idx_ptr;
      private:
         index_extensions   _extensions;
   };

   template<typename BaseIndex>
   class index_impl : public abstract_index {
      public:
         using abstract_index::statistic_info;

         index_impl( BaseIndex& base ):abstract_index( &base ),_base(base){}

         ~index_impl()
         {
            delete (BaseIndex*) abstract_index::_idx_ptr;
         }

         virtual unique_ptr<abstract_session> start_undo_session() override {
            return unique_ptr<abstract_session>(new session_impl<typename BaseIndex::session>( _base.start_undo_session() ) );
         }

         virtual void     undo()const  override { _base.undo(); }
         virtual void     squash()const  override { _base.squash(); }
         virtual void     commit( int64_t revision )const  override { _base.commit(revision); }
         virtual void     undo_all() const override {_base.undo_all(); }
         virtual uint32_t type_id()const override { return BaseIndex::value_type::type_id; }

         virtual int64_t  revision()const  override { return _base.revision(); }
         virtual void     set_revision( int64_t revision ) override { _base.set_revision( revision ); }
//         virtual int64_t  next_id()const override { return _base.next_id(); }
//         virtual void     set_next_id( int64_t next_id ) override { _base.set_next_id( next_id ); }

//         virtual statistic_info get_statistics(bool onlyStaticInfo) const override final
//         {
//            typedef typename BaseIndex::index_type index_type;
//            helpers::index_statistic_provider<index_type> provider;
//            return provider.gather_statistics(_base.indices(), onlyStaticInfo);
//         }

//         virtual size_t size() const override final
//         {
//            return _base.indicies().size();
//         }

         virtual void clear() override final
         {
            _base.clear();
         }

         virtual void open( const bfs::path& p, const boost::any& o ) override final
         {
            _base.open( p, o );
         }

         virtual void close() override final
         {
            _base.close();
         }

         virtual void wipe( const bfs::path& dir ) override final
         {
            _base.wipe( dir );
         }

         virtual void flush() override final
         {
            _base.flush();
         }

         virtual size_t get_cache_usage() const override final
         {
            return _base.get_cache_usage();
         }

         virtual size_t get_cache_size() const override final
         {
            return _base.get_cache_size();
         }

         virtual void dump_lb_call_counts() override final
         {
            _base.dump_lb_call_counts();
         }

         virtual void trim_cache() override final
         {
            _base.trim_cache();
         }

//         virtual void print_stats() const override final
//         {
//            _base.indicies().print_stats();
//         }
//
//         virtual void begin_bulk_load() override final
//         {
//            _base.mutable_indices().begin_bulk_load();
//         }
//
//         virtual void end_bulk_load() override final
//         {
//            _base.mutable_indices().end_bulk_load();
//         }
//
//         virtual void flush_bulk_load() override final
//         {
//            _base.mutable_indices().flush_bulk_load();
//         }

      private:
         BaseIndex& _base;
   };

   template<typename IndexType>
   class index : public index_impl<IndexType> {
      public:
         index( IndexType& i ):index_impl<IndexType>( i ){}
   };


   class read_write_mutex_manager
   {
      public:
         read_write_mutex_manager()
         {
            _current_lock = 0;
         }

         ~read_write_mutex_manager(){}

         void next_lock()
         {
            _current_lock++;
            new( &_locks[ _current_lock % CHAINBASE_NUM_RW_LOCKS ] ) read_write_mutex();
         }

         read_write_mutex& current_lock()
         {
            return _locks[ _current_lock % CHAINBASE_NUM_RW_LOCKS ];
         }

         uint32_t current_lock_num()
         {
            return _current_lock;
         }

      private:
         std::array< read_write_mutex, CHAINBASE_NUM_RW_LOCKS >     _locks;
         std::atomic< uint32_t >                                    _current_lock;
   };

   struct lock_exception : public std::exception
   {
      explicit lock_exception() {}
      virtual ~lock_exception() {}

      virtual const char* what() const noexcept { return "Unable to acquire database lock"; }
   };

   /**
    *  This class
    */
   class database
   {
      private:
         class abstract_index_type
         {
            public:
               abstract_index_type() {}
               virtual ~abstract_index_type() {}

               virtual void add_index( database& db ) = 0;
         };

         template< typename IndexType >
         class index_type_impl : public abstract_index_type
         {
            virtual void add_index( database& db ) override
            {
               db.add_index_helper< IndexType >();
            }
         };

      public:
         void open( const bfs::path& dir, uint32_t flags = 0, const boost::any& database_cfg = nullptr );
         void close();
         void flush();
         size_t get_cache_usage() const;
         size_t get_cache_size() const;
         void dump_lb_call_counts();
         void trim_cache();
         void wipe( const bfs::path& dir );
         void set_require_locking( bool enable_require_locking );

#ifdef CHAINBASE_CHECK_LOCKING
         void require_lock_fail( const char* method, const char* lock_type, const char* tname )const;

         void require_read_lock( const char* method, const char* tname )const
         {
            if( BOOST_UNLIKELY( _enable_require_locking & (_read_lock_count <= 0) ) )
               require_lock_fail(method, "read", tname);
         }

         void require_write_lock( const char* method, const char* tname )
         {
            if( BOOST_UNLIKELY( _enable_require_locking & (_write_lock_count <= 0) ) )
               require_lock_fail(method, "write", tname);
         }
#endif

         struct session {
            public:
               session( session&& s )
                  : _index_sessions( std::move(s._index_sessions) ),
                    _revision( s._revision ),
                    _session_incrementer( s._session_incrementer )
               {}

               session( vector<std::unique_ptr<abstract_session>>&& s, int32_t& session_count )
                  : _index_sessions( std::move(s) ), _session_incrementer( session_count )
               {
                  if( _index_sessions.size() )
                     _revision = _index_sessions[0]->revision();
               }

               ~session() {
                  undo();
               }

               void push()
               {
                  for( auto& i : _index_sessions ) i->push();
                  _index_sessions.clear();
               }

               void squash()
               {
                  for( auto& i : _index_sessions ) i->squash();
                  _index_sessions.clear();
               }

               void undo()
               {
                  for( auto& i : _index_sessions ) i->undo();
                  _index_sessions.clear();
               }

               int64_t revision()const { return _revision; }

            private:
               friend class database;

               vector< std::unique_ptr<abstract_session> > _index_sessions;
               int64_t _revision = -1;
               int_incrementer _session_incrementer;
         };

         session start_undo_session();

         int64_t revision()const {
             if( _index_list.size() == 0 ) return -1;
             return _index_list[0]->revision();
         }

         void undo();
         void squash();
         void commit( int64_t revision );
         void undo_all();


         void set_revision( int64_t revision )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK( "set_revision", int64_t );
             for( const auto& i : _index_list ) i->set_revision( revision );
         }

//         void print_stats()
//         {
//            for( const auto& i : _index_list )  i->print_stats();
//         }

         template<typename MultiIndexType>
         void add_index()
         {
            _index_types.push_back( unique_ptr< abstract_index_type >( new index_type_impl< MultiIndexType >() ) );
            _index_types.back()->add_index( *this );
         }

         unsigned long long get_total_system_memory() const
         {
#if !defined( __APPLE__ ) // OS X does not support _SC_AVPHYS_PAGES
            long pages = sysconf(_SC_AVPHYS_PAGES);
            long page_size = sysconf(_SC_PAGE_SIZE);
            return pages * page_size;
#else
            return 0;
#endif
         }

         size_t get_free_memory()const
         {
            return get_total_system_memory();
         }

         size_t get_max_memory()const
         {
            return _file_size;
         }

         template<typename MultiIndexType>
         bool has_index()const
         {
            CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            return _index_map.size() > index_type::value_type::type_id && _index_map[index_type::value_type::type_id];
         }

         template<typename MultiIndexType>
         const generic_index<MultiIndexType>& get_index()const
         {
            CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;

            if( !has_index< MultiIndexType >() )
            {
               std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
               BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
            }

            return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
         }

         template<typename MultiIndexType>
         void add_index_extension( std::shared_ptr< index_extension > ext )
         {
            typedef generic_index<MultiIndexType> index_type;

            if( !has_index< MultiIndexType >() )
            {
               std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
               BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
            }

            _index_map[index_type::value_type::type_id]->add_index_extension( ext );
         }

         template<typename MultiIndexType, typename ByIndex>
         typename generic_index< MultiIndexType >::template secondary_index< ByIndex > get_index() const
         {
            CHAINBASE_REQUIRE_READ_LOCK("get_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;

            if( !has_index< MultiIndexType >() )
            {
               std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
               BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
            }

            return index_type_ptr( _index_map[ index_type::value_type::type_id ]->get() )->template get_secondary_index< ByIndex >();
         }

         template<typename MultiIndexType>
         generic_index<MultiIndexType>& get_mutable_index()
         {
            CHAINBASE_REQUIRE_WRITE_LOCK("get_mutable_index", typename MultiIndexType::value_type);
            typedef generic_index<MultiIndexType> index_type;
            typedef index_type*                   index_type_ptr;

            if( !has_index< MultiIndexType >() )
            {
               std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );
               BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find index for " + type_name + " in database" ) );
            }

            return *index_type_ptr( _index_map[index_type::value_type::type_id]->get() );
         }

         template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
         const ObjectType* find( CompatibleKey&& key )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             return get_index< index_type >().template find< IndexedByType >( std::forward< CompatibleKey >( key ) );
         }

         template< typename ObjectType >
         const ObjectType* find( oid< ObjectType > key = oid< ObjectType >() ) const
         {
             CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             return get_index< index_type >().find( key );
         }

         template< typename ObjectType, typename IndexedByType, typename CompatibleKey >
         const ObjectType& get( CompatibleKey&& key )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("get", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             return get_index< index_type >().template get< IndexedByType >( std::forward< CompatibleKey >( key ) );
         }

         template< typename ObjectType >
         const ObjectType& get( const oid< ObjectType >& key = oid< ObjectType >() )const
         {
             CHAINBASE_REQUIRE_READ_LOCK("find", ObjectType);
             typedef typename get_index_type< ObjectType >::type index_type;
             return get_index< index_type >().get( key );
         }

         template<typename ObjectType, typename Modifier>
         void modify( const ObjectType& obj, Modifier&& m )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("modify", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             get_mutable_index<index_type>().modify( obj, m );
         }

         template<typename ObjectType>
         void remove( const ObjectType& obj )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("remove", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             return get_mutable_index<index_type>().remove( obj );
         }

         template<typename ObjectType, typename Constructor>
         const ObjectType& create( Constructor&& con )
         {
             CHAINBASE_REQUIRE_WRITE_LOCK("create", ObjectType);
             typedef typename get_index_type<ObjectType>::type index_type;
             return get_mutable_index<index_type>().emplace( std::forward<Constructor>(con) );
         }

         template< typename ObjectType >
         size_t count()const
         {
            return 0;
            //typedef typename get_index_type<ObjectType>::type index_type;
            //return get_index< index_type >().indices().size();
         }

         template< typename Lambda >
         auto with_read_lock( Lambda&& callback, uint64_t wait_micro = 1000000 ) -> decltype( (*(Lambda*)nullptr)() )
         {
            read_lock lock( _rw_manager.current_lock(), boost::defer_lock_t() );

#ifdef CHAINBASE_CHECK_LOCKING
            BOOST_ATTRIBUTE_UNUSED
            int_incrementer ii( _read_lock_count );
#endif

            if( !wait_micro )
            {
               lock.lock();
            }
            else
            {
               if( !lock.timed_lock( boost::posix_time::microsec_clock::universal_time() + boost::posix_time::microseconds( wait_micro ) ) )
                  BOOST_THROW_EXCEPTION( lock_exception() );
            }

            return callback();
         }

         template< typename Lambda >
         auto with_write_lock( Lambda&& callback, uint64_t wait_micro = 1000000 ) -> decltype( (*(Lambda*)nullptr)() )
         {
            write_lock lock( _rw_manager.current_lock(), boost::defer_lock_t() );
#ifdef CHAINBASE_CHECK_LOCKING
            BOOST_ATTRIBUTE_UNUSED
            int_incrementer ii( _write_lock_count );
#endif

#ifdef IS_TEST_NET
            if( wait_micro )
            {
               while( !lock.timed_lock( boost::posix_time::microsec_clock::universal_time() + boost::posix_time::microseconds( wait_micro ) ) )
               {
                  _rw_manager.next_lock();
                  std::cerr << "Lock timeout, moving to lock " << _rw_manager.current_lock_num() << std::endl;
                  lock = write_lock( _rw_manager.current_lock(), boost::defer_lock_t() );
               }
            }
            else
#endif
            {
               lock.lock();
            }

            return callback();
         }
/*
         template< typename Lambda >
         void bulk_load( Lambda&& callback )
         {
            for( auto& item : _index_list )
            {
               item->begin_bulk_load();
            }

            callback();

            for( auto& item : _index_list )
            {
               item->end_bulk_load();
            }
         }

         void flush_bulk_load()
         {
            for( auto& item : _index_list )
            {
               item->flush_bulk_load();
            }
         }
*/
         template< typename IndexExtensionType, typename Lambda >
         void for_each_index_extension( Lambda&& callback )const
         {
            for( const abstract_index* idx : _index_list )
            {
               const index_extensions& exts = idx->get_index_extensions();
               for( const std::shared_ptr< index_extension >& e : exts )
               {
                  std::shared_ptr< IndexExtensionType > e2 = std::dynamic_pointer_cast< IndexExtensionType >( e );
                  if( e2 )
                     callback( e2 );
               }
            }
         }

         typedef vector<abstract_index*> abstract_index_cntr_t;

         const abstract_index_cntr_t& get_abstract_index_cntr() const
            { return _index_list; }

      private:
         template<typename MultiIndexType>
         void add_index_helper() {
            const uint16_t type_id = generic_index<MultiIndexType>::value_type::type_id;
            typedef generic_index<MultiIndexType>          index_type;

            std::string type_name = boost::core::demangle( typeid( typename index_type::value_type ).name() );

            if( !( _index_map.size() <= type_id || _index_map[ type_id ] == nullptr ) ) {
               BOOST_THROW_EXCEPTION( std::logic_error( type_name + "::type_id is already in use" ) );
            }

            index_type* idx_ptr =  nullptr;
            idx_ptr = new index_type();

            if( type_id >= _index_map.size() )
               _index_map.resize( type_id + 1 );

            auto new_index = new index<index_type>( *idx_ptr );

            _index_map[ type_id ].reset( new_index );
            _index_list.push_back( new_index );

            if( _is_open ) new_index->open( _data_dir, _database_cfg );
         }

         read_write_mutex_manager                                    _rw_manager;

         /**
          * This is a sparse list of known indicies kept to accelerate creation of undo sessions
          */
         abstract_index_cntr_t                                       _index_list;

         /**
          * This is a full map (size 2^16) of all possible index designed for constant time lookup
          */
         vector<unique_ptr<abstract_index>>                          _index_map;

         vector<unique_ptr<abstract_index_type>>                     _index_types;

         bfs::path                                                   _data_dir;
#ifdef CHAINBASE_CHECK_LOCKING
         int32_t                                                     _read_lock_count = 0;
         int32_t                                                     _write_lock_count = 0;
         bool                                                        _enable_require_locking = false;
#endif

         bool                                                        _is_open = false;

         int32_t                                                     _undo_session_count = 0;
         size_t                                                      _file_size = 0;
         boost::any                                                  _database_cfg = nullptr;
   };

}  // namepsace chainbase
