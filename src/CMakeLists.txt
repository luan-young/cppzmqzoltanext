
# ---------------------------------------------------------------------------------------
# Source and Headers Files
# ---------------------------------------------------------------------------------------

set(CZZE_SOURCES
	poller.cpp
	loop.cpp
	actor.cpp
	signal.cpp
	interrupt.cpp
)

set(CZZE_PUBLIC_HEADERS
	../include/cppzmqzoltanext/poller.h
	../include/cppzmqzoltanext/loop.h
	../include/cppzmqzoltanext/actor.h
	../include/cppzmqzoltanext/signal.h
	../include/cppzmqzoltanext/interrupt.h
    ${CMAKE_CURRENT_BINARY_DIR}/../include/cppzmqzoltanext/czze_export.h
)

set(CZZE_PRIVATE_HEADERS
)

# ---------------------------------------------------------------------------------------
# Shared Library
# ---------------------------------------------------------------------------------------

add_library(libcppzmqzoltanext SHARED ${CZZE_SOURCES} ${CZZE_PRIVATE_HEADERS})
target_link_libraries(libcppzmqzoltanext PUBLIC cppzmq)
target_link_libraries(libcppzmqzoltanext PRIVATE Threads::Threads)

add_library(cppzmqzoltanext::cppzmqzoltanext ALIAS libcppzmqzoltanext)
set_target_properties(libcppzmqzoltanext PROPERTIES
    OUTPUT_NAME cppzmqzoltanext
    EXPORT_NAME cppzmqzoltanext
    DEBUG_POSTFIX d
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    DLL_NAME_WITH_SOVERSION TRUE
)

target_sources(libcppzmqzoltanext
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ../include
            ${CMAKE_CURRENT_BINARY_DIR}/../include
        FILES
            ${CZZE_PUBLIC_HEADERS}
)

# ---------------------------------------------------------------------------------------
# Static Library
# ---------------------------------------------------------------------------------------

add_library(libcppzmqzoltanextstatic STATIC ${CZZE_SOURCES} ${CZZE_PRIVATE_HEADERS})
target_link_libraries(libcppzmqzoltanextstatic PUBLIC cppzmq-static)
target_link_libraries(libcppzmqzoltanextstatic PRIVATE Threads::Threads)

add_library(cppzmqzoltanext::cppzmqzoltanext-static ALIAS libcppzmqzoltanextstatic)
set_target_properties(libcppzmqzoltanextstatic PROPERTIES
    OUTPUT_NAME cppzmqzoltanext-static
    EXPORT_NAME cppzmqzoltanext-static
    DEBUG_POSTFIX d
    VERSION ${PROJECT_VERSION}
)

target_sources(libcppzmqzoltanextstatic
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ../include
            ${CMAKE_CURRENT_BINARY_DIR}/../include
        FILES
            ${CZZE_PUBLIC_HEADERS}
)

# ---------------------------------------------------------------------------------------
# Export Header
# ---------------------------------------------------------------------------------------

# Shared target used for generating export header for both the shared and static targets
generate_export_header(libcppzmqzoltanext
    BASE_NAME CZZE
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/cppzmqzoltanext/czze_export.h
)
# Static target needs special preprocessor define
# to prevent symbol import/export keywords being added
target_compile_definitions(libcppzmqzoltanextstatic PRIVATE CZZE_STATIC_DEFINE)
