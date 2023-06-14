cmake_minimum_required(VERSION 3.12.4)
project(sfbrename VERSION 1.1.0 DESCRIPTION "Tool for batch renaming files" HOMEPAGE_URL "https://github.com/karwler/sfbrename" LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CONSOLE "Build for command line use only." OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	option(NATIVE "Build for the current CPU." OFF)
endif()
if(UNIX)
	option(APPIMAGE "Package as an AppImage." OFF)
endif()

find_package(PkgConfig REQUIRED)
if(CONSOLE)
	pkg_check_modules(PKGCONFS REQUIRED glib-2.0 gio-2.0)
else()
	pkg_check_modules(PKGCONFS REQUIRED gtk+-3.0)
endif()

include_directories(${PKGCONFS_INCLUDE_DIRS})
link_directories(${PKGCONFS_LIBRARY_DIRS})
add_definitions(${PKGCONFS_CFLAGS_OTHER})
add_compile_definitions($<$<BOOL:${CONSOLE}>:CONSOLE>)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-Wall -Wextra -pedantic-errors -Wdouble-promotion -Wfloat-conversion -Wformat=2 -Wshadow -Wunreachable-code -Wno-implicit-fallthrough -Wno-switch -Wno-unused-parameter $<$<NOT:$<BOOL:${MINGW}>>:-Wdouble-promotion>)
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-Wcast-align=strict -Wduplicated-cond)
	else()
		add_compile_options(-Wcast-align -Wimplicit-int-float-conversion -Wnull-dereference -Wover-aligned -Wshadow-all)
	endif()
	if(NATIVE)
		add_compile_options(-march=native)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		add_compile_options(-march=x86-64)
	endif()
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		add_compile_options("$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-flto;-fuse-linker-plugin>")
		add_link_options("$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-flto;-fuse-linker-plugin;-s>")
	endif()
	if(MINGW)
		add_link_options(-Wl,-subsystem,$<IF:$<BOOL:${CONSOLE}>,console,windows>)
	elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CONSOLE)
		add_link_options(-rdynamic)
	endif()
endif()

set(DIR_RSC "${CMAKE_SOURCE_DIR}/rsc")
set(SRC_FILES
	"src/arguments.c"
	"src/arguments.h"
	"src/common.h"
	"src/main.c"
	"src/main.h"
	"src/rename.c"
	"src/rename.h")
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${PKGCONFS_LIBRARIES})
if(MINGW)
	set(RESOURCE_OBJ "${CMAKE_BINARY_DIR}/sfbrename.res")
	execute_process(COMMAND windres "${DIR_RSC}/sfbrename.rc" -O coff -o "${RESOURCE_OBJ}")
	target_link_libraries(${PROJECT_NAME} gnurx "${RESOURCE_OBJ}")
endif()

if(APPIMAGE)
	set(PBOUT_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.AppDir")
	set(EXECUTABLE_OUTPUT_PATH "${PBOUT_DIR}/usr/bin")
	set(DATA_DIR "${PBOUT_DIR}/usr/share/${PROJECT_NAME}")
else()
	set(PBOUT_DIR "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")
	set(EXECUTABLE_OUTPUT_PATH "${PBOUT_DIR}/bin")
	set(DATA_DIR "${PBOUT_DIR}/share/${PROJECT_NAME}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
						RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_DEBUG "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_RELEASE "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${EXECUTABLE_OUTPUT_PATH}"
						RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${EXECUTABLE_OUTPUT_PATH}")
if(MINGW)
	execute_process(COMMAND where gdbus OUTPUT_VARIABLE GDBUS_LOC)
	string(REGEX REPLACE "\r?\n" ";" GDBUS_LOC "${GDBUS_LOC}")
	list(LENGTH GDBUS_LOC GDBUS_LEN)
	math(EXPR GDBUS_LEN "${GDBUS_LEN} - 1")
	foreach(ID RANGE ${GDBUS_LEN})
		list(GET GDBUS_LOC ${ID} GDBUS)
		if(GDBUS MATCHES "mingw" OR GDBUS MATCHES "msys")
			break()
		endif()
	endforeach()
	if(NOT GDBUS)
		list(GET GDBUS_LOC 0 GDBUS)
	endif()
	string(REPLACE "\\" "/" GDBUS "${GDBUS}")

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
						COMMAND bash -c "ldd '$<TARGET_FILE:${PROJECT_NAME}>' | grep '/mingw.*\\.dll' -o | xargs -I{} cp '{}' '$<TARGET_FILE_DIR:${PROJECT_NAME}>'"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${GDBUS}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
	if(NOT CONSOLE)
		string(REPLACE "/" ";" MINGW_LOC "${GDBUS}")
		list(LENGTH MINGW_LOC MINGW_LEN)
		math(EXPR MINGW_LEN "${MINGW_LEN} - 2")
		list(SUBLIST MINGW_LOC 0 ${MINGW_LEN} MINGW_LOC)
		string(REPLACE ";" "/" MINGW_LOC "${MINGW_LOC}")
		set(THEME_LOC "${MINGW_LOC}/share/icons")
		set(THEME_DIR "${PBOUT_DIR}/share/icons")

		file(GLOB PIXBUF_LOC "${MINGW_LOC}/lib/gdk-pixbuf-2.0/*")
		list(GET PIXBUF_LOC 0 PIXBUF_LOC)
		string(REGEX REPLACE "[\\/]" ";" PIXBUF_DIR "${PIXBUF_LOC}")
		list(LENGTH PIXBUF_DIR PIXBUF_LEN)
		math(EXPR PIXBUF_POS "${PIXBUF_LEN} - 3")
		list(SUBLIST PIXBUF_DIR ${PIXBUF_POS} 3 PIXBUF_DIR)
		string(REPLACE ";" "/" PIXBUF_DIR "${PIXBUF_DIR}")
		set(PIXBUF_DIR "${PBOUT_DIR}/${PIXBUF_DIR}")

		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
						COMMAND "${CMAKE_COMMAND}" -E copy_directory "${THEME_LOC}/Adwaita/16x16" "${THEME_DIR}/Adwaita/16x16"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${THEME_LOC}/Adwaita/icon-theme.cache" "${THEME_LOC}/Adwaita/index.theme" "${THEME_DIR}/Adwaita"
						COMMAND "${CMAKE_COMMAND}" -E copy_directory "${THEME_LOC}/hicolor/16x16" "${THEME_DIR}/hicolor/16x16"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${THEME_LOC}/hicolor/icon-theme.cache" "${THEME_LOC}/hicolor/index.theme" "${THEME_DIR}/hicolor"
						COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PIXBUF_LOC}" "${PIXBUF_DIR}")
	endif()
endif()

if(APPIMAGE OR NOT CONSOLE)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
						COMMAND "${CMAKE_COMMAND}" -E make_directory "${DATA_DIR}"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DIR_RSC}/${PROJECT_NAME}.png" "$<$<NOT:$<BOOL:${CONSOLE}>>:${DIR_RSC}/main.glade>" "${CMAKE_SOURCE_DIR}/LICENSE" "${DATA_DIR}")
endif()
if(NOT CONSOLE)
	find_program(GTK_TOOL "gtk-builder-tool")
	if(GTK_TOOL)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
							COMMAND "${GTK_TOOL}" simplify --replace "${DATA_DIR}/main.glade")
	else()
		message(WARNING "Can't simplify the glade file: Failed to find gtk-builder-tool")
	endif()

	find_program(SED "sed")
	if(SED)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
							COMMAND "${SED}" -i "'s/^\\s*</</g;/<!--.*/d'" "${DATA_DIR}/main.glade")
	else()
		message(WARNING "Can't reduce the size of the glade file: Failed to find sed")
	endif()

	find_program(GZIP "gzip")
	if(GZIP)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
							COMMAND "${GZIP}" -f "${DATA_DIR}/main.glade"
							COMMAND "${GZIP}" -f "${DATA_DIR}/LICENSE")
	else()
		message(WARNING "Can't compress the glade file: Failed to find gzip")
	endif()
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	find_program(STRIPPER "strip")
	if(STRIPPER)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
							COMMAND "$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:${STRIPPER};-s;$<TARGET_FILE:${PROJECT_NAME}>>"
							COMMAND_EXPAND_LISTS)
	else()
		message(WARNING "Can't strip the executable: Failed to find strip")
	endif()
endif()

if(APPIMAGE)
	find_program(LINUXDEPLOY "linuxdeploy")
	if(NOT LINUXDEPLOY)
		message(WARNING "Can't deploy the AppImage: Failed to find linuxdeploy")
	endif()
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
						COMMAND "${CMAKE_COMMAND}" -E create_symlink "usr/bin/${PROJECT_NAME}" "${PBOUT_DIR}/AppRun"
						COMMAND "${CMAKE_COMMAND}" -E create_symlink "usr/share/${PROJECT_NAME}/${PROJECT_NAME}.png" "${PBOUT_DIR}/${PROJECT_NAME}.png"
						COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DIR_RSC}/${PROJECT_NAME}.desktop" "${PBOUT_DIR}"
						COMMAND "$<$<BOOL:${LINUXDEPLOY}>:${LINUXDEPLOY};--appdir;${PBOUT_DIR};--output;appimage>"
						COMMAND_EXPAND_LISTS)
endif()

if(APPIMAGE)
	install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.AppImage" DESTINATION "bin")
else()
	install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
	if(NOT CONSOLE)
		install(DIRECTORY "${DATA_DIR}" DESTINATION "share")
	endif()
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

enable_testing()
add_test(NAME "tests" COMMAND bash -c "${DIR_RSC}/test.sh $<TARGET_FILE:${PROJECT_NAME}>")

foreach(FSRC IN LISTS SRC_FILES)
	get_filename_component(FGRP "${FSRC}" DIRECTORY)
	string(REPLACE "/" ";" FGRP "${FGRP}")
	list(REMOVE_AT FGRP 0)
	string(REPLACE ";" "/" FGRP "${FGRP}")
	source_group("${FGRP}" FILES "${FSRC}")
endforeach()
