# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(myNpmLib)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(midiplayer SHARED napi_init.cpp types/player/MidiEngine.h types/player/MidiEngine.cpp types/tsf/tsf_impl.cpp)
target_link_libraries(midiplayer PUBLIC libace_napi.z.so libohaudio.so libhilog_ndk.z.so)
