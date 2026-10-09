idf_component_register(SRCS "main.cc" INCLUDE_DIRS "." REQUIRES board audio)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../models/wakeword/hello_robot_v1/hello_robot_v1.tflite"
               "${CMAKE_CURRENT_BINARY_DIR}/hello_robot_v1.tflite" COPYONLY)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/hello_robot_v1.tflite" BINARY)
