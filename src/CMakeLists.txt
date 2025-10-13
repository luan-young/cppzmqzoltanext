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
)

set(CZZE_PRIVATE_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/czze_export.h
)

if(BUILD_SHARED_LIBS)
    add_library(CppZmqZoltanExt_Library SHARED ${CZZE_SOURCES} ${CZZE_PRIVATE_HEADERS})
else()
    add_library(CppZmqZoltanExt_Library STATIC ${CZZE_SOURCES} ${CZZE_PRIVATE_HEADERS})
endif()

add_library(CppZmqZoltanExt::CppZmqZoltanExt ALIAS CppZmqZoltanExt_Library)
set_target_properties(CppZmqZoltanExt_Library PROPERTIES
    OUTPUT_NAME CppZmqZoltanExt
    EXPORT_NAME CppZmqZoltanExt
    DEBUG_POSTFIX d
)

# Generate export header
generate_export_header(CppZmqZoltanExt_Library
    BASE_NAME CZZE
    EXPORT_FILE_NAME czze_export.h
)

# target_include_directories(CppZmqZoltanExt_Library
#     PUBLIC
#         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
#         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
#         $<INSTALL_INTERFACE:include>
# )

target_sources(CppZmqZoltanExt_Library
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ../include
			#${CMAKE_CURRENT_BINARY_DIR}/../include
            ${CMAKE_CURRENT_BINARY_DIR}
        FILES
            ${CZZE_PUBLIC_HEADERS}
            ${CZZE_PRIVATE_HEADERS}
)

target_link_libraries(CppZmqZoltanExt_Library
    PUBLIC
        cppzmq
)
