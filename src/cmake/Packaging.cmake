# Instalação do app = o conteúdo do pacote. O `cmake --install` monta a árvore
# que vira o .tar.gz (Linux), o .app do .dmg (macOS) e a pasta do .msi
# (Windows); os builders em Docker (packaging/<plataforma>/build.sh) só
# acrescentam o que o CMake não sabe fazer num cross-compile.
#
# Uso (src/app/CMakeLists.txt):
#
#   include(${CMAKE_SOURCE_DIR}/src/cmake/Packaging.cmake)
#   photoidx_install(photoidx)
#
# Layout instalado (o mesmo que core/Paths.cpp procura):
#
#   Linux    bin/PhotoIDX              lib/ (Qt, ONNX Runtime; rpath $ORIGIN/../lib)
#            share/photoidx/models/
#            share/applications/photoidx.desktop + ícone hicolor
#   Windows  PhotoIDX.exe e DLLs       models/
#   macOS    PhotoIDX.app/Contents/{MacOS,Frameworks,PlugIns}
#            PhotoIDX.app/Contents/Resources/models

# Qt (bibliotecas, plugins, QML) copiado pelo próprio Qt na instalação. Num
# cross-compile não dá (o macdeployqt/windeployqt não rodam no Linux): lá o
# packaging/<plataforma>/bundle.py faz esse papel.
if(CMAKE_CROSSCOMPILING)
    set(_photoidx_deploy_default OFF)
else()
    set(_photoidx_deploy_default ON)
endif()
option(PHOTOIDX_DEPLOY "Copiar Qt e dependências para a instalação (qt deploy)" ${_photoidx_deploy_default})

# Entradas preparadas pelo packaging/build.py antes do configure.
set(PHOTOIDX_ART_DIR "${CMAKE_SOURCE_DIR}/.cache/art" CACHE PATH
    "Ícones gerados do logo (packaging/build.py art): PhotoIDX.icns/.ico/.png")
set(PHOTOIDX_MODELS_DIR "${CMAKE_SOURCE_DIR}/.cache/models" CACHE PATH
    "Modelos de IA (packaging/build.py model): clip-vit-base-patch32/ e buffalo_sc/; vazio = pacote sem IA")

set(PHOTOIDX_BUNDLE_ID "br.com.photoidx.desktop") # nunca trocar (regra 18)

function(photoidx_install target)
    get_target_property(_name ${target} OUTPUT_NAME)
    if(NOT _name)
        set(_name ${target})
    endif()

    if(APPLE)
        set(_contents "${_name}.app/Contents")
        set(_res "${_contents}/Resources")
        set(_libdir "${_contents}/Frameworks")
        set(_doc "${_res}")
    elseif(WIN32)
        set(_res ".")
        set(_libdir ".")
        set(_doc ".")
    else()
        set(_res "${CMAKE_INSTALL_DATADIR}/photoidx")
        set(_libdir "${CMAKE_INSTALL_LIBDIR}")
        set(_doc "${CMAKE_INSTALL_DATADIR}/doc/photoidx")
    endif()

    # --- ícone embutido no executável -----------------------------------
    # (Se o alvo já traz um .rc com ícone, ele vale: dois IDI_ICON1 não ligam.)
    get_target_property(_sources ${target} SOURCES)
    list(FILTER _sources INCLUDE REGEX "\\.rc$")
    if(WIN32 AND NOT _sources AND EXISTS "${PHOTOIDX_ART_DIR}/PhotoIDX.ico")
        set(_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}_icon.rc")
        file(WRITE "${_rc}" "IDI_ICON1 ICON \"${PHOTOIDX_ART_DIR}/PhotoIDX.ico\"\n")
        target_sources(${target} PRIVATE "${_rc}")
    elseif(APPLE AND EXISTS "${PHOTOIDX_ART_DIR}/PhotoIDX.icns")
        set(_icns "${PHOTOIDX_ART_DIR}/PhotoIDX.icns")
        set_source_files_properties("${_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        target_sources(${target} PRIVATE "${_icns}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE PhotoIDX.icns)
    endif()

    set_target_properties(${target} PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER ${PHOTOIDX_BUNDLE_ID}
        MACOSX_BUNDLE_BUNDLE_NAME ${_name}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    if(APPLE)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
    elseif(UNIX)
        # Pacote relocável: as .so ficam em ../lib, em qualquer pasta.
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()

    if(WIN32)
        set(_bindir ".")
    else()
        set(_bindir "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(TARGETS ${target}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${_bindir})

    # --- ONNX Runtime (biblioteca compartilhada pronta) --------------------
    if(PHOTOIDX_WITH_ONNX AND PHOTOIDX_ORT_RUNTIME_FILES)
        foreach(_file IN LISTS PHOTOIDX_ORT_RUNTIME_FILES)
            get_filename_component(_fname "${_file}" NAME)
            if(UNIX AND NOT APPLE)
                # Só o SONAME (libonnxruntime.so.1), com o conteúdo do arquivo
                # real: os outros nomes são links que ninguém carrega. Com o
                # deploy do Qt ligado, ele mesmo já a copia (dependência do app).
                if(_fname STREQUAL "libonnxruntime.so.1" AND NOT PHOTOIDX_DEPLOY)
                    file(REAL_PATH "${_file}" _real)
                    install(FILES "${_real}" DESTINATION ${_libdir} RENAME ${_fname})
                endif()
            elseif(APPLE)
                # O install name é o arquivo versionado; os links não servem.
                if(NOT IS_SYMLINK "${_file}")
                    install(FILES "${_file}" DESTINATION ${_libdir})
                endif()
            else()
                install(FILES "${_file}" DESTINATION ${_libdir})
            endif()
        endforeach()
    endif()

    # --- modelos de IA: CLIP (categorias, busca) e buffalo_sc (faces) -----
    if(PHOTOIDX_MODELS_DIR)
        foreach(_model clip-vit-base-patch32 buffalo_sc)
            if(EXISTS "${PHOTOIDX_MODELS_DIR}/${_model}")
                install(DIRECTORY "${PHOTOIDX_MODELS_DIR}/${_model}/" DESTINATION "${_res}/models/${_model}"
                        FILES_MATCHING PATTERN "*.onnx" PATTERN "vocab.json" PATTERN "merges.txt")
            else()
                message(WARNING "Modelo ${_model} ausente em ${PHOTOIDX_MODELS_DIR}: o pacote sai sem "
                                "essa IA (rode `packaging/build.py model`).")
            endif()
        endforeach()
    endif()

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION "${_doc}")

    # --- integração com o desktop do Linux --------------------------------
    if(UNIX AND NOT APPLE)
        install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/photoidx.desktop"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
        # .photoidxe (foto privada) -> PhotoIDX; o install-desktop.sh na raiz
        # do pacote registra no menu do usuário (o .tar.gz não tem instalador).
        install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/photoidx-mime.xml"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages" RENAME photoidx.xml)
        install(PROGRAMS "${CMAKE_SOURCE_DIR}/packaging/linux/install-desktop.sh" DESTINATION ".")
        if(EXISTS "${PHOTOIDX_ART_DIR}/PhotoIDX.png")
            install(FILES "${PHOTOIDX_ART_DIR}/PhotoIDX.png"
                    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps" RENAME photoidx.png)
        endif()
    endif()

    # --- Qt: bibliotecas, plugins e módulos QML ----------------------------
    # Linux: lib/, plugins/, qml/ e bin/qt.conf (deploy genérico do Qt 6.5+,
    # sem as bibliotecas do sistema). Windows: windeployqt. macOS: macdeployqt.
    if(PHOTOIDX_DEPLOY)
        # O script do Qt procura o executável em <prefixo>/${CMAKE_INSTALL_BINDIR}:
        # no Windows ele fica na raiz da instalação (ao lado das DLLs).
        if(WIN32)
            set(CMAKE_INSTALL_BINDIR ".")
        endif()
        qt_generate_deploy_qml_app_script(
            TARGET ${target}
            OUTPUT_SCRIPT _deploy
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
        install(SCRIPT "${_deploy}")
    endif()
endfunction()
