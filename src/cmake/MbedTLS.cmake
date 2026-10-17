# Mbed TLS (Apache-2.0): só a parte de criptografia (mbedcrypto) — AES-256-GCM
# e PBKDF2-HMAC-SHA512 das fotos privadas. Compilado ESTÁTICO a partir do
# fonte oficial, junto com o app, nos seis alvos (o Qt não tem AES e o
# OpenSSL não existe igual em todos). Versão e sha256 fixos (regra 16); o
# tarball da release já traz os arquivos gerados (sem Python no build).
#
# PHOTOIDX_MBEDTLS_SOURCE aponta para um fonte já extraído (build offline);
# vazio, o FetchContent baixa a versão abaixo.
set(MBEDTLS_VERSION 3.6.4)
set(MBEDTLS_SHA256 ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GEN_FILES OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)

if(PHOTOIDX_MBEDTLS_SOURCE)
    add_subdirectory(${PHOTOIDX_MBEDTLS_SOURCE} ${CMAKE_BINARY_DIR}/_deps/mbedtls-build EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(mbedtls_src
        URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${MBEDTLS_VERSION}/mbedtls-${MBEDTLS_VERSION}.tar.bz2
        URL_HASH SHA256=${MBEDTLS_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(mbedtls_src)
endif()

set_target_properties(mbedcrypto PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Código de terceiros: os avisos dele não são nossos.
    target_compile_options(mbedcrypto PRIVATE -w)
elseif(MSVC)
    target_compile_options(mbedcrypto PRIVATE /W0)
endif()
