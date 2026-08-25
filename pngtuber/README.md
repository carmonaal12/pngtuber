# PNGTuber Desktop

Overlay de PNGTuber para el escritorio, en C++17 + Qt 6. Sin Electron, sin navegador
embebido: un único ejecutable nativo que pinta un GIF con transparencia real por
encima de la barra de tareas y cambia de variación con atajos de teclado globales.

Consumo típico: ~35-60 MB de RAM y prácticamente 0 % de CPU en reposo.

## Qué hace

- Ventana de configuración al arrancar; luego vive en la bandeja del sistema.
- GIF animado con canal alfa, siempre encima, sin bordes ni sombra.
- Escala del 10 % al 400 % y opacidad regulable.
- Anclaje relativo a la barra de tareas: al principio, al centro o al final, y
  apoyado encima de la barra, centrado dentro de ella o pegado al borde de la
  pantalla. Más un ajuste fino en píxeles.
- Variaciones ilimitadas, cada una con su GIF, su atajo global y su modo:
  - **Conmutar**: se pulsa y la variación se queda fija hasta pulsarla de nuevo.
  - **Mantener**: se muestra sólo mientras la tecla está pulsada.
- Multimonitor: cada pantalla se activa o desactiva por separado y se le asigna
  un perfil. Mismo perfil en todas = el mismo PNGTuber repetido; perfiles
  distintos = un personaje diferente en cada monitor.
- Paso de clics (*click-through*) activable y desactivable con un atajo. Con los
  clics desactivados puedes arrastrar el GIF con el ratón y la posición se guarda.
- Todo se guarda en JSON y se aplica en caliente, sin reiniciar.

## Conseguir el .exe de Windows sin instalar nada

El repositorio incluye un flujo de GitHub Actions que compila el programa en una
máquina Windows real y genera dos cosas: un instalador (`PngtuberDesktop-Setup.exe`)
y una carpeta portable comprimida.

1. Crea un repositorio en GitHub y sube este proyecto entero, incluida la carpeta
   oculta `.github`.
2. Entra en la pestaña **Actions**, elige *Compilar para Windows* y pulsa
   **Run workflow**. Tarda unos 5-8 minutos.
3. Cuando termine, abre la ejecución y descarga el artefacto
   **PngtuberDesktop-windows** de la sección *Artifacts*.

El instalador no pide permisos de administrador: instala en la carpeta del usuario,
crea el acceso directo en el menú Inicio y ofrece la casilla de arranque automático
con Windows. La versión portable es una carpeta que se descomprime y se ejecuta.

Si etiquetas una versión (`git tag v1.0 && git push --tags`), el flujo publica
además una Release con los dos archivos listos para descargar o compartir.

La versión de Qt está fijada al principio de
`.github/workflows/build-windows.yml`; cámbiala ahí si quieres otra.

## Compilación manual

Requiere CMake 3.16+, un compilador con C++17 y Qt 6 (probado con 6.4).

### Linux

```bash
sudo apt install build-essential cmake qt6-base-dev libx11-dev   # Debian/Ubuntu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/pngtuber
```

### Windows

Instala Qt 6 con el instalador oficial (componente MSVC 2022 64-bit) y:

```powershell
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2022_64"
cmake --build build --config Release
windeployqt build\Release\pngtuber.exe    # copia las DLL de Qt junto al .exe
```

Se compila con `WIN32_EXECUTABLE`, así que no abre ventana de consola.

### macOS

```bash
brew install qt cmake
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
macdeployqt build/pngtuber.app
```

### Prueba funcional

```bash
cmake -B build -DBUILD_TESTS=ON && cmake --build build --target geometry_test
QT_QPA_PLATFORM=offscreen ./build/geometry_test
```

Verifica el escalado, el anclaje, el cambio de variación y el click-through.

## Uso

1. Abre la pestaña **Perfiles** y elige el GIF base (el de reposo).
2. Ajusta escala y posición. El overlay se actualiza mientras mueves el deslizador.
3. En **Variaciones y atajos**, añade una entrada por cada GIF alternativo: nombre,
   archivo, atajo y modo. El campo de atajo captura la combinación que pulses.
4. En **Monitores**, marca en qué pantallas quieres el overlay y con qué perfil.
5. Cierra la ventana: el programa sigue en la bandeja.

Atajos globales por defecto: `Ctrl+Alt+C` alterna el paso de clics y `Ctrl+Alt+H`
muestra u oculta el overlay.

Para recolocar el GIF a mano: pulsa `Ctrl+Alt+C` para que deje de ignorar el ratón,
arrástralo, y vuelve a pulsarlo. Doble clic sobre el GIF abre la configuración.

## Limitaciones conocidas

Son limitaciones de los sistemas operativos, no del programa.

**Linux — sólo sesiones Xorg.** Los atajos globales usan `XGrabKey` y el
posicionamiento absoluto usa coordenadas de pantalla; Wayland no permite ninguna
de las dos cosas a una aplicación normal. En Wayland el overlay aparecerá donde
decida el compositor y los atajos no se registrarán. Si tu distribución arranca
en Wayland por defecto, elige "Xorg" en la pantalla de inicio de sesión. Dar
soporte a Wayland exigiría `wlr-layer-shell` (sólo compositores wlroots: Sway,
Hyprland) para la posición y el portal `GlobalShortcuts` para los atajos.

**macOS — permisos y ausencia de barra de tareas.** macOS no tiene barra de
tareas: la detección se hace contra el Dock y la barra de menús vía
`availableGeometry`, así que "la barra" es el Dock. Además, la primera vez habrá
que conceder permisos en Ajustes → Privacidad y seguridad → Accesibilidad y
Monitorización de entrada para que funcionen los atajos globales. Ten en cuenta
que Qt intercambia Ctrl y Cmd en macOS: lo que grabes como `Ctrl+Alt+1` se
registra como Cmd+Option+1.

**Windows.** Sin limitaciones relevantes. Si `Win+D` (mostrar escritorio) deja el
overlay detrás, `Ctrl+Alt+H` dos veces lo devuelve al frente.

**Atajos ya ocupados.** Si otro programa tiene registrada la misma combinación, el
sistema rechaza el registro. La ventana de configuración lo avisa abajo, en naranja,
indicando qué atajos han fallado.

## Estructura

| Archivo | Responsabilidad |
|---|---|
| `config.h/.cpp` | Modelo de datos y persistencia en JSON |
| `overlaywindow.h/.cpp` | Ventana translúcida, reproducción del GIF, cálculo de anclaje |
| `overlaycontroller.h/.cpp` | Un overlay por monitor; traduce atajos en cambios de variación |
| `hotkeymanager.h/.cpp` | Atajos globales: `RegisterHotKey` / `XGrabKey` / `RegisterEventHotKey` |
| `keystate.h/.cpp` | Sondeo del estado físico de una tecla, para el modo "mantener" |
| `configwindow.h/.cpp` | Interfaz de configuración |
| `main.cpp` | Arranque, bandeja del sistema y cableado |

Sin dependencias externas más allá de Qt y, en Linux, libX11.

## Ideas para más adelante

- Reactividad al micrófono (boca abierta al hablar) con `QAudioSource`.
- Perfiles importables/exportables en un `.zip` con los GIFs incluidos.
- Modo *chroma* con fondo verde para capturarlo desde OBS como fuente de ventana.
