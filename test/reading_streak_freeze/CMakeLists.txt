add_executable(ReadingStreakFreezeTest
  ReadingStreakFreezeTest.cpp
  ${REPO_ROOT}/src/activities/reader/ReadingStatsUtils.cpp
)

target_include_directories(ReadingStreakFreezeTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/activities/reader
)

target_link_libraries(ReadingStreakFreezeTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingStreakFreezeTest)
