cmake_minimum_required(VERSION 3.16)
project(booksim2_interface)

file(GLOB_RECURSE BOOKSIM_SRCS
    CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(booksim2 STATIC ${BOOKSIM_SRCS})

target_include_directories(booksim2 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/allocators
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/arbiters
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/power
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/routers
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/networks
)
target_compile_options(booksim2 PRIVATE -Wall -O3)
