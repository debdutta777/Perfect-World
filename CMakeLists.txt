cmake_minimum_required(VERSION 3.20)
project(PerfectWorldCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(PerfectWorldCore STATIC
    Source/PerfectWorldCore/Private/CharacterCreation.cpp
)
target_include_directories(PerfectWorldCore PUBLIC Source/PerfectWorldCore/Public)
target_compile_features(PerfectWorldCore PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(PerfectWorldCore PRIVATE /W4 /permissive-)
else()
    target_compile_options(PerfectWorldCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(PerfectWorldCoreTests Tests/CharacterCreationTests.cpp)
target_link_libraries(PerfectWorldCoreTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldCoreTests COMMAND PerfectWorldCoreTests)
