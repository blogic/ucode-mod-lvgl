# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 John Crispin <john@phrozen.org>

cmake_minimum_required(VERSION 3.13)
project(ucode-lv C)

set(CMAKE_C_STANDARD 11)

IF(APPLE)
  SET(UCODE_MODULE_LINK_OPTIONS "LINKER:-undefined,dynamic_lookup")
ELSE()
  # Not CMAKE_SHARED_LIBRARY_LINK_C_FLAGS: that holds the flags CMake adds when
  # linking executables against shared libraries, and is never consulted for a
  # MODULE, so the section flags below were paid for and never collected.
  SET(UCODE_MODULE_LINK_OPTIONS "LINKER:--gc-sections")
ENDIF()

FIND_LIBRARY(libdrm NAMES drm)
FIND_LIBRARY(liblvgl NAMES lvgl)
FIND_PATH(ucode_include_dir NAMES ucode/module.h)
FIND_PATH(drm_include_dir NAMES drm.h PATH_SUFFIXES libdrm)
FIND_PATH(lvgl_include_dir NAMES lvgl.h PATH_SUFFIXES lvgl)

# Fail here rather than at the link. A missing library otherwise reaches the
# linker as -llibdrm-NOTFOUND, and a missing header as an include path called
# ucode_include_dir-NOTFOUND, which reports itself as a missing module.h.
IF(NOT libdrm)
  MESSAGE(FATAL_ERROR "libdrm not found")
ENDIF()

IF(NOT liblvgl)
  MESSAGE(FATAL_ERROR "liblvgl not found")
ENDIF()

IF(NOT ucode_include_dir)
  MESSAGE(FATAL_ERROR "ucode/module.h not found")
ENDIF()

IF(NOT drm_include_dir)
  MESSAGE(FATAL_ERROR "drm.h not found")
ENDIF()

IF(NOT lvgl_include_dir)
  MESSAGE(FATAL_ERROR "lvgl.h not found")
ENDIF()

INCLUDE_DIRECTORIES(
  ${ucode_include_dir}
  ${drm_include_dir}
  ${lvgl_include_dir}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# LV_LVGL_H_INCLUDE_SIMPLE picks "lvgl.h" over "lvgl/lvgl.h" in the generated
# font sources; only the former resolves against lvgl_include_dir. The library
# installs its own lv_conf.h beside lvgl.h, so LV_CONF_INCLUDE_SIMPLE reaches
# the same configuration that LVGL itself was compiled with.
ADD_DEFINITIONS(-DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE)

ADD_LIBRARY(lv_lib MODULE
  ucode-lv.c
  ucode-lv-obj.c
)

SET_TARGET_PROPERTIES(lv_lib PROPERTIES OUTPUT_NAME lv PREFIX "")
# No unwind tables: pure C with no exceptions, and ucode reports errors by
# return value rather than unwinding, so nothing reads .eh_frame at run time.
# Both flags are needed. On aarch64 the asynchronous one alone still emits the
# tables; measured, it changes nothing without -fno-unwind-tables beside it.
TARGET_COMPILE_OPTIONS(lv_lib PRIVATE -ffunction-sections -fdata-sections
				      -fno-asynchronous-unwind-tables
				      -fno-unwind-tables)
TARGET_LINK_OPTIONS(lv_lib PRIVATE ${UCODE_MODULE_LINK_OPTIONS})
TARGET_LINK_LIBRARIES(lv_lib ${liblvgl} ${libdrm} m)

INSTALL(TARGETS lv_lib LIBRARY DESTINATION lib/ucode)
