cmake_minimum_required(VERSION 3.5.0...4.3.0)

project(tfsxml C)

set(tfsxml_MAJOR_VERSION 0)
set(tfsxml_MINOR_VERSION 1)
set(tfsxml_PATCH_VERSION 0)
set(tfsxml_VERSION ${tfsxml_MAJOR_VERSION}.${tfsxml_MINOR_VERSION}.${tfsxml_PATCH_VERSION})

if(WIN32)
  set(BUILD_SHARED_LIBS OFF)
elseif(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build shared libs" ON)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(tfsxml_IS_TOP_LEVEL ON)
else()
  set(tfsxml_IS_TOP_LEVEL OFF)
endif()
option(TFSXML_BUILD_TESTS "Build the regression test tool and register the tests" ${tfsxml_IS_TOP_LEVEL})
option(TFSXML_BUILD_EXAMPLES "Build the examples" ${tfsxml_IS_TOP_LEVEL})
option(TFSXML_BUILD_FUZZER "Build the fuzzer (libFuzzer with Clang or MSVC, standalone driver otherwise)" OFF)
set(TFSXML_FUZZER_SANITIZERS "address,undefined" CACHE STRING "Sanitizers of the fuzzer (GCC and Clang, empty for none)")
set(TFSXML_FUZZ_REFERENCE "" CACHE FILEPATH "Reference tfsxml.c (with its tfsxml.h) for differential fuzzing")
get_filename_component(tfsxml_TESTING_FILES_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../../../tfsxml-RegressionTestingFiles" ABSOLUTE)
set(TFSXML_TESTING_FILES "${tfsxml_TESTING_FILES_DEFAULT}" CACHE PATH "Clone of https://github.com/MediaArea/tfsxml-RegressionTestingFiles (test files)")
option(TFSXML_BUILD_TINYXML2 "Build the tinyxml2 compatibility layer (C++)" ON)
set(TFSXML_TINYXML2_REFERENCE "" CACHE PATH "Directory with tinyxml2.cpp and tinyxml2.h of tinyxml2 6.2.0, for comparing the compatibility layer with tinyxml2")

set(tfsxml_SOURCES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../Source)

# On Windows debug library should have 'd' postfix.
if(WIN32)
  set(CMAKE_DEBUG_POSTFIX "d")
elseif(APPLE)
  set(CMAKE_DEBUG_POSTFIX "_debug")
endif()

include(GNUInstallDirs)

set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE STRING "Directory where binary will install")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE STRING "Directory where library will install")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}" CACHE PATH "The directory the headers are installed in")

if(MSVC)
    if(NOT "${CMAKE_VERSION}" VERSION_LESS "3.13")
        add_compile_options(/GS /W3 /guard:cf /sdl)
        add_link_options(/guard:cf)
        if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL ARM64)
            add_compile_options(/guard:signret)
        endif()
        if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL x64)
            add_link_options(/CETCOMPAT)
        endif()
        if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL X86)
            add_link_options(/CETCOMPAT)
        endif()
    else()
        message(WARNING "CMake < 3.13: Compiler/linker security mitigation flags for MSVC are not automatically added.")
    endif()
endif()

if(VCPKG_TARGET_TRIPLET)
    set(tfsxml_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/tfsxml")
elseif(MSVC)
    set(tfsxml_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/cmake")
else()
    set(tfsxml_CONFIG_INSTALL_DIR "${LIB_INSTALL_DIR}/cmake/tfsxml")
endif()

if(MSVC)
  set(tfsxml_WARNINGS /W4 /wd4996)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(tfsxml_WARNINGS -Wall -Wextra)
endif()

if(TFSXML_BUILD_TINYXML2)
  enable_language(CXX)
  if(MSVC)
    set(tfsxml_CXX_WARNINGS /W4 /wd4996)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(tfsxml_CXX_WARNINGS -Wall -Wextra)
  endif()
endif()

#############################################################################
# Library

set(tfsxml_HDRS
  ${tfsxml_SOURCES_PATH}/tfsxml/tfsxml.h
  )

set(tfsxml_SRCS
  ${tfsxml_SOURCES_PATH}/tfsxml/tfsxml.c
  )

add_library(tfsxml ${tfsxml_SRCS} ${tfsxml_HDRS})
target_compile_options(tfsxml PRIVATE ${tfsxml_WARNINGS})
set_property(TARGET tfsxml PROPERTY C_STANDARD 90)

target_include_directories(tfsxml PUBLIC
  $<BUILD_INTERFACE:${tfsxml_SOURCES_PATH}>
  $<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>)

set_target_properties(tfsxml PROPERTIES
  VERSION ${tfsxml_VERSION}
  SOVERSION ${tfsxml_MAJOR_VERSION}
  PUBLIC_HEADER "${tfsxml_HDRS}"
  )

install(TARGETS tfsxml EXPORT tfsxml-export
  PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}/tfsxml
  RUNTIME DESTINATION ${BIN_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

#############################################################################
# tinyxml2 compatibility layer

if(TFSXML_BUILD_TINYXML2)
  set(tfsxml_tinyxml2_HDRS
    ${tfsxml_SOURCES_PATH}/tfsxml/tfsxml_tinyxml2.h
    )
  add_library(tfsxml_tinyxml2 ${tfsxml_SOURCES_PATH}/tfsxml/tfsxml_tinyxml2.cpp ${tfsxml_tinyxml2_HDRS})
  target_link_libraries(tfsxml_tinyxml2 PUBLIC tfsxml)
  target_compile_options(tfsxml_tinyxml2 PRIVATE ${tfsxml_CXX_WARNINGS})
  set_target_properties(tfsxml_tinyxml2 PROPERTIES
    CXX_STANDARD 11
    VERSION ${tfsxml_VERSION}
    SOVERSION ${tfsxml_MAJOR_VERSION}
    PUBLIC_HEADER "${tfsxml_tinyxml2_HDRS}"
    )
  install(TARGETS tfsxml_tinyxml2 EXPORT tfsxml-export
    PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}/tfsxml
    RUNTIME DESTINATION ${BIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})
endif()

install(EXPORT tfsxml-export DESTINATION ${tfsxml_CONFIG_INSTALL_DIR} FILE tfsxmlTargets.cmake)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

# pkg-config
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  set(tfsxml_PKGCONFIG_LIBRARY "tfsxml")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libtfsxml.pc.in
                 ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml.pc
                 @ONLY)

  if(CMAKE_DEBUG_POSTFIX)
    set(tfsxml_PKGCONFIG_LIBRARY "tfsxml${CMAKE_DEBUG_POSTFIX}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libtfsxml.pc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml-debug.pc
                   @ONLY)
  endif()

  if(CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_DEBUG_POSTFIX)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml.pc
              DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
              CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml-debug.pc
              DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
              RENAME libtfsxml.pc
              CONFIGURATIONS Debug)
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml.pc
              DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
    endif()
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_DEBUG_POSTFIX)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml-debug.pc
            DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
            RENAME libtfsxml.pc)
  else()
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libtfsxml.pc
            DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
  endif()
endif()

# cmake-modules
include(CMakePackageConfigHelpers)
configure_package_config_file(
  tfsxmlConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/tfsxmlConfig.cmake"
  INSTALL_DESTINATION ${tfsxml_CONFIG_INSTALL_DIR}
  PATH_VARS INCLUDE_INSTALL_DIR)

write_basic_package_version_file(tfsxmlConfigVersion.cmake VERSION ${tfsxml_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/tfsxmlConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/tfsxmlConfigVersion.cmake
  DESTINATION ${tfsxml_CONFIG_INSTALL_DIR})

if(NOT TARGET uninstall)
  add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

#############################################################################
# Examples

if(TFSXML_BUILD_EXAMPLES)
  add_executable(tfsxml_HowToUse ${tfsxml_SOURCES_PATH}/Example/HowToUse.c)
  target_link_libraries(tfsxml_HowToUse PRIVATE tfsxml)
  target_compile_options(tfsxml_HowToUse PRIVATE ${tfsxml_WARNINGS})
  add_executable(tfsxml_HowToUse_Stream ${tfsxml_SOURCES_PATH}/Example/HowToUse_Stream.c)
  target_link_libraries(tfsxml_HowToUse_Stream PRIVATE tfsxml)
  target_compile_options(tfsxml_HowToUse_Stream PRIVATE ${tfsxml_WARNINGS})
endif()

#############################################################################
# Regression tests

set(tfsxml_TEST_SRCS
  ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest.c
  ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_Api.c
  ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_Speed.c
  ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_Trace.c
  )

if(TFSXML_BUILD_TESTS)
  enable_testing()
  add_executable(tfsxml_RegressionTest ${tfsxml_TEST_SRCS})
  target_link_libraries(tfsxml_RegressionTest PRIVATE tfsxml)
  target_compile_options(tfsxml_RegressionTest PRIVATE ${tfsxml_WARNINGS})
  if(EXISTS "${TFSXML_TESTING_FILES}/Files")
    add_test(NAME tfsxml_RegressionTest
             COMMAND tfsxml_RegressionTest ${TFSXML_TESTING_FILES}/Files)
    add_test(NAME tfsxml_HowToUse
             COMMAND tfsxml_HowToUse ${TFSXML_TESTING_FILES}/Files/Formats/Adm.xml)
    add_test(NAME tfsxml_HowToUse_Stream
             COMMAND tfsxml_HowToUse_Stream ${TFSXML_TESTING_FILES}/Files/Formats/Adm.xml 7)
    if(NOT TFSXML_BUILD_EXAMPLES)
      set_tests_properties(tfsxml_HowToUse tfsxml_HowToUse_Stream PROPERTIES DISABLED ON)
    endif()
  else()
    message(WARNING "Test files not found in ${TFSXML_TESTING_FILES}, only the API unit tests are run. "
                    "Clone https://github.com/MediaArea/tfsxml-RegressionTestingFiles next to the tfsxml directory or set TFSXML_TESTING_FILES.")
    add_test(NAME tfsxml_RegressionTest
             COMMAND tfsxml_RegressionTest)
  endif()

  if(TFSXML_BUILD_TINYXML2)
    # Expected results of the compatibility layer
    add_executable(tfsxml_RegressionTest_TinyXml2 ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_TinyXml2.cpp)
    target_link_libraries(tfsxml_RegressionTest_TinyXml2 PRIVATE tfsxml_tinyxml2)
    target_compile_options(tfsxml_RegressionTest_TinyXml2 PRIVATE ${tfsxml_CXX_WARNINGS})
    set_property(TARGET tfsxml_RegressionTest_TinyXml2 PROPERTY CXX_STANDARD 11)
    if(EXISTS "${TFSXML_TESTING_FILES}/TinyXml2")
      add_test(NAME tfsxml_RegressionTest_TinyXml2
               COMMAND tfsxml_RegressionTest_TinyXml2 ${TFSXML_TESTING_FILES}/TinyXml2)
    else()
      message(WARNING "Test files of the tinyxml2 compatibility layer not found in ${TFSXML_TESTING_FILES}/TinyXml2, its tests are not run.")
    endif()

    # Comparison with tinyxml2
    if(TFSXML_TINYXML2_REFERENCE)
      add_executable(tfsxml_RegressionTest_TinyXml2_Reference
        ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_TinyXml2.cpp
        ${tfsxml_SOURCES_PATH}/tfsxml/tfsxml_tinyxml2.cpp
        ${TFSXML_TINYXML2_REFERENCE}/tinyxml2.cpp)
      target_link_libraries(tfsxml_RegressionTest_TinyXml2_Reference PRIVATE tfsxml)
      target_include_directories(tfsxml_RegressionTest_TinyXml2_Reference PRIVATE ${TFSXML_TINYXML2_REFERENCE})
      target_compile_definitions(tfsxml_RegressionTest_TinyXml2_Reference PRIVATE TFSXML_TINYXML2_REFERENCE TFSXML_TINYXML2_NAMESPACE=tfsxml_tinyxml2)
      set_property(TARGET tfsxml_RegressionTest_TinyXml2_Reference PROPERTY CXX_STANDARD 11)
      if(EXISTS "${TFSXML_TESTING_FILES}/TinyXml2" AND EXISTS "${TFSXML_TESTING_FILES}/Files")
        add_test(NAME tfsxml_RegressionTest_TinyXml2_Reference
                 COMMAND tfsxml_RegressionTest_TinyXml2_Reference --reference ${TFSXML_TESTING_FILES}/TinyXml2 ${TFSXML_TESTING_FILES}/Files)
      endif()
    endif()
  endif()
endif()

#############################################################################
# Fuzzer

if(TFSXML_BUILD_FUZZER)
  set(tfsxml_FUZZ_SRCS
    ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz.c
    ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Reference.c
    ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Library.c
    ${tfsxml_SOURCES_PATH}/RegressionTest/RegressionTest_Trace.c
    )

  set(tfsxml_FUZZ_FLAGS "")
  set(tfsxml_FUZZ_LINK_FLAGS "")
  set(tfsxml_FUZZ_LIBFUZZER OFF)
  if(MSVC)
    set(tfsxml_FUZZ_LIBFUZZER ON)
    list(APPEND tfsxml_FUZZ_FLAGS /fsanitize=address /fsanitize=fuzzer /Zi)
    list(APPEND tfsxml_FUZZ_LINK_FLAGS /INCREMENTAL:NO)
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(tfsxml_FUZZ_LIBFUZZER ON)
    if(TFSXML_FUZZER_SANITIZERS)
      list(APPEND tfsxml_FUZZ_FLAGS -fsanitize=fuzzer,${TFSXML_FUZZER_SANITIZERS} -fno-sanitize-recover=all)
      list(APPEND tfsxml_FUZZ_LINK_FLAGS -fsanitize=fuzzer,${TFSXML_FUZZER_SANITIZERS})
    else()
      list(APPEND tfsxml_FUZZ_FLAGS -fsanitize=fuzzer)
      list(APPEND tfsxml_FUZZ_LINK_FLAGS -fsanitize=fuzzer)
    endif()
    list(APPEND tfsxml_FUZZ_FLAGS -g -fno-omit-frame-pointer)
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(TFSXML_FUZZER_SANITIZERS)
      list(APPEND tfsxml_FUZZ_FLAGS -fsanitize=${TFSXML_FUZZER_SANITIZERS} -fno-sanitize-recover=all)
      list(APPEND tfsxml_FUZZ_LINK_FLAGS -fsanitize=${TFSXML_FUZZER_SANITIZERS})
    endif()
    list(APPEND tfsxml_FUZZ_FLAGS -g -fno-omit-frame-pointer)
  endif()

  if(NOT tfsxml_FUZZ_LIBFUZZER)
    # Standalone driver, with coverage-guided fuzzing if available
    list(APPEND tfsxml_FUZZ_SRCS ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Main.c)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
      set_source_files_properties(${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Main.c PROPERTIES COMPILE_DEFINITIONS TFSXML_FUZZ_COVERAGE)
      set_source_files_properties(${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Library.c PROPERTIES COMPILE_FLAGS -fsanitize-coverage=trace-pc)
    endif()
  endif()

  add_executable(tfsxml_Fuzz ${tfsxml_FUZZ_SRCS})
  target_include_directories(tfsxml_Fuzz PRIVATE ${tfsxml_SOURCES_PATH})
  target_compile_options(tfsxml_Fuzz PRIVATE ${tfsxml_FUZZ_FLAGS})
  string(REPLACE ";" " " tfsxml_FUZZ_LINK_FLAGS_STRING "${tfsxml_FUZZ_LINK_FLAGS}")
  if(NOT "${CMAKE_VERSION}" VERSION_LESS "3.13")
    target_link_options(tfsxml_Fuzz PRIVATE ${tfsxml_FUZZ_LINK_FLAGS})
  else()
    set_target_properties(tfsxml_Fuzz PROPERTIES LINK_FLAGS "${tfsxml_FUZZ_LINK_FLAGS_STRING}")
  endif()
  if(TFSXML_FUZZ_REFERENCE)
    target_compile_definitions(tfsxml_Fuzz PRIVATE "TFSXML_FUZZ_REFERENCE=\"${TFSXML_FUZZ_REFERENCE}\"")
  endif()

  # tinyxml2 compatibility layer, compared with tinyxml2 if available
  # (the fuzzer flags are the ones of the C compiler, the C++ compiler must be of the same kind)
  if(TFSXML_BUILD_TINYXML2 AND NOT CMAKE_CXX_COMPILER_ID STREQUAL CMAKE_C_COMPILER_ID)
    message(STATUS "tfsxml_Fuzz_TinyXml2 is not built: the C++ compiler (${CMAKE_CXX_COMPILER_ID}) is not of the same kind as the C compiler (${CMAKE_C_COMPILER_ID}), set CMAKE_CXX_COMPILER (e.g. clang++ with clang)")
  elseif(TFSXML_BUILD_TINYXML2)
    set(tfsxml_FUZZ_TINYXML2_SRCS
      ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_TinyXml2.cpp
      ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Library.c
      ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Library_TinyXml2.cpp
      )
    if(TFSXML_TINYXML2_REFERENCE)
      list(APPEND tfsxml_FUZZ_TINYXML2_SRCS ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Reference_TinyXml2.cpp)
    endif()
    if(NOT tfsxml_FUZZ_LIBFUZZER)
      list(APPEND tfsxml_FUZZ_TINYXML2_SRCS ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Main.c)
      if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
        set_source_files_properties(
          ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Library_TinyXml2.cpp
          ${tfsxml_SOURCES_PATH}/Fuzz/Fuzz_Reference_TinyXml2.cpp
          PROPERTIES COMPILE_FLAGS -fsanitize-coverage=trace-pc)
      endif()
    endif()
    add_executable(tfsxml_Fuzz_TinyXml2 ${tfsxml_FUZZ_TINYXML2_SRCS})
    target_include_directories(tfsxml_Fuzz_TinyXml2 PRIVATE ${tfsxml_SOURCES_PATH})
    target_compile_options(tfsxml_Fuzz_TinyXml2 PRIVATE ${tfsxml_FUZZ_FLAGS})
    set_property(TARGET tfsxml_Fuzz_TinyXml2 PROPERTY CXX_STANDARD 11)
    if(NOT "${CMAKE_VERSION}" VERSION_LESS "3.13")
      target_link_options(tfsxml_Fuzz_TinyXml2 PRIVATE ${tfsxml_FUZZ_LINK_FLAGS})
    else()
      set_target_properties(tfsxml_Fuzz_TinyXml2 PROPERTIES LINK_FLAGS "${tfsxml_FUZZ_LINK_FLAGS_STRING}")
    endif()
    if(TFSXML_TINYXML2_REFERENCE)
      target_include_directories(tfsxml_Fuzz_TinyXml2 PRIVATE ${TFSXML_TINYXML2_REFERENCE})
      target_compile_definitions(tfsxml_Fuzz_TinyXml2 PRIVATE TFSXML_TINYXML2_REFERENCE TFSXML_TINYXML2_NAMESPACE=tfsxml_tinyxml2)
    endif()
  endif()
endif()
