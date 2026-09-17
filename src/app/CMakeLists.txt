find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)
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
    vista_cascata.cpp vista_cascata.h
    grafico_polinomio.cpp grafico_polinomio.h
)
target_include_directories(hydroedit_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hydroedit_ui PUBLIC hidrcore Qt6::Widgets Qt6::Charts)

qt_add_executable(HydroEdit2 WIN32 main.cpp hydroedit.rc)
qt_add_resources(HydroEdit2 "icone"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../hidro.ico")
target_link_libraries(HydroEdit2 PRIVATE hydroedit_ui)
