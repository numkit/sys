cmake_minimum_required(VERSION 3.20)
project(numkit_sys VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(NUMKIT_SYS_BUILD_TESTS "Build GoogleTest suite for numkit/sys" ON)

include(FetchContent)

# ── Dependency: numkit/value ─────────────────────────────────────────────
if(NOT TARGET numkit_value)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../value/CMakeLists.txt")
        message(STATUS "Using local numkit/value from ${CMAKE_CURRENT_SOURCE_DIR}/../value")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../value" _deps/numkit_value)
    else()
        FetchContent_Declare(
            numkit_value
            GIT_REPOSITORY https://github.com/numkit/value.git
            GIT_TAG main
        )
        FetchContent_MakeAvailable(numkit_value)
    endif()
endif()

set(NUMKIT_SYS_SOURCES
    src/vfs.cpp
    src/fs_context.cpp
    src/process.cpp
)

add_library(numkit_sys STATIC ${NUMKIT_SYS_SOURCES})

add_library(numkit::sys ALIAS numkit_sys)

target_include_directories(numkit_sys PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(numkit_sys PUBLIC numkit_value)

if(MSVC)
    target_compile_options(numkit_sys PRIVATE /W4 /MP /bigobj)
else()
    target_compile_options(numkit_sys PRIVATE -Wall -Wextra -fPIC)
endif()

if(EMSCRIPTEN)
    target_compile_options(numkit_sys PRIVATE -fexceptions)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS numkit_sys
    EXPORT numkit_sys_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(NUMKIT_SYS_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(numkit_sys_test
        tests/process_test.cpp
        tests/vfs_test.cpp
    )
    target_link_libraries(numkit_sys_test PRIVATE numkit_sys GTest::gtest_main)
    add_test(NAME numkit_sys_test COMMAND numkit_sys_test)
endif()
