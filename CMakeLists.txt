cmake_minimum_required(VERSION 3.31)
project(tinytris C)
set(CMAKE_C_STANDARD 23)
add_executable(tinytris main.c)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
target_link_libraries(tinytris PRIVATE SDL3::SDL3)
target_link_libraries(tinytris PRIVATE $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>)

configure_file(block.bmp block.bmp COPYONLY)