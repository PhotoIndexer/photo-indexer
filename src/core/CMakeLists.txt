# Núcleo: o catálogo SQLite (fotos, EXIF, categorias, álbuns, pessoas, faces,
# discos), as etapas da indexação (Pipeline), a identificação de discos e a
# geolocalização reversa. Sem interface — o app (QML) e os testes usam esta
# biblioteca.
add_library(photoidx_core STATIC
    Paths.h Paths.cpp
    Database.h Database.cpp
    Disks.h Disks.cpp
    Places.h Places.cpp
    HashTree.h HashTree.cpp
    FaceIndex.h FaceIndex.cpp
    Taxonomy.h Taxonomy.cpp
    Models.h
    Crypto.h Crypto.cpp
    Vault.h Vault.cpp
    Privacy.h Privacy.cpp
    Catalog.h Catalog.cpp CatalogAi.cpp CatalogPeople.cpp CatalogQuery.cpp CatalogOrganize.cpp CatalogFilter.cpp
    CatalogPrivate.cpp
    Pipeline.h Pipeline.cpp
)
target_include_directories(photoidx_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(photoidx_core PUBLIC photoidx_imaging Qt6::Core Qt6::Sql Qt6::Concurrent PRIVATE mbedcrypto)
if(APPLE)
    # UUID e tipo do volume (Disks.cpp).
    target_link_libraries(photoidx_core PRIVATE "-framework DiskArbitration" "-framework CoreFoundation")
elseif(WIN32)
    # WNetGetConnection: o \\servidor\compartilhamento de uma unidade mapeada.
    target_link_libraries(photoidx_core PRIVATE mpr)
endif()

# Passos do esquema (:/photoidx/schema/NNNN.sql) e a tabela de lugares do
# GeoNames (:/photoidx/data/places.qz), embutidos no binário.
file(GLOB PHOTOIDX_SCHEMA RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS schema/*.sql)
qt_add_resources(photoidx_core "photoidx_core_data"
    PREFIX "/photoidx"
    FILES ${PHOTOIDX_SCHEMA} data/places.qz)
