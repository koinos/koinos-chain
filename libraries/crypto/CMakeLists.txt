find_package(OpenSSL REQUIRED)

file(GLOB HEADERS "include/koinos/crypto/*.hpp" )
add_library( koinos_crypto
             multihash.cpp
             ${HEADERS} )
target_link_libraries( koinos_crypto koinos_exception pack secp256k1 ${OPENSSL_LIBRARIES} )
target_include_directories( koinos_crypto
   PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${OPENSSL_INCLUDE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/secp256k1/include
       )
add_dependencies(koinos_crypto project_secp256k1)
