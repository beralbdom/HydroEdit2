add_library(hidrcore STATIC
    usina_hidr.cpp
    campos.cpp
    registro.cpp
    texto.cpp
    deck_lookup.cpp
)
target_include_directories(hidrcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
