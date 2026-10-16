# Testes (QtTest): um executável por área. As faces e o picture_hash comparam
# com referências geradas pelo insightface/Pillow (tests/data/faces/gen_faces.py).
function(photoidx_test name)
    add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE ${ARGN} Qt6::Test)
    target_compile_definitions(tst_${name} PRIVATE PHOTOIDX_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data"
                                                   PHOTOIDX_MODELS_DIR="${CMAKE_SOURCE_DIR}/.cache/models")
    add_test(NAME ${name} COMMAND tst_${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

photoidx_test(imaging photoidx_imaging)
photoidx_test(ai photoidx_imaging)
photoidx_test(core photoidx_core)
