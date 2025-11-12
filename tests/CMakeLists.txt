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
)

include(GoogleTest)
gtest_discover_tests(cppzmqzoltanext_Tests)
