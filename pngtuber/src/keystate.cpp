#include "keystate.h"

#include <QtGlobal>
#include <Qt>

#if defined(Q_OS_WIN)

#include <windows.h>

static int qtKeyToNative(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)   return 'A' + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)   return '0' + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return VK_F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space:  return VK_SPACE;
    case Qt::Key_Tab:    return VK_TAB;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Left:   return VK_LEFT;
    case Qt::Key_Right:  return VK_RIGHT;
    case Qt::Key_Up:     return VK_UP;
    case Qt::Key_Down:   return VK_DOWN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home:   return VK_HOME;
    case Qt::Key_End:    return VK_END;
    default: return 0;
    }
}

bool KeyState::isPressed(int qtKey)
{
    const int vk = qtKeyToNative(qtKey);
    if (vk == 0)
        return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

#elif defined(Q_OS_MACOS)

#include <ApplicationServices/ApplicationServices.h>

// Los códigos de tecla de macOS no son contiguos: hace falta tabla.
static int qtKeyToNative(int key)
{
    static const int letters[26] = {
        0, 11, 8, 2, 14, 3, 5, 4, 34, 38, 40, 37, 46,
        45, 31, 35, 12, 15, 1, 17, 32, 9, 13, 7, 16, 6
    };
    static const int digits[10] = { 29, 18, 19, 20, 21, 23, 22, 26, 28, 25 };
    static const int fkeys[12]  = { 122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111 };

    if (key >= Qt::Key_A && key <= Qt::Key_Z)    return letters[key - Qt::Key_A];
    if (key >= Qt::Key_0 && key <= Qt::Key_9)    return digits[key - Qt::Key_0];
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) return fkeys[key - Qt::Key_F1];
    switch (key) {
    case Qt::Key_Space:  return 49;
    case Qt::Key_Tab:    return 48;
    case Qt::Key_Escape: return 53;
    case Qt::Key_Left:   return 123;
    case Qt::Key_Right:  return 124;
    case Qt::Key_Down:   return 125;
    case Qt::Key_Up:     return 126;
    default: return -1;
    }
}

bool KeyState::isPressed(int qtKey)
{
    const int code = qtKeyToNative(qtKey);
    if (code < 0)
        return false;
    return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, (CGKeyCode)code);
}

#else // X11

#include <X11/Xlib.h>
#include <X11/keysym.h>

static Display *stateDisplay()
{
    static Display *d = XOpenDisplay(nullptr);
    return d;
}

static KeySym qtKeyToKeySym(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)    return XK_a + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)    return XK_0 + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) return XK_F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space:  return XK_space;
    case Qt::Key_Tab:    return XK_Tab;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Left:   return XK_Left;
    case Qt::Key_Right:  return XK_Right;
    case Qt::Key_Up:     return XK_Up;
    case Qt::Key_Down:   return XK_Down;
    default: return NoSymbol;
    }
}

bool KeyState::isPressed(int qtKey)
{
    Display *d = stateDisplay();
    if (!d)
        return false;

    const KeySym sym = qtKeyToKeySym(qtKey);
    if (sym == NoSymbol)
        return false;

    const KeyCode code = XKeysymToKeycode(d, sym);
    if (code == 0)
        return false;

    char keys[32];
    XQueryKeymap(d, keys);
    return (keys[code / 8] & (1 << (code % 8))) != 0;
}

#endif
