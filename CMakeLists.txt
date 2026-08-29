# ----------- general -----------

cmake_minimum_required(VERSION 3.24)
project(modulemanager)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------- project -----------

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# The manager needs the contract it manages.
find_package(imodule REQUIRED)

# Header-only : nothing to compile, just an include path to propagate.
add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE
	imodule
	${CMAKE_DL_LIBS}
)

# ----------- include -----------

target_include_directories(${PROJECT_NAME} INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/includes/
)

#----------- test -----------

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

	find_package(GoogleTest REQUIRED)

	enable_testing()

	# Two real vendors, loaded at runtime by the tests.
	foreach(vendor ray sfml)
		add_library(${PROJECT_NAME}_${vendor} MODULE examples/${vendor}.cpp)
		target_link_libraries(${PROJECT_NAME}_${vendor} PRIVATE imodule)
		target_include_directories(${PROJECT_NAME}_${vendor} PRIVATE examples)
		set_target_properties(${PROJECT_NAME}_${vendor} PROPERTIES PREFIX "")
	endforeach()

	file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

	add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
	add_dependencies(${PROJECT_NAME}_tests ${PROJECT_NAME}_ray ${PROJECT_NAME}_sfml)

	target_include_directories(${PROJECT_NAME}_tests PRIVATE examples)
	target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
		RAY_PATH="$<TARGET_FILE:${PROJECT_NAME}_ray>"
		SFML_PATH="$<TARGET_FILE:${PROJECT_NAME}_sfml>"
	)

	target_link_libraries(${PROJECT_NAME}_tests
		gtest_main
		${PROJECT_NAME}
	)

	include(GoogleTest)
	gtest_discover_tests(${PROJECT_NAME}_tests)

endif()
