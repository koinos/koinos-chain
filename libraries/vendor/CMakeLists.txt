add_subdirectory(appbase)
add_subdirectory(builtins)
add_subdirectory(eos-vm)
add_subdirectory(koinos-types)
add_subdirectory(mira)
add_subdirectory(rabbitmq-c)
target_compile_options(rabbitmq PRIVATE -Wno-implicit-fallthrough)
target_compile_options(rabbitmq-static PRIVATE -Wno-implicit-fallthrough)

