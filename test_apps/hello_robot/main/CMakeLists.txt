idf_component_register(SRCS "main.cc" INCLUDE_DIRS ".")
# Copy to a simple basename so ESP-IDF emits stable _binary_... linker symbols.
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../models/wakeword/hello_robot_v1/hello_robot_v1.tflite"
               "${CMAKE_CURRENT_BINARY_DIR}/hello_robot_v1.tflite" COPYONLY)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/hello_robot_v1.tflite" BINARY)
