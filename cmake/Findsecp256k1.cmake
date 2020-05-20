include(ExternalProject)

set(prefix "${CMAKE_BINARY_DIR}/deps/secp256k1")
set(SECP256K1_LIBRARY "${prefix}/src/project_secp256k1-build/.libs/libsecp256k1${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SECP256K1_SOURCE_DIR "${CMAKE_SOURCE_DIR}/libraries/vendor/secp256k1")

ExternalProject_Add( project_secp256k1
   PREFIX ${prefix}
   SOURCE_DIR ${SECP256K1_SOURCE_DIR}
   CONFIGURE_COMMAND ${SECP256K1_SOURCE_DIR}/configure --prefix=${prefix} --with-bignum=no --enable-module-recovery
   BUILD_COMMAND make
   INSTALL_COMMAND true
   BUILD_BYPRODUCTS ${SECP256K1_LIBRARY}
)
ExternalProject_Add_Step(project_secp256k1 autogen
   WORKING_DIRECTORY ${SECP256K1_SOURCE_DIR}
   COMMAND ${SECP256K1_SOURCE_DIR}/autogen.sh
   DEPENDERS configure
)

add_library(secp256k1 STATIC IMPORTED)
set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${SECP256K1_LIBRARY})
set_property(TARGET secp256k1 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SECP256K1_SOURCE_DIR}/include)
add_dependencies(secp256k1 project_secp256k1)
