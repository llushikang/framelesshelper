#[[
  MIT License

  Copyright (C) 2021-2023 by wangwenx190 (Yuhang Zhao)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
]]

if(FRAMELESSHELPER_ENABLE_UNIVERSAL_BUILD)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

set(SUB_MODULE Widgets)
set(SUB_MODULE_FULL_NAME ${PROJECT_NAME}${SUB_MODULE})
set(SUB_MODULE_TARGET ${SUB_MODULE_FULL_NAME})
set(SUB_MODULE_PATH ${PROJECT_NAME}/${SUB_MODULE})

set(INCLUDE_PREFIX ../../include/${SUB_MODULE_PATH})

set(PUBLIC_HEADERS
    ${INCLUDE_PREFIX}/framelesshelperwidgets_global.h
    ${INCLUDE_PREFIX}/framelesswidget.h
    ${INCLUDE_PREFIX}/framelessmainwindow.h
    ${INCLUDE_PREFIX}/standardsystembutton.h
    ${INCLUDE_PREFIX}/framelesswidgetshelper.h
    ${INCLUDE_PREFIX}/standardtitlebar.h
    ${INCLUDE_PREFIX}/framelessdialog.h
)

set(PUBLIC_HEADERS_ALIAS
    ${INCLUDE_PREFIX}/Global
    ${INCLUDE_PREFIX}/FramelessWidget
    ${INCLUDE_PREFIX}/FramelessMainWindow
    ${INCLUDE_PREFIX}/StandardSystemButton
    ${INCLUDE_PREFIX}/FramelessWidgetsHelper
    ${INCLUDE_PREFIX}/StandardTitleBar
    ${INCLUDE_PREFIX}/FramelessDialog
)

set(PRIVATE_HEADERS
    ${INCLUDE_PREFIX}/private/framelesswidgetshelper_p.h
    ${INCLUDE_PREFIX}/private/standardsystembutton_p.h
    ${INCLUDE_PREFIX}/private/standardtitlebar_p.h
    ${INCLUDE_PREFIX}/private/framelesswidget_p.h
    ${INCLUDE_PREFIX}/private/framelessmainwindow_p.h
    ${INCLUDE_PREFIX}/private/widgetssharedhelper_p.h
    ${INCLUDE_PREFIX}/private/framelessdialog_p.h
)

set(SOURCES
    framelessmainwindow.cpp
    framelesswidgetshelper.cpp
    framelesswidget.cpp
    standardsystembutton.cpp
    standardtitlebar.cpp
    widgetssharedhelper.cpp
    framelesshelperwidgets_global.cpp
    framelessdialog.cpp
)

if(WIN32 AND NOT FRAMELESSHELPER_BUILD_STATIC)
    set(__rc_path "${CMAKE_CURRENT_BINARY_DIR}/${SUB_MODULE_FULL_NAME}.rc")
    if(NOT EXISTS "${__rc_path}")
        generate_win32_rc_file(
            PATH "${__rc_path}"
            VERSION "${PROJECT_VERSION}"
            COMPANY "wangwenx190"
            DESCRIPTION "${PROJECT_NAME} ${SUB_MODULE} Module"
            COPYRIGHT "MIT License"
            ORIGINAL_FILENAME "${SUB_MODULE_FULL_NAME}.dll"
            PRODUCT "${PROJECT_NAME}"
            COMMENTS "Built from commit ${PROJECT_VERSION_COMMIT} on ${PROJECT_COMPILE_DATETIME} (UTC)."
            LIBRARY
        )
    endif()
    list(APPEND SOURCES "${__rc_path}")
endif()

set(ALL_SOURCES ${PUBLIC_HEADERS} ${PRIVATE_HEADERS} ${SOURCES})

if(FRAMELESSHELPER_BUILD_STATIC)
    set(SUB_MOD_LIB_TYPE "STATIC")
else()
    set(SUB_MOD_LIB_TYPE "SHARED")
endif()
add_library(${SUB_MODULE_TARGET} ${SUB_MOD_LIB_TYPE} ${ALL_SOURCES})
add_library(${PROJECT_NAME}::${SUB_MODULE} ALIAS ${SUB_MODULE_TARGET})
add_library(${PROJECT_NAME}::${SUB_MODULE_FULL_NAME} ALIAS ${SUB_MODULE_TARGET})

set_target_properties(${SUB_MODULE_TARGET} PROPERTIES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

if(FRAMELESSHELPER_BUILD_STATIC)
    target_compile_definitions(${SUB_MODULE_TARGET} PUBLIC FRAMELESSHELPER_WIDGETS_STATIC)
endif()

if(FRAMELESSHELPER_NO_DEBUG_OUTPUT)
    target_compile_definitions(${SUB_MODULE_TARGET} PRIVATE
        FRAMELESSHELPER_WIDGETS_NO_DEBUG_OUTPUT
    )
endif()

if(FRAMELESSHELPER_NO_BUNDLE_RESOURCE)
    target_compile_definitions(${SUB_MODULE_TARGET} PUBLIC FRAMELESSHELPER_WIDGETS_NO_BUNDLE_RESOURCE)
endif()

if(FRAMELESSHELPER_NO_PRIVATE)
    target_compile_definitions(${SUB_MODULE_TARGET} PUBLIC FRAMELESSHELPER_WIDGETS_NO_PRIVATE)
endif()

if(DEFINED FRAMELESSHELPER_NAMESPACE)
    if("x${FRAMELESSHELPER_NAMESPACE}" STREQUAL "x")
        message(FATAL_ERROR "FRAMELESSHELPER_NAMESPACE can't be empty!")
    endif()
    target_compile_definitions(${SUB_MODULE_TARGET} PUBLIC FRAMELESSHELPER_NAMESPACE=${FRAMELESSHELPER_NAMESPACE})
endif()

target_compile_definitions(${SUB_MODULE_TARGET} PRIVATE
    FRAMELESSHELPER_WIDGETS_LIBRARY
)

target_link_libraries(${SUB_MODULE_TARGET} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_link_libraries(${SUB_MODULE_TARGET} PUBLIC
    ${PROJECT_NAME}::Core
)

include(GNUInstallDirs)
target_include_directories(${SUB_MODULE_TARGET} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_PREFIX}/../..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_PREFIX}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_PREFIX}/private>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${SUB_MODULE_PATH}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${SUB_MODULE_PATH}/private>"
)

if(NOT FRAMELESSHELPER_BUILD_STATIC)
    setup_target_rpaths(TARGETS ${SUB_MODULE_TARGET})
endif()

setup_qt_stuff(TARGETS ${SUB_MODULE_TARGET})

set(__extra_flags "")
if(NOT FRAMELESSHELPER_NO_PERMISSIVE_CHECKS)
    list(APPEND __extra_flags PERMISSIVE)
endif()
if(FRAMELESSHELPER_ENABLE_SPECTRE)
    list(APPEND __extra_flags SPECTRE)
endif()
if(FRAMELESSHELPER_ENABLE_EHCONTGUARD)
    list(APPEND __extra_flags EHCONTGUARD)
endif()
if(FRAMELESSHELPER_ENABLE_INTELCET)
    list(APPEND __extra_flags INTELCET)
endif()
if(FRAMELESSHELPER_ENABLE_INTELJCC)
    list(APPEND __extra_flags INTELJCC)
endif()
if(FRAMELESSHELPER_ENABLE_CFGUARD)
    list(APPEND __extra_flags CFGUARD)
endif()
if(FRAMELESSHELPER_FORCE_LTO)
    list(APPEND __extra_flags FORCE_LTO)
endif()
setup_compile_params(TARGETS ${SUB_MODULE_TARGET} ${__extra_flags})

if(NOT FRAMELESSHELPER_NO_INSTALL)
    setup_package_export(
        TARGETS ${SUB_MODULE_TARGET}
        NAMESPACE ${PROJECT_NAME}
        PACKAGE_NAME ${PROJECT_NAME}
        COMPONENT ${SUB_MODULE}
        PUBLIC_HEADERS ${PUBLIC_HEADERS}
        ALIAS_HEADERS ${PUBLIC_HEADERS_ALIAS}
        PRIVATE_HEADERS ${PRIVATE_HEADERS}
    )
endif()

if(NOT FRAMELESSHELPER_NO_SUMMARY)
    dump_target_info(TARGETS ${SUB_MODULE_TARGET})
endif()
