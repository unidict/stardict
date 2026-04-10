# Unity Test Framework
set(UNITY_SOURCES unity/unity.c)
set(UNITY_HEADERS unity/unity.h unity/unity_internals.h)

# Test sources
set(TEST_SOURCES
    test_fixture.c
    main.c
    test_array.c
    test_dictzip.c
    test_stardict.c
    test_dictd.c
)

# Create test executable
add_executable(stardict_tests
    ${UNITY_SOURCES}
    ${TEST_SOURCES}
)

target_include_directories(stardict_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/unity
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(stardict_tests PRIVATE stardict ZLIB::ZLIB)

# Enable testing
add_test(NAME UnityTestSuite COMMAND stardict_tests)
