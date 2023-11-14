cmake_minimum_required(VERSION 3.15)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-D_DEBUG)
    add_definitions(-D_GLIBCXX_ASSERTIONS)
    if (DEBUG_SANITIZER)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -O0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,address")
    endif ()
endif ()

if (APPLE AND PLATFORM_DESKTOP)
    execute_process(COMMAND sw_vers -productVersion
            TIMEOUT 5
            OUTPUT_VARIABLE MACOS_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "compiling on macOS: ${MACOS_VERSION}")
    if (MAC_IntelChip)
        message(STATUS "CMAKE_OSX_ARCHITECTURES: x86_64")
        message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: 10.11")
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "" FORCE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "" FORCE)

        set(USE_BOOST_FILESYSTEM ON)
        set(USE_SYSTEM_CURL OFF)
    elseif(MAC_AppleSilicon)
        # Build a Universal binary on macOS
        message(STATUS "CMAKE_OSX_ARCHITECTURES: arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    elseif(MAC_Universal)
        # Build a Universal binary on macOS
        message(STATUS "CMAKE_OSX_ARCHITECTURES: x86_64;arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "" FORCE)
        set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET[arch=arm64] "11.0" CACHE STRING "" FORCE)

        set(USE_BOOST_FILESYSTEM ON)
        set(USE_SYSTEM_CURL OFF)
    elseif (MACOS_VERSION VERSION_GREATER_EQUAL 10.15)
        set(USE_SYSTEM_CURL ON)
        set(CPR_FORCE_DARWINSSL_BACKEND ON)
    else ()
        set(USE_BOOST_FILESYSTEM ON)
        set(USE_SYSTEM_CURL OFF)
    endif()

    if (NOT USE_SYSTEM_CURL)
        set(CPR_FORCE_DARWINSSL_BACKEND OFF)
        set(CPR_FORCE_OPENSSL_BACKEND ON)
        set(CPR_ENABLE_CURL_HTTP_ONLY ON)
        set(CURL_USE_LIBSSH2 OFF)
        set(CMAKE_USE_LIBSSH2 OFF)
    endif ()
endif ()

if (USE_BOOST_FILESYSTEM)
    set(CPR_USE_BOOST_FILESYSTEM ON)
    add_definitions(-DCPR_USE_BOOST_FILESYSTEM)
    add_definitions(-DUSE_BOOST_FILESYSTEM)
endif ()
