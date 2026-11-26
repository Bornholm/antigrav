# Compilation croisée de Linux vers Windows 64 bits avec MinGW-w64 :
#   cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
# La variante « posix » de MinGW fournit std::thread et std::mutex.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_ASM_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Les tests se lancent avec Wine quand il est installé.
find_program(WINE NAMES wine64 wine HINTS /usr/lib/wine)
if(WINE)
  set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE})
endif()
