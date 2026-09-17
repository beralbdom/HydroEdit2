find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
qt_standard_project_setup()

function(adicionar_teste nome)
    qt_add_executable(${nome} ${nome}.cpp)
    target_link_libraries(${nome} PRIVATE hidrcore Qt6::Test Qt6::Widgets ${ARGN})
    target_compile_definitions(${nome} PRIVATE
        DIR_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        DIR_DECK="${CMAKE_SOURCE_DIR}/deck_newave_2026_09_rev1")
    add_test(NAME ${nome} COMMAND ${nome})
    set_tests_properties(${nome} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

adicionar_teste(test_campos)
