add_executable(full_example full_example.cpp)
target_link_libraries(full_example
    PRIVATE
        cppzmqzoltanext::cppzmqzoltanext
)

add_executable(zpl_config_example zpl_config_example.cpp)
target_link_libraries(zpl_config_example
    PRIVATE
        cppzmqzoltanext::cppzmqzoltanext
)
