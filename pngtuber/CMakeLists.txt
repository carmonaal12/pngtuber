cmake_minimum_required(VERSION 3.16)

project(PngtuberDesktop VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# --- Icono ---
# icon.png viaja dentro del ejecutable (bandeja y ventanas) vía Qt resources.
# icon.ico se incrusta como recurso nativo de Windows para que el Explorador,
# la barra de tareas y el instalador muestren el icono correcto.
set(PNGTUBER_RESOURCES resources/icon.qrc)
if(WIN32)
    list(APPEND PNGTUBER_RESOURCES resources/app.rc)
endif()

add_executable(pngtuber
    ${PNGTUBER_RESOURCES}
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

# --- Pruebas funcionales opcionales: cmake -DBUILD_TESTS=ON ---
option(BUILD_TESTS "Compilar las pruebas funcionales" OFF)
if(BUILD_TESTS)
    add_executable(geometry_test
        test/geometry_test.cpp
        src/config.cpp
        src/overlaywindow.cpp
    )
    target_link_libraries(geometry_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

    add_executable(ui_test
        test/ui_test.cpp
        src/config.cpp
        src/keystate.cpp
        src/hotkeymanager.cpp
        src/overlaywindow.cpp
        src/overlaycontroller.cpp
        src/configwindow.cpp
    )
    target_link_libraries(ui_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    if(WIN32)
        target_link_libraries(ui_test PRIVATE user32)
    elseif(APPLE)
        target_link_libraries(ui_test PRIVATE
            "-framework Carbon"
            "-framework ApplicationServices"
        )
    else()
        target_link_libraries(ui_test PRIVATE X11::X11)
    endif()
endif()
