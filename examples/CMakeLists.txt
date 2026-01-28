add_executable(full_example full_example.cpp)
target_link_libraries(full_example
    PRIVATE
        cppzmqzoltanext::cppzmqzoltanext
)
