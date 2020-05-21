find_package(Boost 1.57 REQUIRED)

set(KOINOS_PACK_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")

#${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../../programs/koinos_reflect
execute_process(
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../programs/koinos_reflect
   COMMAND python3 -m koinos_reflect.analyze block.bt types.hpp block.hpp submit.hpp -s -o ${CMAKE_CURRENT_BINARY_DIR}/block.schema
)

execute_process(
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../programs/koinos_reflect
   COMMAND python3 -m koinos_codegen.codegen --target-path lang --target cpp -p koinos/pack
   -o ${KOINOS_PACK_GEN_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}/block.schema
)

file(GLOB HEADERS "${CMAKE_CURRENT_BINARY_DIR}/gen/koinos/pack/*.hpp" "${CMAKE_CURRENT_BINARY_DIR}/gen/koinos/pack/rt/*.hpp" "${CMAKE_CURRENT_BINARY_DIR}/gen/koinos/pack/rt/util/*.hpp")
add_library(pack INTERFACE)
target_sources(pack INTERFACE ${HEADERS})

set_source_files_properties(${HEADERS} PROPERTIES GENERATED TRUE)

target_link_libraries(pack INTERFACE koinos_exception nlohmann_json ${Boost_LIBRARIES} )
target_include_directories(pack INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/gen" "${CMAKE_CURRENT_BINARY_DIR}/gen" ${Boost_INCLUDE_DIR})
