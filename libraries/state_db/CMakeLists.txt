file(GLOB HEADERS
     "include/koinos/state_db/*.hpp"
     "include/koinos/state_db/detail/*.hpp"
     "include/koinos/state_db/backends/*.hpp"
     "include/koinos/state_db/backends/rocksdb/*.hpp")
add_library(koinos_state_db
            state_db.cpp
            detail/state_delta.cpp
            detail/merge_iterator.cpp
            backends/backend.cpp
            backends/iterator.cpp
            backends/map/map_backend.cpp
            backends/map/map_iterator.cpp
            backends/rocksdb/rocksdb_backend.cpp
            backends/rocksdb/rocksdb_iterator.cpp
            backends/rocksdb/object_cache.cpp
            ${HEADERS} )
target_link_libraries(koinos_state_db Koinos::exception Koinos::proto Koinos::crypto mira)
target_include_directories(koinos_state_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::state_db ALIAS koinos_state_db)
