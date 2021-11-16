cmake_minimum_required(VERSION 3.1)

# TODO: SDK Versioning.
project(coro_test VERSION 1.0.0)

LINK_DIRECTORIES(
	${PROJECT_SOURCE_DIR}/dep/lib
	)

INCLUDE_DIRECTORIES(
	${PROJECT_SOURCE_DIR}/dep/include
	${PROJECT_SOURCE_DIR}/dep/include/asio
)

add_executable(coro_test test.cpp)

add_definitions(-DASIO_STANDALONE)

if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fcoroutines")
	target_link_libraries(coro_test PUBLIC winmm ws2_32 mswsock)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fcoroutines")
	target_link_libraries(coro_test PUBLIC)
endif()

install(TARGETS coro_test DESTINATION ${PROJECT_SOURCE_DIR})
