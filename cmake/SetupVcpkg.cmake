# Copyright 2021-2026 Jens A. Koch.
# SPDX-License-Identifier: MIT
# This file is part of ontolaw/gdl.

# SetupVcpkg
#
# This configures vcpkg using environment variables instead of a command-line options.
#
# https://github.com/microsoft/vcpkg/blob/master/docs/users/integration.md#using-an-environment-variable-instead-of-a-command-line-option
#
# Environment Variables: https://vcpkg.readthedocs.io/en/latest/users/config-environment/
#

#
# -- Automatic install of vcpkg dependencies.
# Automatically copy dependencies into the install target directory for executables
# This is experimental.
# See https://github.com/Microsoft/vcpkg/issues/1653
#
# set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
# set(VCPKG_APPLOCAL_DEPS_INSTALL ON)
# if(DEFINED ENV{VCPKG_APPLOCAL_DEPS} AND NOT DEFINED VCPKG_APPLOCAL_DEPS)
#     set(VCPKG_APPLOCAL_DEPS "$ENV{VCPKG_APPLOCAL_DEPS}" CACHE BOOL "")
# endif()
# if(DEFINED ENV{X_VCPKG_APPLOCAL_DEPS_INSTALL} AND NOT DEFINED VCPKG_APPLOCAL_DEPS)
#     if(POLICY CMP0087)
#         cmake_policy(SET CMP0087 NEW)
#     endif()
#     set(X_VCPKG_APPLOCAL_DEPS_INSTALL "$ENV{X_VCPKG_APPLOCAL_DEPS_INSTALL}" CACHE BOOL "")
# endif()

# Add this file and the VCPKG_MANIFEST_FILE as a "vcpkg" source_group to the IDE.
# They are not automatically picked up and listed as "important project" files by IDEs, yet.
source_group("vcpkg" FILES
    "${CMAKE_SOURCE_DIR}/cmake/SetupVcpkg.cmake"
    "${CMAKE_SOURCE_DIR}/vcpkg.json"
)

# This var is uninitalized.. it might be feature gated?
iF(NOT DEFINED VCPKG_MANIFEST_FILE)
    set(VCPKG_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/vcpkg.json")
endif()

#
# Check to make sure the VCPKG_TARGET_TRIPLET matches BUILD_SHARED_LIBS
#
#if ("${VCPKG_TARGET_TRIPLET}" MATCHES ".*-static")
#    if (BUILD_SHARED_LIBS)
#        message(FATAL_ERROR "When the VCPKG_TARGET_TRIPLET ends with '-static' the BUILD_SHARED_LIBS must be 'OFF'.")
#    endif()
#else()
#    if (NOT BUILD_SHARED_LIBS)
#        message(FATAL_ERROR "${BUILD_SHARED_LIBS} When the VCPKG_TARGET_TRIPLET does not end with '-static' the BUILD_SHARED_LIBS must be 'ON'.")
#    endif()
#endif()

#
# Print VCPKG configuration overview
#
message(STATUS "")
message(STATUS "[VCPKG]  Configuration Overview:")
message(STATUS "")
message(STATUS "[VCPKG]  BUILD_SHARED_LIBS             -> '${BUILD_SHARED_LIBS}'")
message(STATUS "[VCPKG]  ENV.VCPKG_ROOT                -> '$ENV{VCPKG_ROOT}'")
message(STATUS "[VCPKG]  CMAKE_TOOLCHAIN_FILE          -> '${CMAKE_TOOLCHAIN_FILE}'")
message(STATUS "")
message(STATUS "[VCPKG]  VCPKG_ENABLED                 -> '${VCPKG_ENABLED}'")
message(STATUS "[VCPKG]  VCPKG_VERBOSE                 -> '${VCPKG_VERBOSE}'")
#message(STATUS "[VCPKG]  VCPKG_APPLOCAL_DEPS           -> '${VCPKG_APPLOCAL_DEPS}'")
message(STATUS "[VCPKG]  VCPKG_MANIFEST_FILE           -> '${VCPKG_MANIFEST_FILE}'")
message(STATUS "[VCPKG]  VCPKG_INSTALLED_DIR           -> '${VCPKG_INSTALLED_DIR}'")
message(STATUS "[VCPKG]  VCPKG_TARGET_TRIPLET          -> '${VCPKG_TARGET_TRIPLET}'")
message(STATUS "")
