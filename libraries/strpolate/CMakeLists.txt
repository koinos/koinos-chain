file(GLOB HEADERS "include/strpolate/*.hpp")
add_library(strpolate ${HEADERS} strpolate.cpp)
target_include_directories(strpolate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
