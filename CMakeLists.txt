cmake_minimum_required(VERSION 3.4)

project(libObfuscator)

if(NOT DEFINED ENV{LLVM_HOME})
    message(FATAL_ERROR "LLVM_HOME is not defined")
endif()

if(NOT DEFINED ENV{LLVM_DIR})
    set(ENV{LLVM_DIR} $ENV{LLVM_HOME}/lib/cmake/llvm)
endif()

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(
    ${LLVM_INCLUDE_DIRS}
    include
)
link_directories(${LLVM_LIBRARY_DIRS})

add_library(libObfuscator MODULE
    src/obfuscator.cpp
    src/CryptoUtils.cpp
)