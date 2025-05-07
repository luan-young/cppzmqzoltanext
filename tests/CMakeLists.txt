add_executable(CppZmqZoltanExt_Tests
    UTestPoller.cpp
)

target_link_libraries(CppZmqZoltanExt_Tests
    PRIVATE
        CppZmqZoltanExt::CppZmqZoltanExt
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(CppZmqZoltanExt_Tests)
