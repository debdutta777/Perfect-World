cmake_minimum_required(VERSION 3.20)
project(PerfectWorldCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(PerfectWorldCore STATIC
    Source/PerfectWorldCore/Private/Appearance.cpp
    Source/PerfectWorldCore/Private/GameClock.cpp
    Source/PerfectWorldCore/Private/CharacterCreation.cpp
    Source/PerfectWorldCore/Private/CreationSaveSerialization.cpp
    Source/PerfectWorldCore/Private/RealmCoordinate.cpp
    Source/PerfectWorldCore/Private/AwakenedProgression.cpp
    Source/PerfectWorldCore/Private/Json.cpp
    Source/PerfectWorldCore/Private/ContentRegistry.cpp
    Source/PerfectWorldCore/Private/ContentSchemas.cpp
    Source/PerfectWorldCore/Private/RealmGenerator.cpp
    Source/PerfectWorldCore/Private/WorldReducer.cpp
    Source/PerfectWorldCore/Private/WorldHash.cpp
    Source/PerfectWorldCore/Private/SaveStore.cpp
    Source/PerfectWorldCore/Private/GateSites.cpp
    Source/PerfectWorldCore/Private/RandomStreams.cpp
    Source/PerfectWorldCore/Private/ChunkDeltaStore.cpp
    Source/PerfectWorldCore/Private/DistrictGenerator.cpp
    Source/PerfectWorldCore/Private/DistrictLayouts.cpp
    Source/PerfectWorldCore/Private/DistrictLayoutsLand.cpp
    Source/PerfectWorldCore/Private/CityData.cpp
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

add_executable(PerfectWorldClockTests Tests/GameClockTests.cpp)
target_link_libraries(PerfectWorldClockTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldClockTests COMMAND PerfectWorldClockTests)

add_executable(PerfectWorldRealmTests Tests/RealmCoordinateTests.cpp)
target_link_libraries(PerfectWorldRealmTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldRealmTests COMMAND PerfectWorldRealmTests)

add_executable(PerfectWorldRealmGeneratorTests Tests/RealmGeneratorTests.cpp)
target_link_libraries(PerfectWorldRealmGeneratorTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldRealmGeneratorTests COMMAND PerfectWorldRealmGeneratorTests)

add_executable(PerfectWorldReducerTests Tests/WorldReducerTests.cpp)
target_link_libraries(PerfectWorldReducerTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldReducerTests COMMAND PerfectWorldReducerTests)

add_executable(PerfectWorldWorldHashTests Tests/WorldHashTests.cpp)
target_link_libraries(PerfectWorldWorldHashTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldWorldHashTests COMMAND PerfectWorldWorldHashTests)

add_executable(PerfectWorldSaveStoreTests Tests/SaveStoreTests.cpp)
target_link_libraries(PerfectWorldSaveStoreTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldSaveStoreTests COMMAND PerfectWorldSaveStoreTests)

add_executable(PerfectWorldGateTests Tests/GateSitesTests.cpp)
target_link_libraries(PerfectWorldGateTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldGateTests COMMAND PerfectWorldGateTests)

add_executable(PerfectWorldRandomTests Tests/RandomStreamsTests.cpp)
target_link_libraries(PerfectWorldRandomTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldRandomTests COMMAND PerfectWorldRandomTests)

add_executable(PerfectWorldChunkDeltaTests Tests/ChunkDeltaStoreTests.cpp)
target_link_libraries(PerfectWorldChunkDeltaTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldChunkDeltaTests COMMAND PerfectWorldChunkDeltaTests)

add_executable(PerfectWorldDistrictTests Tests/DistrictGeneratorTests.cpp)
target_link_libraries(PerfectWorldDistrictTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldDistrictTests COMMAND PerfectWorldDistrictTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldAwakenedTests Tests/AwakenedProgressionTests.cpp)
target_link_libraries(PerfectWorldAwakenedTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAwakenedTests COMMAND PerfectWorldAwakenedTests)

add_executable(PerfectWorldContentTests Tests/ContentRegistryTests.cpp)
target_link_libraries(PerfectWorldContentTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldContentTests COMMAND PerfectWorldContentTests)

# The content validator tool (ROADMAP M1-06): the real data must pass, and each
# bad fixture must fail for the reason it was written to demonstrate.
add_executable(pwcontent Tools/ContentCheck/main.cpp)
target_link_libraries(pwcontent PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldContentData COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Data/Content)
foreach(fixture_and_issue
        "bad_duplicate_id|duplicate ID"
        "bad_dangling_ref|which does not exist"
        "bad_unknown_field|unknown field"
        "bad_unit|must be a whole number"
        "bad_range|outside 0..4"
        "bad_semantic|share a householdResourcesRank"
        "bad_json|not valid JSON"
        "bad_prefix|must start with")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()
