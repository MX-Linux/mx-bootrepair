# **********************************************************************
# * Copyright (C) 2014-2025 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of mx-boot-repair.
# *
# * mx-boot-repair is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * MX Boot Repair is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with MX Boot Repair.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION_FROM_CHANGELOG "")

if(DEFINED PROJECT_VERSION_OVERRIDE AND NOT "${PROJECT_VERSION_OVERRIDE}" STREQUAL "")
    set(PROJECT_VERSION_FROM_CHANGELOG "${PROJECT_VERSION_OVERRIDE}")
    message(STATUS "Using version override: ${PROJECT_VERSION_FROM_CHANGELOG}")
else()
    # Try dpkg-parsechangelog first (Debian/Ubuntu)
    execute_process(
        COMMAND dpkg-parsechangelog -SVersion
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_VERSION_FROM_CHANGELOG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE DPKG_RESULT
        ERROR_QUIET
    )

    if(DPKG_RESULT EQUAL 0 AND NOT "${PROJECT_VERSION_FROM_CHANGELOG}" STREQUAL "")
        message(STATUS "Using version from debian/changelog: ${PROJECT_VERSION_FROM_CHANGELOG}")
    elseif(EXISTS ${CMAKE_SOURCE_DIR}/debian/changelog)
        file(READ ${CMAKE_SOURCE_DIR}/debian/changelog PROJECT_CHANGELOG_CONTENT)
        string(REGEX MATCH "^[^\\(]*\\(([^)]+)\\)" _match "${PROJECT_CHANGELOG_CONTENT}")
        if(NOT _match)
            message(FATAL_ERROR "Could not parse version from debian/changelog")
        endif()
        set(PROJECT_VERSION_FROM_CHANGELOG "${CMAKE_MATCH_1}")
        message(STATUS "Using version parsed from debian/changelog: ${PROJECT_VERSION_FROM_CHANGELOG}")
    else()
        message(FATAL_ERROR "Could not determine version: dpkg-parsechangelog not available and debian/changelog missing")
    endif()
endif()

project(mx-boot-repair
    VERSION ${PROJECT_VERSION_FROM_CHANGELOG}
    DESCRIPTION "MX Boot Repair - GUI for repairing boot problems"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile commands export for IDEs and tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimize for Ninja builds
if(CMAKE_GENERATOR STREQUAL "Ninja")
    # Enable colored output for Ninja
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-fcolor-diagnostics)
    endif()
endif()

# Option to use clang for testing builds
option(USE_CLANG "Use clang compiler" OFF)
if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
    set(CMAKE_CXX_COMPILER_ID "Clang")
    message(STATUS "Using clang compiler")
endif()

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    LinguistTools
)

# Enable automatic MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Define source files
set(SOURCES
    src/gui/main.cpp
    src/gui/mainwindow.cpp
    src/gui/about.cpp
)

set(HEADERS
    src/core/common.h
    src/gui/mainwindow.h
    src/gui/about.h
)

set(UI_FILES
    src/gui/mainwindow.ui
)

set(RESOURCE_FILES
    src/gui/images.qrc
)

# Get all translation files
file(GLOB TRANSLATION_FILES "translations/*.ts")

# Create the executable
add_library(bootrepair-core STATIC
    src/core/cmd.cpp
    src/core/bootrepair_engine.cpp
    src/core/bootrepair_engine.h
    src/core/app_init.cpp
    src/core/app_init.h
)

target_link_libraries(bootrepair-core
    Qt6::Core
)

target_include_directories(bootrepair-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(bootrepair-core PRIVATE
    -Wpedantic
    -pedantic
    -Werror=return-type
    -Werror=switch
    -Werror=uninitialized
    -Werror
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR USE_CLANG)
    target_compile_options(bootrepair-core PRIVATE -Werror=return-stack-address)
else()
    target_compile_options(bootrepair-core PRIVATE -Werror=return-local-addr)
endif()

add_executable(mx-boot-repair
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCE_FILES}
    src/cli/controller.cpp
    src/cli/controller.h
)

# Link Qt6 libraries
target_link_libraries(mx-boot-repair
    bootrepair-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Allow GUI sources to include headers from src/ (e.g., core/..., common.h)
target_include_directories(mx-boot-repair PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Set compiler flags
target_compile_options(mx-boot-repair PRIVATE
    -Wpedantic
    -pedantic
    -Werror=return-type
    -Werror=switch
    -Werror=uninitialized
    -Werror
)

# Add compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR USE_CLANG)
    target_compile_options(mx-boot-repair PRIVATE -Werror=return-stack-address)
else()
    target_compile_options(mx-boot-repair PRIVATE -Werror=return-local-addr)
endif()

# Set compile definitions
target_compile_definitions(mx-boot-repair PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    VERSION="${PROJECT_VERSION}"
)

# Release-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(mx-boot-repair PRIVATE NDEBUG)
    target_compile_options(mx-boot-repair PRIVATE -O3)

    # Add LTO - different flags for different compilers
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR USE_CLANG)
        target_compile_options(mx-boot-repair PRIVATE -flto=thin)
        target_link_options(mx-boot-repair PRIVATE -flto=thin)
    else()
        target_compile_options(mx-boot-repair PRIVATE -flto=auto)
        target_link_options(mx-boot-repair PRIVATE -flto=auto)
    endif()
endif()

# Handle translations
qt6_add_translations(mx-boot-repair
    TS_FILES ${TRANSLATION_FILES}
    LRELEASE_OPTIONS -compress -nounfinished -removeidentical -silent
    QM_FILES_OUTPUT_VARIABLE qm_files
)

# Set target properties
set_target_properties(mx-boot-repair PROPERTIES
    OUTPUT_NAME "mx-boot-repair"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# CLI target (QtCore-only) — disabled by default
option(BUILD_CLI "Build standalone CLI target" OFF)
if(BUILD_CLI)
    add_executable(mx-boot-repair-cli
        src/cli/main.cpp
        src/cli/controller.cpp
        src/cli/controller.h
    )

    target_link_libraries(mx-boot-repair-cli
        bootrepair-core
        Qt6::Core
    )

    target_compile_definitions(mx-boot-repair-cli PRIVATE
        QT_DEPRECATED_WARNINGS
        QT_DISABLE_DEPRECATED_BEFORE=0x060000
        VERSION="${PROJECT_VERSION}"
    )

    set_target_properties(mx-boot-repair-cli PROPERTIES
        OUTPUT_NAME "mx-boot-repair-cli"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Install target (required by Debian build system)
# Other files are handled by debian/install
install(TARGETS mx-boot-repair
    RUNTIME DESTINATION bin
)
