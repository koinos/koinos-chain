file(GLOB HEADERS "include/koinos/mq/*.hpp")
add_library(koinos_mq
            message_broker.cpp
            ${HEADERS})

target_link_libraries(koinos_mq PUBLIC Koinos::exception Koinos::types rabbitmq)
target_include_directories(koinos_mq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/libraries/vendor/rabbitmq-c/librabbitmq)
add_library(Koinos::mq ALIAS koinos_mq)
