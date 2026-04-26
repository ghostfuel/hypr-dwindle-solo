cmake_minimum_required(VERSION 3.19)
project(hypr-dwindle-solo
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SHARED_MODULE_PREFIX "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET hyprland pixman-1 libdrm)

add_library(hypr-dwindle-solo SHARED
    main.cpp
    soloCenter.cpp
)

target_include_directories(hypr-dwindle-solo PRIVATE ${DEPS_INCLUDE_DIRS})
target_link_directories(hypr-dwindle-solo PRIVATE ${DEPS_LIBRARY_DIRS})
target_compile_options(hypr-dwindle-solo PRIVATE -fPIC --no-gnu-unique -g)
