cmake_minimum_required(VERSION 3.21)
project(HydroEdit VERSION 5.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

enable_testing()
add_subdirectory(src/hidrcore)
add_subdirectory(src/app)
add_subdirectory(tests)
