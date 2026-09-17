find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
qt_add_executable(HydroEdit WIN32 main.cpp)
target_link_libraries(HydroEdit PRIVATE hidrcore Qt6::Widgets)
