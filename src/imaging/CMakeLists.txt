# Processamento de imagem e IA: leitura de JPEG/PNG/RAW (OpenCV, Qt, LibRaw),
# EXIF completo, hashes, CLIP (categorias e busca) e faces (insightface). Sem
# banco e sem interface — só OpenCV, LibRaw, ONNX Runtime e o QtCore/QtGui.
add_library(photoidx_imaging STATIC
    Formats.h Formats.cpp
    Exif.h Exif.cpp
    ImageIO.h ImageIO.cpp
    Hashes.h Hashes.cpp
    Raw.h Raw.cpp
    Memory.h Memory.cpp
    Inspect.h Inspect.cpp
    Embedding.h Embedding.cpp
    Onnx.h Onnx.cpp
    Clip.h Clip.cpp
    Faces.h Faces.cpp
    Collage.h Collage.cpp
)
target_include_directories(photoidx_imaging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(photoidx_imaging PUBLIC Qt6::Core Qt6::Gui ${OpenCV_LIBS} photoidx_libraw)
if(PHOTOIDX_WITH_ONNX)
    target_link_libraries(photoidx_imaging PUBLIC photoidx_onnxruntime)
    target_compile_definitions(photoidx_imaging PUBLIC PHOTOIDX_HAVE_ONNX)
endif()
