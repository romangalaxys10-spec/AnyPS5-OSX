add_executable(anyps5-launcher
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Launcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(anyps5-launcher PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(anyps5-launcher PRIVATE cxx_std_20)

install(TARGETS anyps5-launcher RUNTIME DESTINATION bin)
