hunter_config(Boost
   VERSION ${HUNTER_Boost_VERSION}
   CMAKE_ARGS
   USE_CONFIG_FROM_BOOST=ON
   Boost_USE_STATIC_LIBS=ON
   Boost_NO_BOOST_CMAKE=ON
)

hunter_config(rocksdb
   VERSION ${HUNTER_rocksdb_VERSION}
   CMAKE_ARGS
      WITH_TESTS=OFF
      WITH_TOOLS=OFF
      WITH_JNI=OFF
      WITH_BENCHMARK_TOOLS=OFF
      WITH_CORE_TOOLS=OFF
      WITH_GFLAGS=OFF
      PORTABLE=ON
      FAIL_ON_WARNINGS=OFF
      ROCKSDB_BUILD_SHARED=OFF
      CMAKE_CXX_FLAGS=-fvisibility=hidden
      CMAKE_C_FLAGS=-fvisibility=hidden
)

hunter_config(rabbitmq-c
   VERSION 0.10.0-773b883-t1
   CMAKE_ARGS
      ENABLE_SSL_SUPPORT=OFF
      CMAKE_C_FLAGS=-Wno-implicit-fallthrough
      CMAKE_CXX_FLAGS=-Wno-implicit-fallthrough
)

hunter_config(koinos_log
   GIT_SUBMODULE "libraries/log"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)

hunter_config(koinos_util
   GIT_SUBMODULE "libraries/util"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)

hunter_config(koinos_types
   GIT_SUBMODULE "libraries/types"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)

hunter_config(koinos_exception
   GIT_SUBMODULE "libraries/exception"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)

hunter_config(koinos_crypto
   GIT_SUBMODULE "libraries/crypto"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)

hunter_config(koinos_mq
   GIT_SUBMODULE "libraries/mq"
   CMAKE_ARGS
      BUILD_TESTS=OFF
)
