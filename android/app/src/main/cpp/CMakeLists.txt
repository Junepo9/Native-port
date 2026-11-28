cmake_minimum_required(VERSION 3.16)
project(oot3d_port_android CXX)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

add_subdirectory(${REPO_ROOT}/external/SDL2 SDL2-build)

add_library(main SHARED ${REPO_ROOT}/src/main.cpp)
target_link_libraries(main PRIVATE SDL2::SDL2)
if(TARGET SDL2::SDL2main)
    target_link_libraries(main PRIVATE SDL2::SDL2main)
endif()
