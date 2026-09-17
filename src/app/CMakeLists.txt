find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_library(hydroedit_ui STATIC
    modelo_hidr.cpp modelo_hidr.h
    comandos.cpp comandos.h
    filtro_usinas.cpp filtro_usinas.h
    janela_principal.cpp janela_principal.h
    delegate_numerico.cpp delegate_numerico.h
    grade_vetor.cpp grade_vetor.h
    formulario_usina.cpp formulario_usina.h
    paginas_usina.cpp
    painel_problemas.cpp painel_problemas.h
)
target_include_directories(hydroedit_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hydroedit_ui PUBLIC hidrcore Qt6::Widgets)

qt_add_executable(HydroEdit WIN32 main.cpp)
target_link_libraries(HydroEdit PRIVATE hydroedit_ui)
