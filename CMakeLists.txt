cmake_minimum_required(VERSION 3.20)
project(genfx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable folder structure in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# When linking statically via vcpkg, wxWidgets image handlers may require these
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(WebP REQUIRED)

# Sources
file(GLOB GENFX_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.cxx
)
file(GLOB GENFX_HEADERS CONFIGURE_DEPENDS
    src/*.h
    src/*.hpp
)

add_executable(genfx
    ${GENFX_SOURCES}
    ${GENFX_HEADERS}
)

# Windows: make sure app is GUI subsystem
if (WIN32)
    set_target_properties(genfx PROPERTIES WIN32_EXECUTABLE YES)
endif()

# Include dirs
target_include_directories(genfx PRIVATE src)

# Link wxWidgets
target_link_libraries(genfx PRIVATE ${wxWidgets_LIBRARIES} PNG::PNG ZLIB::ZLIB WebP::webp WebP::webpdemux)

# Optimize a bit in Release
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (MSVC)
        target_compile_options(genfx PRIVATE /O2)
    else()
        target_compile_options(genfx PRIVATE -O3)
    endif()
endif()
