# Toolchain do CMake para Windows x64 com o llvm-mingw (packaging/windows/Dockerfile).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(_triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${_triple}-clang)
set(CMAKE_CXX_COMPILER ${_triple}-clang++)
set(CMAKE_RC_COMPILER ${_triple}-windres)
set(CMAKE_AR /opt/llvm-mingw/bin/llvm-ar CACHE FILEPATH "")
set(CMAKE_RANLIB /opt/llvm-mingw/bin/llvm-ranlib CACHE FILEPATH "")
# Bibliotecas e headers só do llvm-mingw, das dependências e do Qt de
# Windows; pacotes CMake também do host (as ferramentas do Qt de Linux, via
# QT_HOST_PATH).
set(CMAKE_FIND_ROOT_PATH /opt/llvm-mingw/${_triple} /opt/deps/x86_64 /opt/qt/$ENV{QT_VERSION}/llvm-mingw_64)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
