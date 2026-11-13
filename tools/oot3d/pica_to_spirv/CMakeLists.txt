cmake_minimum_required(VERSION 3.22)
project(pica_to_spirv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pica_to_spirv STATIC
    spirv_builder.h
    spirv_builder.cpp
    pica_vertex_translator.h
    pica_vertex_translator.cpp
    pica_fragment_translator.h
    pica_fragment_translator.cpp
)

target_include_directories(pica_to_spirv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/azahar_pica
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/three_ds_recomp/include
)

target_compile_features(pica_to_spirv PUBLIC cxx_std_20)

add_executable(pica_to_spirv_tests
    pica_to_spirv_tests.cpp
)

target_link_libraries(pica_to_spirv_tests PRIVATE pica_to_spirv)

enable_testing()
add_test(NAME pica_to_spirv_tests COMMAND pica_to_spirv_tests)

