
#pragma once

#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>

#include <nlohmann/json.hpp>

#include <koinos/log.hpp>
#include <koinos/pack/rt/json.hpp>

#define _DETAIL_KOINOS_INIT_VA_ARGS( ... ) init __VA_ARGS__

#define KOINOS_THROW( exception, msg, ... ) \
do {  \
   nlohmann::json j; \
   koinos::detail::json_initializer init(j);\
   _DETAIL_KOINOS_INIT_VA_ARGS( __VA_ARGS__ ); \
   BOOST_THROW_EXCEPTION( \
      exception( msg ) \
      << koinos::detail::json_info( std::move(j) ) \
      << koinos::detail::exception_stacktrace( boost::stacktrace::stacktrace() ) \
   ); \
} while(0)

#define KOINOS_ASSERT( cond, exc_name, msg, ... )     \
   do {                                               \
      if( !(cond) )                                   \
      {                                               \
         KOINOS_THROW( exc_name, msg, __VA_ARGS__ );  \
      }                                               \
   } while (0)

#define KOINOS_CAPTURE_CATCH_AND_RETHROW( ... )                                        \
catch( std::exception& e )                                                             \
{                                                                                      \
   nlohmann::json* maybe_json = boost::get_error_info< koinos::detail::json_info >(e); \
   if( maybe_json )                                                                    \
   {                                                                                   \
      koinos::detail::json_initializer init(*maybe_json);                              \
      _DETAIL_KOINOS_INIT_VA_ARGS( __VA_ARGS__ );                                      \
   }                                                                                   \
                                                                                       \
   throw;                                                                              \
}

#define KOINOS_CATCH_LOG_AND_RETHROW( log_level )           \
catch( std::exception& e )                                  \
{                                                           \
   LOG( log_level ) << boost::diagnostic_information( e );  \
   throw;                                                   \
}

#define KOINOS_CATCH_AND_LOG( log_level )                   \
catch( std::exception& e )                                  \
{                                                           \
   LOG( log_level ) << boost::diagnostic_information( e );  \
}

#define KOINOS_CATCH_AND_GET_JSON( j )                                                 \
catch( std::exception& e )                                                             \
{                                                                                      \
   nlohmann::json* maybe_json = boost::get_error_info< koinos::detail::json_info >(e); \
   if( maybe_json ) j = *maybe_json;                                                   \
}

#define KOINOS_CATCH_LOG_AND_GET_JSON( log_level, j )                                  \
catch( std::exception& e )                                                             \
{                                                                                      \
   LOG( log_level ) << boost::diagnostic_information( e );                             \
   nlohmann::json* maybe_json = boost::get_error_info< koinos::detail::json_info >(e); \
   if( maybe_json ) j = *maybe_json;                                                   \
}

#define KOINOS_DECLARE_EXCEPTION( exc_name )                         \
   struct exc_name : public koinos::exception                        \
   {                                                                 \
      exc_name() {}                                                  \
      exc_name( const std::string& m ) : koinos::exception( m ) {}   \
      exc_name( std::string&& m ) : koinos::exception( m ) {}        \
                                                                     \
      virtual ~exc_name() {};                                        \
   };

#define KOINOS_DECLARE_DERIVED_EXCEPTION( exc_name, base )  \
   struct exc_name : public base                            \
   {                                                        \
      exc_name() {}                                         \
      exc_name( const std::string& m ) : base( m ) {}       \
      exc_name( std::string&& m ) : base( m ) {}            \
                                                            \
      virtual ~exc_name() {};                               \
   };

namespace koinos { namespace detail {

using json_info = boost::error_info< struct json_tag, nlohmann::json >;
using exception_stacktrace = boost::error_info< struct stacktrace_tag, boost::stacktrace::stacktrace >;

std::string json_strpolate( const std::string& format_str, const nlohmann::json& j );

/**
 * Initializes a json object using a bubble list of key value pairs.
 */
struct json_initializer
{
   nlohmann::json& _j;

   json_initializer() = delete;
   json_initializer( nlohmann::json& j );

   template< typename T >
   json_initializer& operator()( const std::string& key, T&& t )
   {
      koinos::pack::to_json( _j[key], t );
      return *this;
   }

   template< typename T >
   json_initializer& operator()( const std::string& key, const T& t )
   {
      koinos::pack::to_json( _j[key], t );
      return *this;
   }

   json_initializer& operator()();
};

} // detail

struct exception : virtual boost::exception, virtual std::exception
{
   private:
      std::string msg;

   public:
      exception();
      exception( const std::string& m );
      exception( std::string&& m );

      virtual ~exception();

      virtual const char* what() const noexcept override;

      std::string get_stacktrace() const;
      const nlohmann::json& get_json() const;
};

} // koinos
