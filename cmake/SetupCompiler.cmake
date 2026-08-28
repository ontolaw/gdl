# Copyright 2021-2026 Jens A. Koch.
# SPDX-License-Identifier: MIT
# This file is part of ontolaw/gdl.

#-------------------------------------------------------------------
# Compiler Setup
#-------------------------------------------------------------------

if(WIN32)
    # Build for a Windows 10 host system.
    set(CMAKE_SYSTEM_VERSION 10.0)
    add_definitions(/DWINVER=_WIN32_WINNT_WIN10 /DNTDDI_VERSION=NTDDI_WIN10 /D_WIN32_WINNT=_WIN32_WINNT_WIN10)

    message(STATUS "[INFO] BUILD_SHARED_LIBS -> '${BUILD_SHARED_LIBS}'.")

    # When we build statically (MT):
    if(NOT BUILD_SHARED_LIBS)
        # Select MSVC runtime based on CMAKE_MSVC_RUNTIME_LIBRARY.
        # We switch from the multi-threaded dynamically-linked library (default)
        # to the multi-threaded statically-linked runtime library.
        cmake_policy(SET CMP0091 NEW)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT") # = static build
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /MTd")  # = static debug build

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:libcpmtd.lib /NODEFAULTLIB:msvcrtd.lib")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcpmt.lib /NODEFAULTLIB:msvcrt.lib")
endif()

#-------------------------------------------------------------------
# Define C++ Standard to use
#-------------------------------------------------------------------

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)

message("Using Compiler: ${CMAKE_CXX_COMPILER_ID}")

if(MSVC)
  set(CMAKE_CXX_STANDARD        23) # this sets /std:c++latest on MSVC (VS2022)

  # /W4     Warnings Level 4 (all)
  # /WX     Treats all compiler warnings as errors
  # /sdl    Enable Additional Security Checks
  # /utf-8  Set source and execution character sets to UTF-8
  # /nologo Suppress compiler copyright and version messages.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /sdl /utf-8 /nologo")

  add_compile_definitions(UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "CLANG")
        # enable incomplete features to get "std::format" support
        set(LIBCXX_ENABLE_INCOMPLETE_FEATURES ON)

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -stdlib=libc++ -pthread -Wall -Wextra -Werror -fexec-charset=UTF-8 -lstdc++")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl -stdlib=libc++ -lc++ -lc++abi -lstdc++")

        # 3.29
        set(CMAKE_LINKER_TYPE "LLD")

        # Prefer and force lld if available to avoid the system ld loading LLVM gold plugin
        find_program(LLD_LINKER NAMES ld.lld lld /usr/lib/llvm-20/bin /usr/bin /usr/local/bin)
        if(LLD_LINKER)
            message(STATUS "Using lld linker: ${LLD_LINKER}")
            # Tell CMake to use this linker
            set(CMAKE_LINKER ${LLD_LINKER} CACHE FILEPATH "" FORCE)
            # Ensure the compiler is instructed to use lld as well
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
            set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
        else()
            # https://github.com/llvm/llvm-project/issues/139602 - Debian/Ubuntu package does not ship LLVMgold.so
            message(WARNING "lld not found on PATH; build may attempt to use system ld which can load LLVMgold plugin and fail.")
            # still add -fuse-ld to prefer lld if it becomes available in the toolchain
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
            set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
        endif()
        # else
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -pthread -Wl,--no-as-needed -ldl") # -stdlib=libc++
        set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++") # -stdlib=libc++ -lc++abi
    endif()
endif()

# Use mold linker if available and supported
#find_program(MOLD_LINKER mold)
#if(MOLD_LINKER AND NOT MSVC)
#  message(STATUS "Using mold linker: ${MOLD_LINKER}")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
#endif()

# explicit exports = hide inlines
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use LTO for Release builds only.
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    # Disable LTO for now to avoid LLVM gold plugin issues (see upstream LLVM issue)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)

# Also disable IPO globally to be safe and remove any -flto flags that may have been injected
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# Strip -flto flags from flags to prevent linker from trying to load LLVMgold plugin
string(REPLACE "-flto=thin" "" CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}")
string(REPLACE "-flto"      "" CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}")
string(REPLACE "-flto=thin" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
string(REPLACE "-flto"      "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

# /c65001  Change code page to UTF-8 (65001) and
# /nologo  Suppress the copyright messages.
string(APPEND CMAKE_RC_FLAGS " /c65001 /nologo ")

#-------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------

if (MSVC)

  # /CETCOMPAT - CET Shadow Stack kompatibel = Anti-ROP
  if (MSVC_CXX_ARCHITECTURE_ID STREQUAL "x64")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO " /CETCOMPAT")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /CETCOMPAT")
  endif()

  #
  # Settings for ALL build types
  #
  #
  # - GR           = Enable Run-Time Type Information
  # - EHsc         = Exception Handling with s = stackunwinding, c = extern functions never throw
  # - permissive-  = specify language standards-conforming compiler behavior (=/Zc strict mode)
  # - nologo       = disable logo
  # - guard:cf     = Enable Control Flow Guard
  set(CMAKE_C_FLAGS_INIT             "-DWIN32 -D_WINDOWS -permissive- -nologo /guard:cf")
  set(CMAKE_CXX_FLAGS_INIT           "-DWIN32 -D_WINDOWS -GR -EHsc -permissive- -nologo /guard:cf")
  set(CMAKE_EXE_LINKER_FLAGS_INIT    "-machine:x64 -nologo /GUARD:CF /STACK:4194304")
  set(CMAKE_MODULE_LINKER_FLAGS_INIT "-machine:x64 -nologo /GUARD:CF /STACK:4194304")
  set(CMAKE_SHARED_LINKER_FLAGS_INIT "-machine:x64 -nologo /GUARD:CF /STACK:4194304")
  set(CMAKE_STATIC_LINKER_FLAGS_INIT "-machine:x64 -nologo")

  #
  # Debug
  #
  # Zi:     Produce a separate PDB file (debug symbols)
  # Ob0:    Disable inline expansions
  # Od:     Disable code movements for easier debugging (DEBUG)
  #
  set(CMAKE_CXX_FLAGS_DEBUG_INIT           "-Zi -Ob0 -Od")
  set(CMAKE_C_FLAGS_DEBUG_INIT             "-Zi -Ob0 -Od")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT    "-INCREMENTAL:NO -debug")
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG_INIT "-INCREMENTAL:NO -debug")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG_INIT "-INCREMENTAL:NO -debug")

  #
  # Release
  #
  # O2:     Maximize Speed
  # Ob3:    Aggressive Inline Function Expansion
  # GL:     Whole Program Optimization
  # NDEBUG: Assertion checks turned off at compile time
  #
  set(CMAKE_CXX_FLAGS_RELEASE_INIT           "-O2 -Ob3 -GL -DNDEBUG")
  set(CMAKE_C_FLAGS_RELEASE_INIT             "-O2 -Ob3 -GL -DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT    "-INCREMENTAL:NO -LTCG")
  set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_INIT "-INCREMENTAL:NO -LTCG")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE_INIT "-INCREMENTAL:NO -LTCG")

  # RelWithDebugInfo
  #
  # This build_type is important, because we need to step through the
  # assembly of the optimized release build, while having debug information.
  #
  # Zi:     Produce a separate PDB file (debug symbols)
  # O2:     Maximize Speed
  # Ob3:    Aggressive Inline Function Expansion
  # GL:     Whole Program Optimization
  # NDEBUG: Assertion checks turned off at compile time
  #
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT           "-Zi -O2 -Ob3 -GL -DNDEBUG")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT             "-Zi -O2 -Ob3 -GL -DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT    "-INCREMENTAL:NO -LTCG -debug")
  set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO_INIT "-INCREMENTAL:NO -LTCG -debug")
  set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO_INIT "-INCREMENTAL:NO -LTCG -debug")

endif()


