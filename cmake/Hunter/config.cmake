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
   URL https://github.com/alanxz/rabbitmq-c/archive/773b883175ae50db024d59c7cce85c73e3f47e67.tar.gz
   SHA1 eabbd8d179e49849a47c3c58e1900e6a8610d18d
   CMAKE_ARGS
      CMAKE_C_FLAGS=-Wno-implicit-fallthrough
      CMAKE_CXX_FLAGS=-Wno-implicit-fallthrough
)

