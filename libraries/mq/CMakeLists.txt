file(GLOB HEADERS "include/koinos/mq/*.hpp")
add_library(koinos_mq
            message_broker.cpp
            ${HEADERS})

target_link_libraries(koinos_mq Koinos::exception Koinos::types rabbitmq)
target_include_directories(koinos_mq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::mq ALIAS koinos_mq)
