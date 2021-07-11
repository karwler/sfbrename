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

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

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
set(RSC_FILES
	"rsc/main.glade")

add_executable(sfbrename ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${GTK3_LIBRARIES})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/rsc/main.glade" "${CMAKE_BINARY_DIR}")

set(ALL_FILES ${SRC_FILES} ${RSC_FILES})
foreach(FSRC IN LISTS ALL_FILES)
	get_filename_component(FGRP "${FSRC}" DIRECTORY)
	string(REPLACE "/" ";" FGRP "${FGRP}")
	list(REMOVE_AT FGRP 0)
	string(REPLACE ";" "/" FGRP "${FGRP}")
	source_group("${FGRP}" FILES "${FSRC}")
endforeach()
