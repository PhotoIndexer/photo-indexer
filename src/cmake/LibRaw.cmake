# LibRaw (LGPL-2.1 / CDDL-1.0): leitura dos RAW (Canon, Nikon, Sony, Fuji,
# Olympus, Panasonic, Pentax, DNG...) — metadados, EXIF completo pelo callback e
# a prévia JPEG embutida. Compilado ESTÁTICO a partir do fonte oficial, junto
# com o app, em todos os alvos: nada a instalar nos builders e o mesmo código
# nos seis pacotes. Versão e sha256 fixos (regra 16).
#
# PHOTOIDX_LIBRAW_SOURCE aponta para um fonte já extraído (build offline);
# vazio, o FetchContent baixa a versão abaixo.
set(LIBRAW_VERSION 0.21.4)
set(LIBRAW_SHA256 6be43f19397e43214ff56aab056bf3ff4925ca14012ce5a1538a172406a09e63)

if(PHOTOIDX_LIBRAW_SOURCE)
    set(LIBRAW_ROOT ${PHOTOIDX_LIBRAW_SOURCE})
else()
    include(FetchContent)
    FetchContent_Declare(libraw_src
        URL https://www.libraw.org/data/LibRaw-${LIBRAW_VERSION}.tar.gz
        URL_HASH SHA256=${LIBRAW_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(libraw_src)
    set(LIBRAW_ROOT ${libraw_src_SOURCE_DIR})
endif()

# Os *_ph.cpp são "placeholders" incluídos por outros fontes; o resto é a
# lista do Makefile.dist (LIB_MT_OBJECTS).
file(GLOB_RECURSE _libraw_sources ${LIBRAW_ROOT}/src/*.cpp)
list(FILTER _libraw_sources EXCLUDE REGEX "_ph\\.cpp$")

add_library(photoidx_libraw STATIC ${_libraw_sources})
# Os includes do LibRaw são "libraw/libraw.h" (a partir da raiz) e os
# internos, "../../internal/..." relativos ao fonte.
target_include_directories(photoidx_libraw SYSTEM PUBLIC ${LIBRAW_ROOT})
# Versão thread-safe (o libraw_r): cada worker usa o seu LibRaw. Sem zlib e sem
# libjpeg — o app só usa metadados e a prévia embutida, nunca a revelação de
# DNG compactado com deflate/JPEG com perdas.
target_compile_definitions(photoidx_libraw PUBLIC LIBRAW_NODLL PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(photoidx_libraw PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Código de terceiros: os avisos dele não são nossos.
    target_compile_options(photoidx_libraw PRIVATE -w)
elseif(MSVC)
    target_compile_options(photoidx_libraw PRIVATE /W0)
endif()
if(WIN32)
    # htonl/ntohs do datastream.
    target_link_libraries(photoidx_libraw PUBLIC ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(photoidx_libraw PUBLIC Threads::Threads m)
endif()
