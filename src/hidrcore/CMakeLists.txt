add_library(hidrcore STATIC
    usina_hidr.cpp
    campos.cpp
    registro.cpp
    texto.cpp
    arquivo_hidr.cpp
    exportador_csv.cpp
    validacao.cpp
)
target_include_directories(hidrcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
