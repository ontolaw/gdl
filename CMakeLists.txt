cmake_minimum_required(VERSION 3.25)

#-------------------------------------------------------------------
# Setup CMake
#-------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_FIND_LIBRARY_PREFIXES ";")
set(CMAKE_FIND_LIBRARY_SUFFIXES ";")

# Generate CompilationDatabase in `compile_commands.json`.
# The file is used by build tools and IntelliSense (VSCode, YouCompleteMe, etc).
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(CMAKE_ERROR_DEPRECATED TRUE)

#-------------------------------------------------------------------
# Build Options
#-------------------------------------------------------------------

option(BUILD_SHARED_LIBS    "Build shared libraries"             OFF)
option(BUILD_TESTING        "Build tests"                         ON)

#-------------------------------------------------------------------
# Get Versioning Data
#-------------------------------------------------------------------

# vcpkg.json is the primary source for versioning data
file(READ ${CMAKE_SOURCE_DIR}/vcpkg.json VCPKG_JSON_STRING)
string(JSON APP_NAME     GET ${VCPKG_JSON_STRING} "name")
string(JSON APP_VERSION  GET ${VCPKG_JSON_STRING} "version")
string(JSON APP_LICENSE  GET ${VCPKG_JSON_STRING} "license")
string(JSON APP_DESC     GET ${VCPKG_JSON_STRING} "description")
string(JSON APP_HOMEPAGE GET ${VCPKG_JSON_STRING} "homepage")

#-------------------------------------------------------------------
# Project
#-------------------------------------------------------------------

project(gdl VERSION ${APP_VERSION} DESCRIPTION "gdl" LANGUAGES CXX)

#-------------------------------------------------------------------
# CMake Modules
#-------------------------------------------------------------------

include(SetupVcpkg)
include(SetupCompiler)
include(ShowBuildTargetProperties)

#-------------------------------------------------------------------
# Versioning
#-------------------------------------------------------------------
# The following variables are used in the "version.h.in" file:
#
# - CURRENT_YEAR
# - BUILD_DATE
# - BUILD_COMMIT
# - BUILD_PLATFORM
# - BUILD_TRIPLET
# - APP_BINARY_NAME
#
# These variables are replaced by CMake during the configuration step.

string(TIMESTAMP CURRENT_YEAR "%Y")
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

# BUILD_COMMIT: try to get the current Git commit, otherwise unknown
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE BUILD_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT BUILD_COMMIT)
  set(BUILD_COMMIT "unknown")
endif()

# get the platform and architecture
set(BUILD_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# BUILD_TRIPLET
if(DEFINED VCPKG_TARGET_TRIPLET)
  set(BUILD_TRIPLET ${VCPKG_TARGET_TRIPLET})
else()
  set(BUILD_TRIPLET "unknown")
endif()

# APP_BINARY_NAME
set(APP_BINARY_NAME "${PROJECT_NAME}")

configure_file("src/version.h.in" "version.h" @ONLY)

#-------------------------------------------------------------------
# Find Dependencies
#-------------------------------------------------------------------

find_package(pugixml CONFIG REQUIRED)

find_package(CURL CONFIG REQUIRED)

# zlib is installed with curl, but not used as library.
# zlib headers are included from minizip.
#find_package(ZLIB REQUIRED)

# find_package(minizip CONFIG)' is deprecated
find_package(unofficial-minizip CONFIG REQUIRED)

find_package(spdlog CONFIG REQUIRED)

# msvc has C++20 std::format
# we need fmt as polyfill lib for all other compilers
if(NOT MSVC)
  find_package(fmt CONFIG REQUIRED)
endif()

find_package(Threads REQUIRED)

if(BUILD_TESTING)
  find_package(Catch2 CONFIG REQUIRED)
endif()

# Disabled, because SSL certificate validation is broken server side.
# cacert.pem
#file(DOWNLOAD "https://curl.se/ca/cacert.pem" "${CMAKE_BINARY_DIR}/ca-bundle.crt" TLS_VERIFY ON)
#install(FILES "${CMAKE_BINARY_DIR}/ca-bundle.crt" DESTINATION "ssl/certs")

#-------------------------------------------------------------------
# Setup Build Targets
#-------------------------------------------------------------------

#-------------------------------------------------------------------
# Build Target: gdl                                    (executable)
#-------------------------------------------------------------------

set(SOURCES
    # Version
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    # Application
    src/app/main.h          src/app/main.cpp
    src/app/config.h
    # Utils
    src/utils/xml.h         src/utils/xml.cpp
    src/utils/files.h       src/utils/files.cpp
    src/utils/unzip.h       src/utils/unzip.cpp
    src/utils/download.h    src/utils/download.cpp
    src/utils/crawler.h     src/utils/crawler.cpp
    src/utils/clicolors.h   src/utils/clicolors.cpp
    src/utils/dataset.h     src/utils/dataset.cpp
    # CLI Commands
    src/commands/cmd.base.h
    src/commands/cmd.help.h             src/commands/cmd.help.cpp
    src/commands/cmd.version.h          src/commands/cmd.version.cpp
    src/commands/cmd.download-toc.h     src/commands/cmd.download-toc.cpp
    src/commands/cmd.download-files.h   src/commands/cmd.download-files.cpp
    src/commands/cmd.unzip-files.h      src/commands/cmd.unzip-files.cpp
    src/commands/cmd.format-xml.h       src/commands/cmd.format-xml.cpp
    src/commands/cmd.dataset-report.h   src/commands/cmd.dataset-report.cpp
)

add_executable(gdl ${SOURCES})

target_sources(gdl PRIVATE ${SOURCES})

target_include_directories(gdl PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  ${CMAKE_CURRENT_BINARY_DIR} # to include the generated "version.h"
)

target_link_libraries(gdl
  PRIVATE
    pugixml::static
    CURL::libcurl
    spdlog::spdlog
    unofficial::minizip::minizip
    #ZLIB::ZLIB                  # zlib is included by minizip
)

target_link_libraries(gdl
  PRIVATE
    # The MSVC compiler has C++20 std::format.
    # All other compilers need to polyfill using fmt.
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:fmt::fmt>
)

# Ensure linking against threads and the C++ standard library when using
# static third-party libraries built with libstdc++ (fixes undefined
# references to operator new/delete, threading and exception symbols).
target_link_libraries(gdl PRIVATE Threads::Threads)
target_link_libraries(gdl PRIVATE -lstdc++)

#-------------------------------------------------------------------
# FeatureSummary
#-------------------------------------------------------------------

include(FeatureSummary)

feature_summary(WHAT ALL DESCRIPTION "[Build Summary]" FATAL_ON_MISSING_REQUIRED_PACKAGES)

#-------------------------------------------------------------------
# Display Compiler and Linker properties of Build Targets
#-------------------------------------------------------------------

show_build_target_properties(gdl)

#-------------------------------------------------------------------
# Build Target: gdl_tests                               (executable)
#-------------------------------------------------------------------

if(BUILD_TESTING)

  # disabled intentionally, because CDash isnt used for this project
  #set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
  #include(CTest)

  enable_testing()

  add_executable(gdl_tests
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    src/app/config.h
    src/app/main.h
    src/app/main.cpp
    src/commands/cmd.base.h
    src/commands/cmd.help.h
    src/commands/cmd.help.cpp
    src/commands/cmd.version.h
    src/commands/cmd.version.cpp
    src/commands/cmd.download-toc.h
    src/commands/cmd.download-toc.cpp
    src/commands/cmd.download-files.h
    src/commands/cmd.download-files.cpp
    src/commands/cmd.unzip-files.h
    src/commands/cmd.unzip-files.cpp
    src/commands/cmd.format-xml.h
    src/commands/cmd.format-xml.cpp
    src/commands/cmd.dataset-report.h
    src/commands/cmd.dataset-report.cpp
    src/utils/files.h
    src/utils/files.cpp
    src/utils/xml.h
    src/utils/xml.cpp
    src/utils/unzip.h
    src/utils/unzip.cpp
    src/utils/clicolors.h
    src/utils/clicolors.cpp
    src/utils/download.h
    src/utils/download.cpp
    src/utils/crawler.h
    src/utils/crawler.cpp
    src/utils/dataset.h
    src/utils/dataset.cpp
    tests/commands/command_metadata_tests.cpp
    tests/commands/command_run_tests.cpp
    tests/app/config_tests.cpp
    tests/app/main_tests.cpp
    tests/utils/files_utils_tests.cpp
    tests/utils/xml_utils_tests.cpp
    tests/utils/unzip_utils_tests.cpp
    tests/utils/clicolors_tests.cpp
    tests/utils/download_utils_tests.cpp
    tests/utils/download_smoke_tests.cpp
    tests/utils/crawler_utils_tests.cpp
  )

  target_include_directories(gdl_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_compile_definitions(gdl_tests PRIVATE GDL_DISABLE_MAIN)

  target_link_libraries(gdl_tests
    PRIVATE
      Catch2::Catch2WithMain
      CURL::libcurl
      pugixml::static
        unofficial::minizip::minizip
      spdlog::spdlog
      Threads::Threads
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:fmt::fmt>
      -lstdc++
  )

  add_test(NAME gdl_tests COMMAND gdl_tests)
endif()
