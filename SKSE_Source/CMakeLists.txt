# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)

# Set your project name. This will be the name of your SKSE .dll file.
project(OStimNet VERSION 0.0.1 LANGUAGES CXX)

# #
# YOU DO NOT NEED TO EDIT ANYTHING BELOW HERE
# #

# If you're not using a mod manager, you probably want the SKSE plugin to go
# inside of your Skyrim "Data" folder.
#
# To do this automatically, set the `SKYRIM_FOLDER` environment variable
# to the path of your Skyrim Special Edition folder
set(MOD_FOLDER_NAME "OStimNet Dev")
set(SKYRIM_MODS_FOLDER "E:\\Skyrim\\development\\mods")
set(OUTPUT_FOLDER "${SKYRIM_MODS_FOLDER}/${MOD_FOLDER_NAME}")

set(CommonLibPath "extern/CommonLibSSE-NG")
set(CommonLibName "CommonLibSSE")

set(BUILD_TESTS       OFF CACHE BOOL "" FORCE)

# Force static linking for all vcpkg dependencies
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

if (DEFINED CommonLibPath AND NOT ${CommonLibPath} STREQUAL "")
        add_subdirectory(${CommonLibPath} ${CommonLibName} EXCLUDE_FROM_ALL)
else ()
        message(
                FATAL_ERROR
                "Variable ${CommonLibName}Path is not set."
        )
endif()

include("${CommonLibPath}/cmake/CommonLibSSE.cmake")

# Otherwise, you can set OUTPUT_FOLDER to any place you'd like :)
# set(OUTPUT_FOLDER "C:/path/to/any/folder")

# Setup your SKSE plugin as an SKSE plugin!
# find_package(CommonLibSSE CONFIG REQUIRED)

# Collect all source files
file(GLOB_RECURSE SOURCES
    "plugin.cpp"
    "src/*.cpp"
    "version.rc"
)

# add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${SOURCES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) # <--- use C++23 standard

find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)
# Ensure project root, src/, and include/ are on the include path so files can include headers by name
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h) # <--- PCH.h is required!

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Silence Microsoft STL deprecation warnings
target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

# Workaround for MSBuild 17.14 bug: create props file to override ATL_KeyFile
if(MSVC)
    set(PROPS_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.props")
    file(WRITE "${PROPS_FILE}" "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n")
    file(APPEND "${PROPS_FILE}" "<Project xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n")
    file(APPEND "${PROPS_FILE}" "  <PropertyGroup>\n")
    file(APPEND "${PROPS_FILE}" "    <!-- Override ATL_KeyFile to empty to bypass MSBuild 17.14 bug -->\n")
    file(APPEND "${PROPS_FILE}" "    <ATL_KeyFile></ATL_KeyFile>\n")
    file(APPEND "${PROPS_FILE}" "  </PropertyGroup>\n")
    file(APPEND "${PROPS_FILE}" "</Project>\n")

    # Tell Visual Studio to import our props file (use relative path to avoid space issues)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_USER_PROPS "${PROJECT_NAME}.props")
endif()

# Force static linking of runtime library to reduce dependencies
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Also ensure CommonLibSSE uses static runtime
    if(TARGET ${CommonLibName})
        set_property(TARGET ${CommonLibName} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # Additional linker flags to embed dependencies
    target_link_options(${PROJECT_NAME} PRIVATE
        /INCREMENTAL:NO
        /OPT:REF
        /OPT:ICF
    )
endif()

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    # If you perform a "Debug" build, also copy .pdb file (for debug symbols)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()
