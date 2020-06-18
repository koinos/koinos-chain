find_package(Boost 1.57 REQUIRED)

set(KOINOS_PACK_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")

set(KOINOS_REFLECT_PYTHONPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../programs/koinos_reflect")

set(KOINOS_REFLECT_TEMPLATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../programs/koinos_reflect/lang")

set(KOINOS_REFLECT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(KOINOS_REFLECT_SOURCES
   "${KOINOS_REFLECT_SRC_DIR}/block.bt"
   "${KOINOS_REFLECT_SRC_DIR}/types.hpp"
   "${KOINOS_REFLECT_SRC_DIR}/block.hpp"
   "${KOINOS_REFLECT_SRC_DIR}/submit.hpp"
   "${KOINOS_REFLECT_SRC_DIR}/system_calls.hpp"
   "${KOINOS_REFLECT_SRC_DIR}/thunks.hpp"
   )

set(KOINOS_SCHEMA_DIR
   "${CMAKE_CURRENT_BINARY_DIR}/schema"
   )

set(KOINOS_SCHEMA_FILES
   "${KOINOS_SCHEMA_DIR}/block.schema"
   )

set(KOINOS_PACK_GEN_FILES
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/classes.hpp"

   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/basetypes.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/binary_fwd.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/binary.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/binary_serializer.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/exceptions.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/json_fwd.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/json.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/pack_fwd.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/reflect.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/string_fwd.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/string.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/typename.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/varint.hpp"

   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/util/base58.hpp"
   "${KOINOS_PACK_GEN_DIR}/koinos/pack/rt/util/variant_helpers.hpp"
   )

add_custom_target(mkdir_schema ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${KOINOS_SCHEMA_DIR})

add_custom_command(
   COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${KOINOS_REFLECT_PYTHONPATH}
   python3 -m koinos_reflect.analyze
   ${KOINOS_REFLECT_SOURCES}
   -s
   -o ${KOINOS_SCHEMA_DIR}/block.schema
   OUTPUT ${KOINOS_SCHEMA_DIR}/block.schema
   DEPENDS mkdir_schema ${KOINOS_REFLECT_SOURCES}
)

add_custom_command(
   COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${KOINOS_REFLECT_PYTHONPATH}
   python3 -m koinos_codegen.codegen
   --target-path "${KOINOS_REFLECT_TEMPLATE_DIR}"
   --target cpp
   -p koinos/pack
   -o "${KOINOS_PACK_GEN_DIR}"
   ${KOINOS_SCHEMA_FILES}
   OUTPUT ${KOINOS_PACK_GEN_FILES}
   DEPENDS ${KOINOS_SCHEMA_FILES}
)

set_source_files_properties(${KOINOS_PACK_GEN_FILES} PROPERTIES GENERATED TRUE)

# Unfortunately cmake doesn't seem to support interface libraries with generated header files,
# so we create a dummy .a file

add_library(koinos_pack ${KOINOS_PACK_GEN_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/pack.cpp
   )

target_link_libraries(koinos_pack INTERFACE koinos_exception nlohmann_json ${Boost_LIBRARIES} )
target_include_directories(koinos_pack INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/gen" "${CMAKE_CURRENT_BINARY_DIR}/gen" ${Boost_INCLUDE_DIR})
