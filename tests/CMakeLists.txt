add_executable(CppZmqZoltanExt_Tests
    UTestPoller.cpp
    UTestLoop.cpp
    UTestActor.cpp
    utils.h
)

target_link_libraries(CppZmqZoltanExt_Tests
    PRIVATE
        CppZmqZoltanExt::CppZmqZoltanExt
        cppzmq
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(CppZmqZoltanExt_Tests)
