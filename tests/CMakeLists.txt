add_executable(cppzmqzoltanext_Tests
    UTestPoller.cpp
    UTestLoop.cpp
    UTestActor.cpp
    utils.h
)

target_link_libraries(cppzmqzoltanext_Tests
    PRIVATE
        cppzmqzoltanext::cppzmqzoltanext
        cppzmq
        GTest::gtest_main
        GTest::gmock_main
)

if (WIN32)
    # https://stackoverflow.com/questions/69978314/cmake-with-gtest-on-windows-build-starts-test-but-shared-libs-cannot-be-found
    add_custom_command(
        TARGET cppzmqzoltanext_Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:cppzmqzoltanext_Tests> $<TARGET_FILE_DIR:cppzmqzoltanext_Tests>
        COMMAND_EXPAND_LISTS
    )
endif()

include(GoogleTest)
gtest_discover_tests(cppzmqzoltanext_Tests DISCOVERY_MODE PRE_TEST)
