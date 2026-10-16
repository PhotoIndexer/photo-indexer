# ONNX Runtime (MIT): pacote oficial pré-compilado da plataforma, como no
# clivcut. PHOTOIDX_ONNXRUNTIME_ROOT aponta para um já extraído (build
# offline); vazio, o FetchContent baixa a versão fixa abaixo.
set(ORT_VERSION 1.30.0)

# Arquitetura alvo: no macOS vem de CMAKE_OSX_ARCHITECTURES, se definida.
set(PHOTOIDX_TARGET_ARCH ${CMAKE_SYSTEM_PROCESSOR})
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(GET CMAKE_OSX_ARCHITECTURES 0 PHOTOIDX_TARGET_ARCH)
endif()
if(WIN32 AND CMAKE_GENERATOR_PLATFORM)
    set(PHOTOIDX_TARGET_ARCH ${CMAKE_GENERATOR_PLATFORM})
endif()

if(PHOTOIDX_ONNXRUNTIME_ROOT)
    set(ORT_ROOT ${PHOTOIDX_ONNXRUNTIME_ROOT})
else()
    if(WIN32 AND PHOTOIDX_TARGET_ARCH MATCHES "ARM64|arm64|aarch64")
        set(ORT_PKG onnxruntime-win-arm64-${ORT_VERSION}.zip)
    elseif(WIN32)
        set(ORT_PKG onnxruntime-win-x64-${ORT_VERSION}.zip)
    elseif(APPLE AND PHOTOIDX_TARGET_ARCH MATCHES "arm64|aarch64")
        set(ORT_PKG onnxruntime-osx-arm64-${ORT_VERSION}.tgz)
    elseif(APPLE)
        # A Microsoft parou de publicar o pacote para Mac Intel depois da 1.23.2.
        set(ORT_VERSION 1.23.2)
        set(ORT_PKG onnxruntime-osx-x86_64-${ORT_VERSION}.tgz)
    elseif(PHOTOIDX_TARGET_ARCH MATCHES "aarch64|arm64")
        set(ORT_PKG onnxruntime-linux-aarch64-${ORT_VERSION}.tgz)
    else()
        set(ORT_PKG onnxruntime-linux-x64-${ORT_VERSION}.tgz)
    endif()
    include(FetchContent)
    FetchContent_Declare(onnxruntime_pkg
        URL https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${ORT_PKG}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(onnxruntime_pkg)
    set(ORT_ROOT ${onnxruntime_pkg_SOURCE_DIR})
endif()

add_library(photoidx_onnxruntime SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(photoidx_onnxruntime PROPERTIES
        IMPORTED_LOCATION ${ORT_ROOT}/lib/onnxruntime.dll
        IMPORTED_IMPLIB ${ORT_ROOT}/lib/onnxruntime.lib)
    set(ORT_RUNTIME_FILES ${ORT_ROOT}/lib/onnxruntime.dll)
    if(EXISTS ${ORT_ROOT}/lib/onnxruntime_providers_shared.dll)
        list(APPEND ORT_RUNTIME_FILES ${ORT_ROOT}/lib/onnxruntime_providers_shared.dll)
    endif()
elseif(APPLE)
    file(GLOB ORT_RUNTIME_FILES ${ORT_ROOT}/lib/libonnxruntime*.dylib)
    set_target_properties(photoidx_onnxruntime PROPERTIES IMPORTED_LOCATION ${ORT_ROOT}/lib/libonnxruntime.dylib)
else()
    file(GLOB ORT_RUNTIME_FILES ${ORT_ROOT}/lib/libonnxruntime.so*)
    set_target_properties(photoidx_onnxruntime PROPERTIES
        IMPORTED_LOCATION ${ORT_ROOT}/lib/libonnxruntime.so.1
        IMPORTED_SONAME libonnxruntime.so.1)
endif()
set_target_properties(photoidx_onnxruntime PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${ORT_ROOT}/include)
set(PHOTOIDX_ORT_RUNTIME_FILES ${ORT_RUNTIME_FILES} CACHE INTERNAL "")
