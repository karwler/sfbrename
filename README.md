# sfbrename  

Simple Fucking Bulk Rename: A small tool for batch renaming files.  
I couldn't find any mass file renaming tool that I liked, so I wrote one myself.  
It runs on Linux and Windows with a GTK+ 3 GUI or can be operated using command line arguments.  
The program can also be built for command line use only and thus won't link GTK+.  
Use the --help option for further details.  

## Build  
Required are CMake 3.12.4, GCC or MinGW with C11 support and GTK+ 3.10.0.  
On Windows the program needs to be built from an MSYS2 MinGW console.  

CMake variables:  
- APPIMAGE : bool = 0  
  - package the program as an AppImage  
- CMAKE_BUILD_TYPE : string = Release  
  - can be set to "Debug"  
- CONSOLE : bool = 0  
  - build for command line use only  
- NATIVE : bool = 0  
  - build for the current CPU (only available for GCC and MinGW)  
