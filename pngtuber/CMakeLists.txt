cmake_minimum_required(VERSION 3.16)

project(PngtuberDesktop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(pngtuber
    src/main.cpp
    src/config.cpp
    src/config.h
    src/keystate.cpp
    src/keystate.h
    src/hotkeymanager.cpp
    src/hotkeymanager.h
    src/overlaywindow.cpp
    src/overlaywindow.h
    src/overlaycontroller.cpp
    src/overlaycontroller.h
    src/configwindow.cpp
    src/configwindow.h
)

target_link_libraries(pngtuber PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# --- Sin consola en Windows, bundle .app en macOS ---
set_target_properties(pngtuber PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "PNGTuber Desktop"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.pngtuber.desktop"
)

# --- Dependencias nativas de cada plataforma para los atajos globales ---
if(WIN32)
    target_link_libraries(pngtuber PRIVATE user32)
elseif(APPLE)
    target_link_libraries(pngtuber PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
    )
else()
    find_package(X11 REQUIRED)
    target_link_libraries(pngtuber PRIVATE X11::X11)
endif()

install(TARGETS pngtuber
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

# --- Prueba funcional opcional: cmake -DBUILD_TESTS=ON ---
option(BUILD_TESTS "Compilar la prueba funcional de geometría" OFF)
if(BUILD_TESTS)
    add_executable(geometry_test
        test/geometry_test.cpp
        src/config.cpp
        src/overlaywindow.cpp
    )
    target_link_libraries(geometry_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
endif()
