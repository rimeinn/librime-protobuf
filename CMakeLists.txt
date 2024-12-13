cmake_minimum_required(VERSION 3.10)
project(rime-protobuf)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${PROJECT_SOURCE_DIR})

find_package(Protobuf REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/lib)

# search path for protobuf schemata: *.proto
set(RIME_PROTO_SCHEMA_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
# search path for generated capnp binding headers: *.pb.h
set(RIME_PROTO_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)

protobuf_generate_cpp(
  protobuf_gen_srcs
  protobuf_gen_hdrs
  rime_proto.proto
)

message(STATUS "compiled protobuf schemata (sources): ${protobuf_gen_srcs}")
message(STATUS "compiled protobuf schemata (headers): ${protobuf_gen_hdrs}")

aux_source_directory(src proto_src)

add_library(rime-proto-objs OBJECT ${protobuf_gen_srcs} ${proto_src})
target_include_directories(rime-proto-objs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if(BUILD_SHARED_LIBS)
  set_target_properties(rime-proto-objs
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
endif()

add_library(rime-proto-deps INTERFACE)
target_link_libraries(rime-proto-deps INTERFACE protobuf::libprotobuf)

set(plugin_name rime-proto PARENT_SCOPE)
set(plugin_objs $<TARGET_OBJECTS:rime-proto-objs> PARENT_SCOPE)
set(plugin_deps ${rime_library} rime-proto-deps PARENT_SCOPE)
set(plugin_modules "proto" PARENT_SCOPE)

# msvc doesn't export all symbols
if(NOT (WIN32 AND BUILD_SHARED_LIBS))
  add_executable(rime_proto_console "rime_proto_console.cc" $<TARGET_OBJECTS:rime-proto-objs>)
  target_link_libraries(rime_proto_console ${rime_library} rime-proto-deps)
  target_include_directories(rime_proto_console PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()
