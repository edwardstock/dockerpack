cmake_minimum_required(VERSION 3.12)
project(dockerpack
        VERSION 0.1.0
        DESCRIPTION "DockerPack is a small simple docker-based local stateful CI to test and deploy projects that should be built on multiple environments."
        LANGUAGES CXX
        )

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DDOCKERPACK_VERSION="${PROJECT_VERSION}")
cmake_policy(SET CMP0074 NEW)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)
if (WIN32)
	execute_process(COMMAND cmd /C "echo ${CMAKE_PROJECT_VERSION} > ${CMAKE_CURRENT_SOURCE_DIR}/version")
else ()
	execute_process(COMMAND bash -c "echo ${CMAKE_PROJECT_VERSION} > ${CMAKE_CURRENT_SOURCE_DIR}/version")
endif ()


include(modules/compilers_flags.cmake)


set(HEADERS
    src/execmd.h
    src/data.h
    src/config.h
    src/state.h
    src/builder.h
    src/docker.h
    src/utils.h)

set(SOURCES
    ${HEADERS}
    src/main.cpp
    src/execmd.cpp
    src/config.cpp
    src/state.cpp
    src/builder.cpp
    src/docker.cpp
    src/data.cpp
    src/utils.cpp)

add_executable(dockerpack ${SOURCES})

include(ConanInit)
conan_init()

target_link_libraries(${PROJECT_NAME} CONAN_PKG::toolbox)
target_link_libraries(${PROJECT_NAME} CONAN_PKG::boost)
target_link_libraries(${PROJECT_NAME} CONAN_PKG::nlohmann_json)
target_link_libraries(${PROJECT_NAME} CONAN_PKG::yaml-cpp)
target_link_libraries(${PROJECT_NAME} CONAN_PKG::libsodium)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/termcolor/include)

if (ENABLE_TEST)

	set(gtest_force_shared_crt ${ENABLE_SHARED} CACHE BOOL "" FORCE)

	if (APPLE)
		add_definitions(-DGTEST_USE_OWN_TR1_TUPLE)
		add_definitions(-D__GLIBCXX__)
	endif ()

	enable_testing()
	add_definitions(-DDOCKERPACK_TESTING=1)

	add_executable(${PROJECT_NAME}-test
	               tests/main.cpp)

	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::gtest)

	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::toolbox)
	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::boost)
	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::nlohmann_json)
	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::yaml-cpp)
	target_link_libraries(${PROJECT_NAME}-test CONAN_PKG::libsodium)
	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/termcolor/include)

	if (NOT MSVC)
		target_compile_options(${PROJECT_NAME}-test PRIVATE -Wno-missing-field-initializers)
	endif ()

	if (WITH_COVERAGE)
		set(COVERAGE_LCOV_EXCLUDES
		    'libs/*'
		    '/usr/include/*'
		    'tests/*'
		    'boost/*'
		    '/usr/include/*'
		    '/usr/local/*'
		    'v1'
		    '4.9'
		    )
		include(modules/CodeCoverage.cmake)
		append_coverage_compiler_flags()
		setup_target_for_coverage_lcov(
			NAME ${PROJECT_NAME}-coverage
			EXECUTABLE ${PROJECT_NAME}-test
			DEPENDENCIES ${PROJECT_NAME}-test
		)
	endif ()

	add_test(NAME test-all COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}-test)

endif ()

include(package)