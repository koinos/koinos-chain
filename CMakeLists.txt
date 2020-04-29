
cmake_minimum_required (VERSION 3.10.2)
project( koinos )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(Boost_NO_BOOST_CMAKE ON)
if( NOT( Boost_VERSION LESS 106900 ) )
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
   SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()

add_subdirectory(programs)
add_subdirectory(libraries)
add_subdirectory(tests)
