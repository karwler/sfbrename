cmake_minimum_required(VERSION 3.10.3)
project(sfbrename LANGUAGES C)
if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(APPIMAGE "Package as an AppImage." OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	option(NATIVE "Build for the current CPU." OFF)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	option(APPIMAGE "Package as an AppImage." OFF)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})
if(APPIMAGE)
	add_definitions(-DAPPIMAGE)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	set(FLG_ANY "-rdynamic -Wall -Wextra -pedantic-errors -Wdouble-promotion -Wshadow -Wunreachable-code -Wno-unused-parameter")
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		set(FLG_ANY "${FLG_ANY} -Wcast-align=strict -Wduplicated-cond")
	else()
		set(FLG_ANY "${FLG_ANY} -Wcast-align -Wnull-dereference -Wover-aligned -Wshadow-all")
	endif()

	if(NATIVE)
		set(FLG_ANY "${FLG_ANY} -march=native")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(FLG_ANY "${FLG_ANY} -mmmx -msse -msse2")
	endif()
	set(FLG_DEBUG "-g")
	set(FLG_RELEASE "-O3")
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		set(FLG_RELEASE "${FLG_RELEASE} -flto -fuse-linker-plugin -s")
	endif()
endif()

if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FLG_ANY} ${FLG_DEBUG}")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${FLG_ANY} ${FLG_RELEASE}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLG_ANY} ${FLG_DEBUG}")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLG_ANY} ${FLG_RELEASE}")
endif()

set(SRC_FILES
	"src/arguments.c"
	"src/arguments.h"
	"src/main.c"
	"src/main.h"
	"src/rename.c"
	"src/rename.h")

add_executable(sfbrename ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${GTK3_LIBRARIES})

set(PBOUT_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")
set(EXECUTABLE_OUTPUT_PATH "${PBOUT_DIR}/bin")
set_target_properties(${PROJECT_NAME} PROPERTIES
	                    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_DEBUG "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_RELEASE "${EXECUTABLE_OUTPUT_PATH}")

set(DATA_DIR "${PBOUT_DIR}/share/${PROJECT_NAME}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	                COMMAND "${CMAKE_COMMAND}" -E make_directory "${DATA_DIR}"
					COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/rsc/main.glade" "${DATA_DIR}"
					COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/rsc/${PROJECT_NAME}.desktop" "${PBOUT_DIR}")
find_program(SED "sed")
if(SED)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		                COMMAND "${CMAKE_SOURCE_DIR}/rsc/spacerm.sh" "${DATA_DIR}/main.glade")
endif()
find_program(GZIP "gzip")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	                COMMAND "${GZIP}" -f "${DATA_DIR}/main.glade")

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") AND (NOT CMAKE_BUILD_TYPE STREQUAL "Debug"))
	find_program(STRIPPER "strip")
	if(STRIPPER)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			                COMMAND "${STRIPPER}" -s "${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME_LOWER}")
	endif()
endif()
if(APPIMAGE)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		                COMMAND "${CMAKE_COMMAND}" -E create_symlink "bin/${PROJECT_NAME}" "${PBOUT_DIR}/AppRun"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/rsc/${PROJECT_NAME}.png" "${PBOUT_DIR}")

	find_program(APPIMAGETOOL "appimagetool")
	if(APPIMAGETOOL)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			                COMMAND "${APPIMAGETOOL}" "${PBOUT_DIR}"
							COMMAND "${CMAKE_COMMAND}" -E rename "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-*.AppImage" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.AppImage")
	endif()
else()
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/rsc/${PROJECT_NAME}.png" "${DATA_DIR}")
endif()

if(APPIMAGE)
	install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.AppImage" DESTINATION "bin")
else()
	set(DST_DIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}")
	install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
	install(DIRECTORY "${DATA_DIR}" DESTINATION "share")
endif()

if(APPIMAGE)
	add_custom_target("uninstall"
		                COMMAND "${CMAKE_COMMAND}" -E remove -f "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}.AppImage"
						WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
else()
	add_custom_target("uninstall"
		                COMMAND "${CMAKE_COMMAND}" -E remove -f "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}"
						COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}"
						WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

foreach(FSRC IN LISTS SRC_FILES)
	get_filename_component(FGRP "${FSRC}" DIRECTORY)
	string(REPLACE "/" ";" FGRP "${FGRP}")
	list(REMOVE_AT FGRP 0)
	string(REPLACE ";" "/" FGRP "${FGRP}")
	source_group("${FGRP}" FILES "${FSRC}")
endforeach()
