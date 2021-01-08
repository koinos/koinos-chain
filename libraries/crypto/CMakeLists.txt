find_package(OpenSSL REQUIRED)
find_package(secp256k1 REQUIRED)

file(GLOB HEADERS "include/koinos/crypto/*.hpp")
add_library(koinos_crypto
            elliptic.cpp
            multihash.cpp
            openssl.cpp
            ${HEADERS})
target_link_libraries(koinos_crypto Koinos::exception Koinos::types OpenSSL::SSL OpenSSL::Crypto secp256k1 ${CMAKE_DL_LIBS})
target_include_directories(koinos_crypto
   PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${OPENSSL_INCLUDE_DIR})
add_library(Koinos::crypto ALIAS koinos_crypto)

