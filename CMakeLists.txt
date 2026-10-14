cmake_minimum_required(VERSION 3.20)
project(PerfectWorldCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(PerfectWorldCore STATIC
    Source/PerfectWorldCore/Private/Appearance.cpp
    Source/PerfectWorldCore/Private/GameClock.cpp
    Source/PerfectWorldCore/Private/Condition.cpp
    Source/PerfectWorldCore/Private/CharacterCreation.cpp
    Source/PerfectWorldCore/Private/CreationPresets.cpp
    Source/PerfectWorldCore/Private/CreationEdits.cpp
    Source/PerfectWorldCore/Private/WeekCompressionPlan.cpp
    Source/PerfectWorldCore/Private/WeekCompressionRun.cpp
    Source/PerfectWorldCore/Private/WeekCompressionCompare.cpp
    Source/PerfectWorldCore/Private/CountdownContent.cpp
    Source/PerfectWorldCore/Private/CountdownRules.cpp
    Source/PerfectWorldCore/Private/CountdownProfile.cpp
    Source/PerfectWorldCore/Private/CountdownOffers.cpp
    Source/PerfectWorldCore/Private/CountdownRun.cpp
    Source/PerfectWorldCore/Private/CountdownText.cpp
    Source/PerfectWorldCore/Private/StateDiff.cpp
    Source/PerfectWorldCore/Private/StateDiffLines.cpp
    Source/PerfectWorldCore/Private/CreationPresentation.cpp
    Source/PerfectWorldCore/Private/StreamPresentation.cpp
    Source/PerfectWorldCore/Private/CreationSaveSerialization.cpp
    Source/PerfectWorldCore/Private/CreationIdentity.cpp
    Source/PerfectWorldCore/Private/CreationIdentityText.cpp
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
    Source/PerfectWorldCore/Private/WorldSave.cpp
    Source/PerfectWorldCore/Private/WorldSaveReader.cpp
    Source/PerfectWorldCore/Private/WorldDay.cpp
    Source/PerfectWorldCore/Private/AssetRegistry.cpp
    Source/PerfectWorldCore/Private/AssetReader.cpp
    Source/PerfectWorldCore/Private/AssetContractReader.cpp
    Source/PerfectWorldCore/Private/AssetGeometry.cpp
    Source/PerfectWorldCore/Private/AssetGameplay.cpp
    Source/PerfectWorldCore/Private/IdentityRegistry.cpp
    Source/PerfectWorldCore/Private/EventLog.cpp
    Source/PerfectWorldCore/Private/ContentFairness.cpp
    Source/PerfectWorldCore/Private/LatentStats.cpp
    Source/PerfectWorldCore/Private/ContentActivitySchemas.cpp
    Source/PerfectWorldCore/Private/Activities.cpp
    Source/PerfectWorldCore/Private/ActivityRunnerText.cpp
    Source/PerfectWorldCore/Private/Schedules.cpp
    Source/PerfectWorldCore/Private/SchedulePlan.cpp
    Source/PerfectWorldCore/Private/ScheduleText.cpp
    Source/PerfectWorldCore/Private/ContentProblemSchemas.cpp
    Source/PerfectWorldCore/Private/StructureModel.cpp
    Source/PerfectWorldCore/Private/StructureModelRun.cpp
    Source/PerfectWorldCore/Private/StructureModelText.cpp
    Source/PerfectWorldCore/Private/StructureCommon.cpp
    Source/PerfectWorldCore/Private/StructureExplain.cpp
    Source/PerfectWorldCore/Private/StructureExplainRun.cpp
    Source/PerfectWorldCore/Private/StructureExplainText.cpp
    Source/PerfectWorldCore/Private/StructureInvestigate.cpp
    Source/PerfectWorldCore/Private/StructureInvestigateRun.cpp
    Source/PerfectWorldCore/Private/StructureInvestigateText.cpp
    Source/PerfectWorldCore/Private/StructureCoordinate.cpp
    Source/PerfectWorldCore/Private/StructureCoordinateRun.cpp
    Source/PerfectWorldCore/Private/StructureCoordinateText.cpp
    Source/PerfectWorldCore/Private/ContentStructureSchemas.cpp
    Source/PerfectWorldCore/Private/ContentStructureCases.cpp
    Source/PerfectWorldCore/Private/ContentScheduleSchemas.cpp
    Source/PerfectWorldCore/Private/AttendanceBell.cpp
    Source/PerfectWorldCore/Private/Attendance.cpp
    Source/PerfectWorldCore/Private/AttendanceText.cpp
    Source/PerfectWorldCore/Private/PracticeWindow.cpp
    Source/PerfectWorldCore/Private/Relationships.cpp
    Source/PerfectWorldCore/Private/RelationshipsText.cpp
    Source/PerfectWorldCore/Private/ConductLedger.cpp
    Source/PerfectWorldCore/Private/ConductLedgerText.cpp
    Source/PerfectWorldCore/Private/OpportunityReceipts.cpp
    Source/PerfectWorldCore/Private/OpportunityReceiptsState.cpp
    Source/PerfectWorldCore/Private/OpportunityReceiptsText.cpp
    Source/PerfectWorldCore/Private/DayLoop.cpp
    Source/PerfectWorldCore/Private/DayLoopApply.cpp
    Source/PerfectWorldCore/Private/DayLoopText.cpp
    Source/PerfectWorldCore/Private/Checkpoints.cpp
    Source/PerfectWorldCore/Private/AptitudeProjection.cpp
    Source/PerfectWorldCore/Private/AptitudeProjectionProject.cpp
    Source/PerfectWorldCore/Private/AptitudeProjectionText.cpp
    Source/PerfectWorldCore/Private/ContentCalendarSchemas.cpp
    Source/PerfectWorldCore/Private/SchoolCalendar.cpp
    Source/PerfectWorldCore/Private/SchoolCalendarRules.cpp
    Source/PerfectWorldCore/Private/ContentAssessmentSchemas.cpp
    Source/PerfectWorldCore/Private/Assessment.cpp
    Source/PerfectWorldCore/Private/AssessmentRun.cpp
    Source/PerfectWorldCore/Private/AssessmentText.cpp
    Source/PerfectWorldCore/Private/ContentAbilitySchemas.cpp
    Source/PerfectWorldCore/Private/AwakeningSignals.cpp
    Source/PerfectWorldCore/Private/AwakeningResolve.cpp
    Source/PerfectWorldCore/Private/AwakeningRecipes.cpp
    Source/PerfectWorldCore/Private/AwakeningProfile.cpp
    Source/PerfectWorldCore/Private/Tower.cpp
    Source/PerfectWorldCore/Private/ContentThreadSchemas.cpp
    Source/PerfectWorldCore/Private/ThreadTemplates.cpp
    Source/PerfectWorldCore/Private/ThreadTemplateRules.cpp
    Source/PerfectWorldCore/Private/Threads.cpp
    Source/PerfectWorldCore/Private/ThreadsRun.cpp
    Source/PerfectWorldCore/Private/ThreadsText.cpp
    Source/PerfectWorldCore/Private/TimeContractRules.cpp
    Source/PerfectWorldCore/Private/TimeContractResolve.cpp
    Source/PerfectWorldCore/Private/TimeContractText.cpp
    Source/PerfectWorldCore/Private/TimeContractContent.cpp
    Source/PerfectWorldCore/Private/TimeContractEquivalence.cpp
    Source/PerfectWorldCore/Private/Doors.cpp
    Source/PerfectWorldCore/Private/Conversation.cpp
    Source/PerfectWorldCore/Private/ConversationContent.cpp
    Source/PerfectWorldCore/Private/ConversationText.cpp
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

add_executable(PerfectWorldCreationPresetTests Tests/CreationPresetTests.cpp)
target_link_libraries(PerfectWorldCreationPresetTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldCreationPresetTests COMMAND PerfectWorldCreationPresetTests)

add_executable(PerfectWorldCreationPresentationTests Tests/CreationPresentationTests.cpp)
target_link_libraries(PerfectWorldCreationPresentationTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldCreationPresentationTests COMMAND PerfectWorldCreationPresentationTests)

add_executable(PerfectWorldStreamPresentationTests Tests/StreamPresentationTests.cpp)
target_link_libraries(PerfectWorldStreamPresentationTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldStreamPresentationTests COMMAND PerfectWorldStreamPresentationTests)

add_executable(PerfectWorldCreationIdentityTests Tests/CreationIdentityTests.cpp)
target_link_libraries(PerfectWorldCreationIdentityTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldCreationIdentityTests COMMAND PerfectWorldCreationIdentityTests)

add_executable(PerfectWorldClockTests Tests/GameClockTests.cpp)
target_link_libraries(PerfectWorldClockTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldClockTests COMMAND PerfectWorldClockTests)

add_executable(PerfectWorldScheduleTests Tests/SchedulesTests.cpp Tests/SchedulesProgressTests.cpp)
target_link_libraries(PerfectWorldScheduleTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldScheduleTests COMMAND PerfectWorldScheduleTests)

add_executable(PerfectWorldConditionTests Tests/ConditionTests.cpp)
target_link_libraries(PerfectWorldConditionTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldConditionTests COMMAND PerfectWorldConditionTests)

add_executable(PerfectWorldIdentityTests Tests/IdentityRegistryTests.cpp Tests/EventLogTests.cpp)
target_link_libraries(PerfectWorldIdentityTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldIdentityTests COMMAND PerfectWorldIdentityTests)

add_executable(PerfectWorldLatentStatsTests Tests/LatentStatsTests.cpp)
target_link_libraries(PerfectWorldLatentStatsTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldLatentStatsTests COMMAND PerfectWorldLatentStatsTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldActivitiesTests Tests/ActivitiesTests.cpp Tests/ActivitiesContentTests.cpp)
target_link_libraries(PerfectWorldActivitiesTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldActivitiesTests COMMAND PerfectWorldActivitiesTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldStructureModelTests Tests/StructureModelTests.cpp Tests/StructureModelFlowTests.cpp Tests/StructureModelContentTests.cpp)
target_link_libraries(PerfectWorldStructureModelTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldStructureModelTests COMMAND PerfectWorldStructureModelTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldStructureOthersTests Tests/StructureOthersTests.cpp Tests/StructureExplainTests.cpp Tests/StructureInvestigateTests.cpp
               Tests/StructureCoordinateTests.cpp Tests/StructureOthersContentTests.cpp Tests/StructureDistinctTests.cpp)
target_link_libraries(PerfectWorldStructureOthersTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldStructureOthersTests COMMAND PerfectWorldStructureOthersTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldAttendanceTests Tests/AttendanceTests.cpp Tests/AttendanceRoutineTests.cpp Tests/AttendanceRecordTests.cpp Tests/AttendanceTextTests.cpp Tests/AttendanceCalendarTests.cpp)
target_link_libraries(PerfectWorldAttendanceTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAttendanceTests COMMAND PerfectWorldAttendanceTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldPracticeTests Tests/PracticeWindowTests.cpp)
target_link_libraries(PerfectWorldPracticeTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldPracticeTests COMMAND PerfectWorldPracticeTests)

add_executable(PerfectWorldRelationshipTests Tests/RelationshipsTests.cpp Tests/RelationshipsSaveTests.cpp)
target_link_libraries(PerfectWorldRelationshipTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldRelationshipTests COMMAND PerfectWorldRelationshipTests)

add_executable(PerfectWorldConductTests Tests/ConductLedgerTests.cpp)
target_link_libraries(PerfectWorldConductTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldConductTests COMMAND PerfectWorldConductTests)

add_executable(PerfectWorldReceiptTests Tests/OpportunityReceiptsTests.cpp Tests/OpportunityReceiptsTextTests.cpp)
target_link_libraries(PerfectWorldReceiptTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldReceiptTests COMMAND PerfectWorldReceiptTests)

add_executable(PerfectWorldDayLoopTests Tests/DayLoopTests.cpp Tests/DayLoopRefusalTests.cpp Tests/DayLoopSaveTests.cpp Tests/DayLoopRandomTests.cpp Tests/DayLoopCalendarTests.cpp Tests/DayLoopWorldTests.cpp)
target_link_libraries(PerfectWorldDayLoopTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldDayLoopTests COMMAND PerfectWorldDayLoopTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldAptitudeTests Tests/AptitudeProjectionTests.cpp Tests/AptitudeProjectionTextTests.cpp)
target_link_libraries(PerfectWorldAptitudeTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAptitudeTests COMMAND PerfectWorldAptitudeTests)

add_executable(PerfectWorldSchoolCalendarTests Tests/SchoolCalendarTests.cpp Tests/SchoolCalendarContentTests.cpp)
target_link_libraries(PerfectWorldSchoolCalendarTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldSchoolCalendarTests COMMAND PerfectWorldSchoolCalendarTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldAssessmentTests Tests/AssessmentTests.cpp Tests/AssessmentMarkingTests.cpp Tests/AssessmentSaveTests.cpp
               Tests/AssessmentContentTests.cpp)
target_link_libraries(PerfectWorldAssessmentTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAssessmentTests COMMAND PerfectWorldAssessmentTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldAwakeningTests Tests/AwakeningTests.cpp Tests/AwakeningPersonaTests.cpp Tests/AwakeningProfileTests.cpp)
target_link_libraries(PerfectWorldAwakeningTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAwakeningTests COMMAND PerfectWorldAwakeningTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldTowerTests Tests/TowerTests.cpp)
target_link_libraries(PerfectWorldTowerTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldTowerTests COMMAND PerfectWorldTowerTests)

add_executable(PerfectWorldThreadTests Tests/ThreadsTests.cpp Tests/ThreadsContentTests.cpp Tests/ThreadsRunTests.cpp
               Tests/ThreadsEquivalenceTests.cpp Tests/ThreadsSaveTests.cpp)
target_link_libraries(PerfectWorldThreadTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldThreadTests COMMAND PerfectWorldThreadTests ${CMAKE_SOURCE_DIR}/Data/Content)

add_executable(PerfectWorldTimeContractTests Tests/TimeContractTests.cpp Tests/TimeContractResolveTests.cpp
               Tests/TimeContractContentTests.cpp Tests/TimeContractEquivalenceTests.cpp)
target_link_libraries(PerfectWorldTimeContractTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldTimeContractTests COMMAND PerfectWorldTimeContractTests ${CMAKE_SOURCE_DIR}/Data/Content)

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

add_executable(PerfectWorldWorldSaveTests Tests/WorldSaveTests.cpp Tests/WorldSaveDayTests.cpp)
target_link_libraries(PerfectWorldWorldSaveTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldWorldSaveTests COMMAND PerfectWorldWorldSaveTests)

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

add_executable(PerfectWorldAssetTests Tests/AssetRegistryTests.cpp)
target_link_libraries(PerfectWorldAssetTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldAssetTests COMMAND PerfectWorldAssetTests)

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
        "bad_prefix|must start with"
        "bad_fairness_religion|keys a modifier on religion"
        "bad_fairness_language|keys a modifier on language"
        "bad_fairness_stream|keys a modifier on stream"
        "bad_fairness_family|keys a modifier on family"
        "bad_skill_seed|which is not a creation trait"
        "bad_attribute_set|the attribute set is missing"
        "bad_activity_split|sums to 900 parts per thousand, not exactly 1000"
        "bad_activity_skill|which does not exist"
        "bad_activity_duration|outside 1..1440"
        "bad_problem_few_cases|needs at least 8 cases, found 2"
        "bad_problem_duplicate_variation|repeats the unknown, role and scarce resource of problem_case.fixture_a"
        "bad_problem_template|wants exactly 1 stock quantity, found 2"
        "bad_problem_unknown_kind|the unknown quantity is a inflow, but the case says its unknown is a stock"
        "bad_problem_method|is not one of the family's methods"
        "bad_problem_role|its role presenter is not a role the family allows"
        "bad_problem_link|sound link l_flow_adds must join an inflow (adds)")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# The asset registry (ROADMAP M1-07): the real contracts and assets must pass,
# and each bad fixture must fail for the reason it was written to demonstrate.
add_test(NAME PerfectWorldAssetData COMMAND pwcontent assets ${CMAKE_SOURCE_DIR}/Data)
foreach(fixture_and_issue
        "bad_anchor_outside|lies outside the outer envelope"
        "bad_missing_clearance|missing required field"
        "bad_door_height|must be exactly 2100 mm"
        "bad_unknown_family|unknown family"
        "bad_duplicate_id|duplicate ID"
        "bad_unknown_field|unknown field")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 issue)
    add_test(NAME PerfectWorldAssetFixture_${fixture}
             COMMAND pwcontent assets ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Assets/${fixture} --expect-issue "${issue}")
endforeach()

# Bell schedules (ROADMAP M2-07): each fixture directory carries several bad schedules, one per rule,
# so an entry is "directory|test name|issue text" and every issue text is unique to its rule.
foreach(fixture_and_issue
        "bad_bell_schedule|overlap|overlaps block 2"
        "bad_bell_schedule|bell_order|the bells are not in ascending order"
        "bad_bell_schedule|subject|which is not taught in the humanities stream"
        "bad_bell_schedule|hours|after the school hours end"
        "bad_bell_streams|shared_bells|one school rings one set of bells"
        "bad_bell_streams|duplicate_stream|a second science schedule"
        "bad_bell_streams|missing_stream|has no schedule for the commerce stream")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 rule)
    list(GET parts 2 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}_${rule}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# The other three structures (ROADMAP M2-09): each fixture directory carries several bad records, one per rule,
# so an entry is "directory|test name|issue text" and every issue text is unique to its rule.
foreach(fixture_and_issue
        "bad_explain_cases|unsupported|the defensible claim c_a needs at least 2 supporting evidence items, found 1"
        "bad_explain_cases|unbounded|the defensible claim c_a has no limitation that bounds it"
        "bad_explain_cases|no_indefensible|needs at least one indefensible claim"
        "bad_explain_cases|dangling|evidence e_a1 names the claim c_nope, which the case does not have"
        "bad_explain_cases|duplicate_role|the family's roles names author twice"
        "bad_investigate_cases|two_faults|exactly one cause must be the fault, found 2"
        "bad_investigate_cases|all_causes|test t_3 must implicate some, but not all, of the causes"
        "bad_investigate_cases|same_cost|the tests all cost the same"
        "bad_investigate_cases|unseparable|no test tells the cause c_a from c_b"
        "bad_coordinate_cases|no_self|exactly one member must be the player's own (self), found 0"
        "bad_coordinate_cases|player_fact|the critical fact f_1 is held by the player"
        "bad_coordinate_cases|no_critical|needs at least one critical fact"
        "bad_coordinate_cases|no_assignment|no assignment keeps every member within capacity"
        "bad_coordinate_cases|many_holders|members holding a critical fact, found 2"
        "bad_structure_records|explain_beats|the explain structure's beats must be claim, evidence, limitation, in that order"
        "bad_structure_records|investigate_decisions|the investigate structure's decisions must be run_test, diagnose, report_inconclusive"
        "bad_structure_records|coordinate_period|the coordinate structure's period is 9 units of 5 minutes"
        "bad_structure_links|explain_engine|needs a structure whose engine is explain, but its structure's engine is coordinate"
        "bad_structure_links|investigate_engine|needs a structure whose engine is investigate, but its structure's engine is explain"
        "bad_structure_links|coordinate_engine|needs a structure whose engine is coordinate, but its structure's engine is investigate"
        "bad_structure_links|small_classroom_family|a classroom explain family declares at least 3 cases"
        "bad_structure_links|few_cases|needs at least 4 cases, found 0")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 rule)
    list(GET parts 2 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}_${rule}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# The 2042-43 school calendar (ROADMAP M3-09): one fixture directory carries a calendar per rule, so an entry is
# "directory|test name|issue text" and every issue text is unique to its rule.
foreach(fixture_and_issue
        "bad_calendar|date_missing|is not a date that exists"
        "bad_calendar|date_format|is not written YYYY-MM-DD"
        "bad_calendar|term_overlap|term term_2 overlaps term term_1"
        "bad_calendar|range_order|holiday durga_puja ends before it starts"
        "bad_calendar|outside_window|lies outside the calendar's window"
        "bad_calendar|gap|2042-11-03 is in neither a term nor a break"
        "bad_calendar|holiday_unexplained|holiday durga_puja (2042-10-20 to 2042-11-04) is not wholly inside a term or a break"
        "bad_calendar|break_straddle|break puja_vacation straddles the edge of term term_1"
        "bad_calendar|assessment_term|assessment unit_test_1 (2042-07-28 to 2042-08-01) is not inside its term term_2"
        "bad_calendar|sports_weekend|event sports_day on 2042-12-06 is not on a school day"
        "bad_calendar|missing_independence|the calendar must declare the holiday independence_day on 2042-08-15"
        "bad_calendar|ten_days_school|the ten days after the Awakening have no school, but 2043-02-02 is a school day"
        "bad_calendar|approximate_note|festival durga_puja has an approximate date"
        "bad_calendar|exam_gap|the examination must be exactly 10 days after the Awakening"
        "bad_calendar|not_a_school|the institution institution.nga is not a school"
        # Assessment papers (ROADMAP M3-07): one paper per rule.
        "bad_assessment_paper|units|a paper is 90 minutes: 18 units of 5 minutes"
        "bad_assessment_paper|groups|a paper has 5 question groups, found 4"
        "bad_assessment_paper|marks|group g1 is worth 10 marks, not 20"
        "bad_assessment_paper|duplicate_group|the group id g1 is used twice"
        "bad_assessment_paper|missing_case|group g1 names the case problem_case.xi_no_such, which does not exist"
        "bad_assessment_paper|case_type|a calendar, but its structure model needs a problem_case"
        "bad_assessment_paper|repeat_case|group g2 repeats the case problem_case.a"
        "bad_assessment_paper|window|has no assessment window quarterly")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 rule)
    list(GET parts 2 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}_${rule}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# Ability recipes (ROADMAP M4-02): one fixture directory carries a recipe per rule, so an entry is
# "directory|test name|issue text" and every issue text is unique to its rule.
foreach(fixture_and_issue
        "bad_ability_recipe|fallback_asks|a fallback recipe asks for no evidence"
        "bad_ability_recipe|none_asked|an evidence recipe needs 1 to 3 requirements, found 0"
        "bad_ability_recipe|four_asked|an evidence recipe needs 1 to 3 requirements, found 4"
        "bad_ability_recipe|twice|requires the signal bodywork twice"
        "bad_ability_shape|unknown_signal|has an unknown value"
        "bad_ability_shape|min_range|is 2000, outside 1..1000"
        "bad_ability_shape|unknown_field|in ability_recipe"
        "bad_ability_set|two_fallbacks|the tattva agni has 2 fallback recipes"
        "bad_ability_set|no_fallback|the tattva vayu has 0 fallback recipes"
        "bad_ability_set|conduct_only|the tattva jala has no evidence recipe a student who never entered the dungeon can earn"
        "bad_ability_set|no_evidence|the tattva akasha has no evidence recipe")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 rule)
    list(GET parts 2 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}_${rule}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# Thread templates (ROADMAP M3-06): one fixture directory carries a template per rule (and the calendar they name), so an
# entry is "directory|test name|issue text" and every issue text is unique to its rule.
foreach(fixture_and_issue
        "bad_thread|intensity|stage s2 intensity 400 does not rise above 600"
        "bad_thread|clock|the clock runs 300 days in total, over the limit of 200"
        "bad_thread|few_stages|needs 3 to 6 stages, found 2"
        "bad_thread|scene_first|the first stage must not be a scene: a thread begins quietly"
        "bad_thread|default_repaired|the default resolution mended is repaired: an untouched thread does not mend itself"
        "bad_thread|unreachable|resolution mended is never reached: no trigger resolves it and it is not the default"
        "bad_thread|no_agency|no event trigger eases or resolves the thread, so nothing the player does can change its course"
        "bad_thread|role_unused|role bystander is never used by an expression or a condition"
        "bad_thread|calendar_span|names the calendar span 'pongal', which calendar.nabapara_2042_43 does not have"
        "bad_thread|fair_role|role religion_peer is keyed on 'religion', which no thread may depend on"
        "bad_thread|fair_event|event type 'spoke_same_language' is keyed on 'language', which no thread may depend on"
        "bad_thread|unknown_stage|armedFrom 's7' is not a stage of the template"
        "bad_thread|trigger_role|names the role 'stranger', which is not a role of the template")
    string(REPLACE "|" ";" parts "${fixture_and_issue}")
    list(GET parts 0 fixture)
    list(GET parts 1 rule)
    list(GET parts 2 issue)
    add_test(NAME PerfectWorldContentFixture_${fixture}_${rule}
             COMMAND pwcontent validate ${CMAKE_SOURCE_DIR}/Tests/Fixtures/Content/${fixture} --expect-issue "${issue}")
endforeach()

# Week compression and the persistent-state diff (ROADMAP M3-02). The tool is built first and its path is
# the test's second argument, so the test can run the real command line.
add_executable(pwstatediff Tools/StateDiff/main.cpp)
target_link_libraries(pwstatediff PRIVATE PerfectWorldCore)
add_executable(PerfectWorldWeekTests Tests/WeekCompressionTests.cpp Tests/WeekCompressionRefusalTests.cpp
               Tests/WeekCompressionRulesTests.cpp Tests/WeekCompressionRandomTests.cpp Tests/StateDiffTests.cpp
               Tests/StateDiffMutationTests.cpp Tests/StateDiffSaveTests.cpp)
target_link_libraries(PerfectWorldWeekTests PRIVATE PerfectWorldCore)
add_dependencies(PerfectWorldWeekTests pwstatediff)
add_test(NAME PerfectWorldWeekTests COMMAND PerfectWorldWeekTests ${CMAKE_SOURCE_DIR}/Data/Content $<TARGET_FILE:pwstatediff>)

# Doors and conversation: the portable rules the first enterable home and the first talkable adult need
# (LANE_LIFE_IMPLEMENTATION stage 3 rows 3.1/3.3 and stage 5 rows 5.1/5.2, minimal slice).
add_executable(PerfectWorldDoorsTests Tests/DoorsTests.cpp)
target_link_libraries(PerfectWorldDoorsTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldDoorsTests COMMAND PerfectWorldDoorsTests)

add_executable(PerfectWorldConversationTests Tests/ConversationTests.cpp)
target_link_libraries(PerfectWorldConversationTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldConversationTests COMMAND PerfectWorldConversationTests)

# The ten days (ROADMAP M4-03).
add_executable(PerfectWorldCountdownTests Tests/CountdownTests.cpp Tests/CountdownRulesTests.cpp Tests/CountdownObligationTests.cpp
               Tests/CountdownReloadTests.cpp Tests/CountdownStrategyTests.cpp)
target_link_libraries(PerfectWorldCountdownTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldCountdownTests COMMAND PerfectWorldCountdownTests ${CMAKE_SOURCE_DIR}/Data/Content)

# Lane origin binding (order O); integrator may move the source into the core library list.
add_executable(PerfectWorldLaneOriginTests Tests/LaneOriginTests.cpp Source/PerfectWorldCore/Private/LaneOrigin.cpp)
target_link_libraries(PerfectWorldLaneOriginTests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorldLaneOriginTests COMMAND PerfectWorldLaneOriginTests)
