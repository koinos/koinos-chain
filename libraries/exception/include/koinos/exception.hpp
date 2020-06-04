
#pragma once

#include <strpolate/strpolate.hpp>

#define KOINOS_THROW( exc_name, msg, ... ) \
   throw exc_name( STRPOL( msg, __VA_ARGS__("file", __FILE__)("line", __LINE__)("exc", #exc_name) ) )

#define KOINOS_ASSERT( cond, exc_name, msg, ... )     \
   do {                                             \
      if( !(cond) )                                 \
      {                                             \
         KOINOS_THROW( exc_name, msg, __VA_ARGS__ );  \
      }                                             \
   } while (0)

#define DECLARE_KOINOS_EXCEPTION( exc_name )                     \
   class exc_name : public koinos::exception                     \
   {                                                             \
      public:                                                    \
         exc_name() {}                                           \
         exc_name( const strpolate::strpol& strpol )             \
            : exception(strpol) {}                               \
         virtual ~exc_name() {}                                  \
                                                                 \
         static void get_exception_name(std::string& result)     \
         {  result = #exc_name;           }                      \
   }

#define DECLARE_DERIVED_KOINOS_EXCEPTION( exc_name, base )       \
   class exc_name : public base                                  \
   {                                                             \
      public:                                                    \
         exc_name() {}                                           \
         exc_name( const strpolate::strpol& strpol )             \
            : base(strpol) {}                                    \
         virtual ~exc_name() {}                                  \
                                                                 \
         static void get_exception_name(std::string& result)     \
         {  result = #exc_name;           }                      \
   }

namespace koinos {

class exception
{
   public:
      exception();
      exception( const strpolate::strpol& strpol );
      virtual ~exception();

      virtual void to_string(std::string& result)const;
      virtual std::string to_string()const;

      strpolate::strpol _strpol;
};

} // koinos
