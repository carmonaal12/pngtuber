#pragma once

// Consulta si una tecla física está pulsada ahora mismo, sin depender de que la
// aplicación tenga el foco. Se usa para detectar la SOLTADA de un atajo en modo
// "mantener pulsado": las tres APIs de atajos globales (RegisterHotKey, XGrabKey,
// RegisterEventHotKey) notifican la pulsación de forma fiable, pero la soltada no
// es homogénea entre sistemas, así que se sondea a 30 ms.
namespace KeyState {

// qtKey es un valor de Qt::Key (la tecla principal del atajo, sin modificadores).
bool isPressed(int qtKey);

} // namespace KeyState
