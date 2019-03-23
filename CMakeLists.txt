cmake_minimum_required(VERSION 3.2)
project(grpc_helloworld)

find_package(Protobuf REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

set(PROTO_PATH "${CMAKE_SOURCE_DIR}")
set(HELLOWORLD_PROTO "${CMAKE_SOURCE_DIR}/hello.proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

set(HELLOWORLD_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/hello.pb.cc")
set(HELLOWORLD_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/hello.pb.h")
set(HELLOWORLD_GRPC_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/hello.grpc.pb.cc")
set(HELLOWORLD_GRPC_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/hello.grpc.pb.h")
add_custom_command(
    OUTPUT "${HELLOWORLD_PB_H_FILE}"
           "${HELLOWORLD_PB_CPP_FILE}"
           "${HELLOWORLD_GRPC_PB_H_FILE}"
           "${HELLOWORLD_GRPC_PB_CPP_FILE}"
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--cpp_out=${GENERATED_PROTOBUF_PATH}"
         "${HELLOWORLD_PROTO}"
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--grpc_out=${GENERATED_PROTOBUF_PATH}"
         "--plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin"
         "${HELLOWORLD_PROTO}"
    )
set(GENERATED_PROTOBUF_FILES ${HELLOWORLD_PB_H_FILE} ${HELLOWORLD_PB_CPP_FILE}
    ${HELLOWORLD_GRPC_PB_H_FILE} ${HELLOWORLD_GRPC_PB_CPP_FILE})

include_directories(${GENERATED_PROTOBUF_PATH} /usr/local/include/ /usr/include/zookeeper)
link_libraries(grpc++_unsecure grpc gpr ${PROTOBUF_LIBRARY} conservator-framework zookeeper_mt /usr/lib/x86_64-linux-gnu /usr/local/lib/)

add_executable(master master.cc ${GENERATED_PROTOBUF_FILES})
add_executable(worker worker.cc ${GENERATED_PROTOBUF_FILES})
