add_library(hidrcore STATIC
    usina_hidr.cpp
)
target_include_directories(hidrcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
