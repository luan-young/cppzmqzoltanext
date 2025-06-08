set(CZZE_SOURCES
	poller.cpp
	loop.cpp
)

set(CZZE_PUBLIC_HEADERS
	../include/cppzmqzoltanext/poller.h
	../include/cppzmqzoltanext/loop.h
)

set(CZZE_PRIVATE_HEADERS

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

target_sources(CppZmqZoltanExt_Library
	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
			../include
			#${CMAKE_CURRENT_BINARY_DIR}/../include
		FILES
			${CZZE_PUBLIC_HEADERS}
)
