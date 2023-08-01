cmake_minimum_required(VERSION 3.19)


project(Remill_Multi_Lift C CXX ASM)

set(CMAKE_CPP_COMPILER clang++-16)
set(CMAKE_CXX_STANDARD 11)

set(Dependencies_Path /home/lucas/lifting-bits-downloads/vcpkg_ubuntu-22.04_llvm-16_amd64/installed/x64-linux-rel)
set(XED_DIR ${Dependencies_Path}/share/xed)
set(glog_DIR ${Dependencies_Path}/share/glog)
#set(gflags_DIR /usr/include/gflags)
set(Z3_DIR ${Dependencies_Path}/share/z3)
#set(remill_DIR /home/lucas/remill-build)
set(LLVM_DIR ${Dependencies_Path}/share/llvm)

find_package(remill CONFIG REQUIRED NO_CMAKE_FIND_ROOT_PATH)

find_package(sleigh CONFIG REQUIRED)
find_package(remill CONFIG REQUIRED PATHS /home/lucas/remill-build)

list(APPEND REMILL_LIBRARIES remill_settings remill)
message(STATUS "remill found at: ${remill_settings_DIR}")
get_target_property(REMILL_INCLUDE_LOCATION remill_settings INTERFACE_INCLUDE_DIRECTORIES)

#find_package(gflags REQUIRED)
#message(STATUS "gflags found at: ${gflags_DIR}")
message(STATUS "Remill libraries: ${REMILL_LIBRARIES}")


add_library(REMILL INTERFACE)
include_directories(${Dependencies_Path}/include /usr/local/include/sleigh /usr/include/gflags)
link_directories(${Dependencies_Path}/lib usr/local/lib)

#llvm
find_package(LLVM CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs Support Core IRReader bitreader bitwriter)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
message(STATUS "test")


add_executable(multi-lift remill-multi-lift.cpp)

target_link_libraries(multi-lift PRIVATE ${llvm_libs})
target_link_libraries(multi-lift PRIVATE gflags)

target_link_libraries(multi-lift PRIVATE ${REMILL_LIBRARIES})  
target_include_directories(multi-lift PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(REMILL SYSTEM INTERFACE ${REMILL_INCLUDE_LOCATION})

