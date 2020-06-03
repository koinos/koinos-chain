file(GLOB HEADERS "include/strpolate/*.hpp")
add_library(koinos_strpolate ${HEADERS} strpolate.cpp)
target_include_directories(koinos_strpolate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
