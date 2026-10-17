# Interface gráfica: Qt Quick (QML), como o photo-editor. Os controllers C++
# (AppController = singleton App no QML) expõem o núcleo; o QML só desenha.

# Ícones de plataforma (.ico no exe, .icns no bundle) vêm do photoidx_install
# (src/cmake/Packaging.cmake), desenhados a partir do logo.

qt_add_executable(photoidx
    main.cpp
    AppController.h AppController.cpp
    LibraryController.h LibraryController.cpp
    ViewerController.h ViewerController.cpp
    PeopleController.h PeopleController.cpp
    TasksController.h TasksController.cpp
    PrivacyController.h PrivacyController.cpp
    FilesController.h FilesController.cpp
    SingleInstance.h SingleInstance.cpp
    PhotoGridModel.h PhotoGridModel.cpp
    PeopleListModel.h PeopleListModel.cpp
    TreeModel.h TreeModel.cpp
    ImageProviders.h ImageProviders.cpp
    Language.h Language.cpp
    Logging.h Logging.cpp
)
set_target_properties(photoidx PROPERTIES OUTPUT_NAME PhotoIDX)

file(GLOB PHOTOIDX_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    qml/*.qml qml/components/*.qml qml/pages/*.qml qml/dialogs/*.qml)
file(GLOB PHOTOIDX_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg)
file(GLOB PHOTOIDX_ASSETS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    assets/*.png assets/logo/*.png fonts/*.ttf)
set_source_files_properties(qml/Theme.qml qml/Dialogs.qml qml/Messages.qml qml/Keymap.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Pastas qml/components, qml/pages... com qmldir próprio.
qt_policy(SET QTP0004 NEW)
qt_add_qml_module(photoidx
    URI PhotoIDX
    VERSION 1.0
    # O executável também se chama PhotoIDX: o módulo vai para outra pasta.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/PhotoIDX
    QML_FILES ${PHOTOIDX_QML}
    RESOURCES ${PHOTOIDX_ICONS} ${PHOTOIDX_ASSETS}
)

target_compile_definitions(photoidx PRIVATE
    PHOTOIDX_VERSION="${PHOTOIDX_VERSION_LABEL}"
    PHOTOIDX_BRAND_NAME="${PHOTOIDX_BRAND_NAME}"
    PHOTOIDX_BRAND_URL="${PHOTOIDX_BRAND_URL}"
    PHOTOIDX_REPO_URL="${PHOTOIDX_REPO_URL}")
target_link_libraries(photoidx PRIVATE
    photoidx_core
    Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Concurrent Qt6::Network)

# Traduções: o código está em inglês; photoidx_en.ts guarda só os plurais.
# Atualize os .ts com: make translations
qt_add_translations(TARGETS photoidx
    SOURCE_TARGETS photoidx photoidx_core photoidx_imaging
    TS_FILES ${CMAKE_SOURCE_DIR}/i18n/photoidx_pt_BR.ts
    PLURALS_TS_FILE ${CMAKE_SOURCE_DIR}/i18n/photoidx_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none)

include(${CMAKE_SOURCE_DIR}/src/cmake/Packaging.cmake)
photoidx_install(photoidx)
