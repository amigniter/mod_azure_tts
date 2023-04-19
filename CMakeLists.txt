cmake_minimum_required(VERSION 3.18)
project(mod_azure_tts)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

if(NOT DEFINED AZURE_PATH)
    message("Please set path to SpeechSDK with: -DAZURE_PATH=/path/to/SpeechSDK")
else()
    message("Path to SpeechSDK: ${AZURE_PATH}")
endif()

if (NOT IS_DIRECTORY "${AZURE_PATH}/include/c_api")
    message(FATAL_ERROR "Path to SpeechSDK is invalid")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(FreeSWITCH REQUIRED IMPORTED_TARGET freeswitch)
pkg_get_variable(FS_MOD_DIR freeswitch modulesdir)
message("FreeSWITCH modules dir: ${FS_MOD_DIR}")

add_library(mod_azure_tts SHARED mod_azure_tts.c azure_glue.cpp)

set_property(TARGET mod_azure_tts PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(mod_azure_tts
        PROPERTIES
        INSTALL_RPATH "${AZURE_PATH}/lib/x64"
        )

target_include_directories(mod_azure_tts PRIVATE "${AZURE_PATH}/include/c_api" "${AZURE_PATH}/include/cxx_api")
target_link_directories(mod_azure_tts PRIVATE "${AZURE_PATH}/lib/x64")

target_link_libraries(mod_azure_tts PRIVATE pthread Microsoft.CognitiveServices.Speech.core PkgConfig::FreeSWITCH)

install(TARGETS mod_azure_tts DESTINATION ${FS_MOD_DIR})