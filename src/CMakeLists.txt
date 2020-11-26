# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# Get proto file
get_filename_component(coyote_proto "proto/coyote.proto" ABSOLUTE)
get_filename_component(coyote_proto_path "${coyote_proto}" PATH)

# Generated proto sources
set(coyote_proto_srcs "${PROJECT_SOURCE_DIR}/include/coyote/proto/coyote.pb.cc")
set(coyote_proto_hdrs "${PROJECT_SOURCE_DIR}/include/coyote/proto/coyote.pb.h")
set(coyote_grpc_srcs "${PROJECT_SOURCE_DIR}/include/coyote/proto/coyote.grpc.pb.cc")
set(coyote_grpc_hdrs "${PROJECT_SOURCE_DIR}/include/coyote/proto/coyote.grpc.pb.h")
add_custom_command(
    OUTPUT "${coyote_proto_srcs}" "${coyote_proto_hdrs}" "${coyote_grpc_srcs}" "${coyote_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${PROJECT_SOURCE_DIR}/include/coyote/proto"
        --cpp_out "${PROJECT_SOURCE_DIR}/include/coyote/proto"
        -I "${coyote_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${coyote_proto}"
    DEPENDS "${coyote_proto}")

include_directories("../include/coyote")

set(src_files
    "scheduler.cc"
    "settings.cc"
    "operations/operation.cc"
    "operations/operations.cc"
    "strategies/random.cc"
    "strategies/random_strategy.cc")

add_library(coyote SHARED ${src_files} ${coyote_proto_srcs} ${coyote_grpc_srcs})
target_link_libraries(coyote ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF})
set_target_properties(coyote PROPERTIES
    OUTPUT_NAME "coyote"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")
target_compile_definitions(coyote PUBLIC COYOTE_SHARED COYOTE_EXPORT)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(coyote PRIVATE COYOTE_DEBUG_LOG)
endif()

add_library(coyote_static STATIC ${src_files} ${coyote_proto_srcs} ${coyote_grpc_srcs})
target_link_libraries(coyote_static ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF})
set_target_properties(coyote_static PROPERTIES
    OUTPUT_NAME "coyote_static"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(coyote_static PRIVATE COYOTE_DEBUG_LOG)
endif()
