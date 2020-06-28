#pragma once

#include <koinos/pack/rt/exceptions.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <type_traits>

namespace koinos::pack {

template< typename... T > struct get_typename;

/**
 *  @brief defines visit functions for T
 *  Unless this is specialized, visit() will not be defined for T.
 *
 *  @tparam T - the type that will be visited.
 *
 *  The @ref KOINOS_REFLECT(TYPE,MEMBERS) or KOINOS_STATIC_REFLECT_DERIVED(TYPE,BASES,MEMBERS) macro is used to specialize this
 *  class for your type.
 */
template<typename T>
struct reflector{
   typedef T type;
   typedef std::false_type is_defined;
   typedef std::false_type is_enum;

    /**
     *  @tparam Visitor a function object of the form:
     *
     *    @code
     *     struct functor {
     *        template<typename Member, class Class, Member (Class::*member)>
     *        void operator()( const char* name )const;
     *     };
     *    @endcode
     *
     *  If T is an enum then the functor has the following form:
     *    @code
     *     struct functor {
     *        template<int Value>
     *        void operator()( const char* name )const;
     *     };
     *    @endcode
     *
     *  @param v a functor that will be called for each member on T
     *
     *  @note - this method is not defined for non-reflected types.
     */
// template<typename Visitor>
// static inline void visit( const Visitor& v );
};

void inline throw_bad_enum_cast( int64_t i, const char* e )
{
  throw bad_cast_exception( "invalid enum index" );
}

void inline throw_bad_enum_cast( const char* k, const char* e )
{
   throw bad_cast_exception( "invalid enum index" );
}

} // namespace koinos::pack

#define KOINOS_REFLECT_VISIT_BASE(r, visitor, base) \
   koinos::pack::reflector<base>::visit( visitor );

#define KOINOS_REFLECT_VISIT_MEMBER( r, visitor, elem ) \
{ typedef decltype(((type*)nullptr)->elem) member_type;  \
   visitor.template operator()<member_type,type,&type::elem>( BOOST_PP_STRINGIZE(elem) ); \
}

#define KOINOS_REFLECT_TUPLE_BASE(r, obj, i, base) \
   BOOST_PP_COMMA_IF(i) reflector<base>::make_tuple(obj)

#define KOINOS_REFLECT_TUPLE_MEMBER(r, obj, i, member) \
   BOOST_PP_COMMA_IF(i) std::cref(obj.member)

#define KOINOS_REFLECT_BASE_MEMBER_COUNT( r, OP, elem ) \
   OP koinos::pack::reflector<elem>::total_member_count

#define KOINOS_REFLECT_MEMBER_COUNT( r, OP, elem ) \
   OP 1

#define KOINOS_REFLECT_DERIVED_IMPL_INLINE( TYPE, INHERITS, MEMBERS ) \
template<typename Visitor>\
static inline void visit( const Visitor& v ) { \
   BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_VISIT_BASE, v, INHERITS ) \
   BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_VISIT_MEMBER, v, MEMBERS ) \
}\
static auto make_tuple( const TYPE& t ) \
{ \
   return std::tuple_cat( \
      std::tuple_cat( \
         BOOST_PP_SEQ_FOR_EACH_I( KOINOS_REFLECT_TUPLE_BASE, t, INHERITS ) \
      ), \
      std::make_tuple( \
         BOOST_PP_SEQ_FOR_EACH_I( KOINOS_REFLECT_TUPLE_MEMBER, t, MEMBERS ) \
      ) \
   ); \
} \

/**
 *  @def KOINOS_REFLECT_DERIVED(TYPE,INHERITS,MEMBERS)
 *
 *  @brief Specializes koinos::pack::reflector for TYPE where
 *         type inherits other reflected classes
 *
 *  @param INHERITS - a sequence of base class names (basea)(baseb)(basec)
 *  @param MEMBERS - a sequence of member names.  (field1)(field2)(field3)
 */
#define KOINOS_REFLECT_DERIVED( TYPE, INHERITS, MEMBERS ) \
namespace koinos::pack {  \
template<> struct get_typename<TYPE>  { static const char* name()  { return BOOST_PP_STRINGIZE(TYPE);  } }; \
template<> struct reflector<TYPE> {\
   typedef TYPE type; \
   typedef std::true_type  is_defined; \
   typedef std::false_type is_enum; \
   enum  member_count_enum {  \
      local_member_count = 0  BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_MEMBER_COUNT, +, MEMBERS ),\
      total_member_count = local_member_count BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_BASE_MEMBER_COUNT, +, INHERITS )\
   }; \
   KOINOS_REFLECT_DERIVED_IMPL_INLINE( TYPE, INHERITS, MEMBERS ) \
}; }

/**
 *  @def KOINOS_REFLECT(TYPE,MEMBERS)
 *  @brief Specializes koinos::pack::reflector for TYPE
 *
 *  @param MEMBERS - a sequence of member names.  (field1)(field2)(field3)
 *
 *  @see KOINOS_REFLECT_DERIVED
 */
#define KOINOS_REFLECT( TYPE, MEMBERS ) \
   KOINOS_REFLECT_DERIVED( TYPE, BOOST_PP_SEQ_NIL, MEMBERS )

/*
 * Enum Reflection
 */

#define KOINOS_REFLECT_VISIT_ENUM( r, enum_type, elem ) \
  v.operator()(BOOST_PP_STRINGIZE(elem), int64_t(enum_type::elem) );
#define KOINOS_REFLECT_ENUM_TO_STRING( r, enum_type, elem ) \
   case enum_type::elem: return BOOST_PP_STRINGIZE(elem);
#define KOINOS_REFLECT_ENUM_TO_KOINOS_STRING( r, enum_type, elem ) \
   case enum_type::elem: return std::string(BOOST_PP_STRINGIZE(elem));

#define KOINOS_REFLECT_ENUM_FROM_STRING( r, enum_type, elem ) \
  if( strcmp( s, BOOST_PP_STRINGIZE(elem)  ) == 0 ) return enum_type::elem;
#define KOINOS_REFLECT_ENUM_FROM_STRING_CASE( r, enum_type, elem ) \
   case enum_type::elem:

#define KOINOS_REFLECT_ENUM( ENUM, FIELDS ) \
namespace koinos::pack { \
template<> struct reflector<ENUM> { \
    typedef std::true_type is_defined; \
    typedef std::true_type is_enum; \
    static const char* to_string(ENUM elem) { \
      switch( elem ) { \
        BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_ENUM_TO_STRING, ENUM, FIELDS ) \
        default: \
           koinos::pack::throw_bad_enum_cast( std::to_string(int64_t(elem)).c_str(), BOOST_PP_STRINGIZE(ENUM) ); \
      }\
      return nullptr; \
    } \
    static const char* to_string(int64_t i) { \
      return to_string(ENUM(i)); \
    } \
    static ENUM from_int(int64_t i) { \
      ENUM e = ENUM(i); \
      switch( e ) \
      { \
        BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_ENUM_FROM_STRING_CASE, ENUM, FIELDS ) \
          break; \
        default: \
          koinos::pack::throw_bad_enum_cast( i, BOOST_PP_STRINGIZE(ENUM) ); \
      } \
      return e;\
    } \
    static ENUM from_string( const char* s ) { \
        BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_ENUM_FROM_STRING, ENUM, FIELDS ) \
        int64_t i = 0; \
        try \
        { \
           i = boost::lexical_cast<int64_t>(s); \
        } \
        catch( const boost::bad_lexical_cast& e ) \
        { \
           koinos::pack::throw_bad_enum_cast( s, BOOST_PP_STRINGIZE(ENUM) ); \
        } \
        return from_int(i); \
    } \
    template< typename Visitor > \
    static void visit( const Visitor& v ) \
    { \
        BOOST_PP_SEQ_FOR_EACH( KOINOS_REFLECT_VISIT_ENUM, ENUM, FIELDS ) \
    } \
};  \
template<> struct get_typename<ENUM>  { static const char* name()  { return BOOST_PP_STRINGIZE(ENUM);  } }; \
}

