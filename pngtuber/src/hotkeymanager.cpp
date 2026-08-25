#include "hotkeymanager.h"
#include "keystate.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QSocketNotifier>
#include <QTimer>

// ---------------------------------------------------------------------------
// Traducción de Qt::Key / Qt::KeyboardModifiers a los códigos de cada sistema
// ---------------------------------------------------------------------------

#if defined(Q_OS_WIN)

#include <windows.h>

static bool translateKey(int qtKey, quint32 *nativeKey)
{
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)    { *nativeKey = 'A' + (qtKey - Qt::Key_A); return true; }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)    { *nativeKey = '0' + (qtKey - Qt::Key_0); return true; }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) { *nativeKey = VK_F1 + (qtKey - Qt::Key_F1); return true; }
    switch (qtKey) {
    case Qt::Key_Space:  *nativeKey = VK_SPACE;  return true;
    case Qt::Key_Tab:    *nativeKey = VK_TAB;    return true;
    case Qt::Key_Escape: *nativeKey = VK_ESCAPE; return true;
    case Qt::Key_Left:   *nativeKey = VK_LEFT;   return true;
    case Qt::Key_Right:  *nativeKey = VK_RIGHT;  return true;
    case Qt::Key_Up:     *nativeKey = VK_UP;     return true;
    case Qt::Key_Down:   *nativeKey = VK_DOWN;   return true;
    case Qt::Key_Insert: *nativeKey = VK_INSERT; return true;
    case Qt::Key_Delete: *nativeKey = VK_DELETE; return true;
    case Qt::Key_Home:   *nativeKey = VK_HOME;   return true;
    case Qt::Key_End:    *nativeKey = VK_END;    return true;
    default: return false;
    }
}

static quint32 translateMods(Qt::KeyboardModifiers m)
{
    quint32 r = 0;
    if (m & Qt::ShiftModifier)   r |= MOD_SHIFT;
    if (m & Qt::ControlModifier) r |= MOD_CONTROL;
    if (m & Qt::AltModifier)     r |= MOD_ALT;
    if (m & Qt::MetaModifier)    r |= MOD_WIN;
    return r;
}

#elif defined(Q_OS_MACOS)

#include <Carbon/Carbon.h>

static bool translateKey(int qtKey, quint32 *nativeKey)
{
    static const int letters[26] = {
        0, 11, 8, 2, 14, 3, 5, 4, 34, 38, 40, 37, 46,
        45, 31, 35, 12, 15, 1, 17, 32, 9, 13, 7, 16, 6
    };
    static const int digits[10] = { 29, 18, 19, 20, 21, 23, 22, 26, 28, 25 };
    static const int fkeys[12]  = { 122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111 };

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)    { *nativeKey = letters[qtKey - Qt::Key_A]; return true; }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)    { *nativeKey = digits[qtKey - Qt::Key_0];  return true; }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12) { *nativeKey = fkeys[qtKey - Qt::Key_F1];  return true; }
    switch (qtKey) {
    case Qt::Key_Space:  *nativeKey = 49;  return true;
    case Qt::Key_Tab:    *nativeKey = 48;  return true;
    case Qt::Key_Escape: *nativeKey = 53;  return true;
    case Qt::Key_Left:   *nativeKey = 123; return true;
    case Qt::Key_Right:  *nativeKey = 124; return true;
    case Qt::Key_Down:   *nativeKey = 125; return true;
    case Qt::Key_Up:     *nativeKey = 126; return true;
    default: return false;
    }
}

static quint32 translateMods(Qt::KeyboardModifiers m)
{
    quint32 r = 0;
    if (m & Qt::ShiftModifier)   r |= shiftKey;
    if (m & Qt::ControlModifier) r |= cmdKey;     // Qt mapea Ctrl a Cmd en macOS
    if (m & Qt::AltModifier)     r |= optionKey;
    if (m & Qt::MetaModifier)    r |= controlKey; // y Meta al Control físico
    return r;
}

static HotkeyManager *g_macManager = nullptr;
static QHash<quint32, int> g_macIdToIndex;

static OSStatus macHotkeyHandler(EventHandlerCallRef, EventRef event, void *userData);

#else // X11

#include <X11/Xlib.h>
#include <X11/keysym.h>

static bool translateKey(int qtKey, quint32 *nativeKey)
{
    KeySym sym = NoSymbol;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)         sym = XK_a + (qtKey - Qt::Key_A);
    else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)    sym = XK_0 + (qtKey - Qt::Key_0);
    else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12) sym = XK_F1 + (qtKey - Qt::Key_F1);
    else switch (qtKey) {
        case Qt::Key_Space:  sym = XK_space;  break;
        case Qt::Key_Tab:    sym = XK_Tab;    break;
        case Qt::Key_Escape: sym = XK_Escape; break;
        case Qt::Key_Left:   sym = XK_Left;   break;
        case Qt::Key_Right:  sym = XK_Right;  break;
        case Qt::Key_Up:     sym = XK_Up;     break;
        case Qt::Key_Down:   sym = XK_Down;   break;
        default: return false;
    }

    Display *d = XOpenDisplay(nullptr);
    if (!d)
        return false;
    const KeyCode code = XKeysymToKeycode(d, sym);
    XCloseDisplay(d);
    if (code == 0)
        return false;
    *nativeKey = code;
    return true;
}

static quint32 translateMods(Qt::KeyboardModifiers m)
{
    quint32 r = 0;
    if (m & Qt::ShiftModifier)   r |= ShiftMask;
    if (m & Qt::ControlModifier) r |= ControlMask;
    if (m & Qt::AltModifier)     r |= Mod1Mask;
    if (m & Qt::MetaModifier)    r |= Mod4Mask;
    return r;
}

// NumLock, CapsLock y ScrollLock cambian el estado de modificadores, así que hay
// que registrar el atajo con todas las combinaciones para que no deje de funcionar.
static const quint32 kLockMasks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };

#endif

// ---------------------------------------------------------------------------

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(30);
    connect(m_pollTimer, &QTimer::timeout, this, &HotkeyManager::pollReleases);

#if defined(Q_OS_WIN)
    QCoreApplication::instance()->installNativeEventFilter(this);
#elif defined(Q_OS_MACOS)
    g_macManager = this;
    EventTypeSpec spec;
    spec.eventClass = kEventClassKeyboard;
    spec.eventKind = kEventHotKeyPressed;
    InstallApplicationEventHandler(&macHotkeyHandler, 1, &spec, nullptr, nullptr);
#else
    Display *d = XOpenDisplay(nullptr);
    m_display = d;
    if (d) {
        XSetErrorHandler([](Display *, XErrorEvent *) -> int { return 0; });
        m_notifier = new QSocketNotifier(ConnectionNumber(d), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, [this]() { processX11Events(); });
    }
#endif
}

HotkeyManager::~HotkeyManager()
{
    unbindAll();
#if defined(Q_OS_WIN)
    QCoreApplication::instance()->removeNativeEventFilter(this);
#elif defined(Q_OS_LINUX)
    if (m_display)
        XCloseDisplay(static_cast<Display *>(m_display));
#endif
}

bool HotkeyManager::isSequenceSupported(const QString &sequence)
{
    const QKeySequence seq(sequence);
    if (seq.count() != 1)
        return false;
    quint32 nativeKey = 0;
    return translateKey(seq[0].key(), &nativeKey);
}

bool HotkeyManager::bind(int id, const QString &sequence, bool wantRelease)
{
    if (sequence.trimmed().isEmpty())
        return false;

    const QKeySequence seq(sequence);
    if (seq.count() != 1)
        return false;

    const QKeyCombination combo = seq[0];
    Binding b;
    b.id = id;
    b.qtKey = combo.key();
    b.wantRelease = wantRelease;

    if (!translateKey(b.qtKey, &b.nativeKey))
        return false;
    b.nativeMods = translateMods(combo.keyboardModifiers());

    const int index = m_bindings.size();

#if defined(Q_OS_WIN)
    if (!RegisterHotKey(nullptr, index + 1, b.nativeMods | MOD_NOREPEAT, b.nativeKey))
        return false;
#elif defined(Q_OS_MACOS)
    EventHotKeyID hkId;
    hkId.signature = 'PtBr';
    hkId.id = index + 1;
    EventHotKeyRef ref = nullptr;
    if (RegisterEventHotKey(b.nativeKey, b.nativeMods, hkId,
                            GetApplicationEventTarget(), 0, &ref) != noErr)
        return false;
    b.platformRef = ref;
    g_macIdToIndex.insert(index + 1, index);
#else
    Q_UNUSED(index);
    Display *d = static_cast<Display *>(m_display);
    if (!d)
        return false;
    const Window root = DefaultRootWindow(d);
    for (quint32 lock : kLockMasks)
        XGrabKey(d, b.nativeKey, b.nativeMods | lock, root, True, GrabModeAsync, GrabModeAsync);
    XSync(d, False);
#endif

    m_bindings.append(b);
    if (wantRelease && !m_pollTimer->isActive())
        m_pollTimer->start();
    return true;
}

void HotkeyManager::unbindAll()
{
#if defined(Q_OS_WIN)
    for (int i = 0; i < m_bindings.size(); ++i)
        UnregisterHotKey(nullptr, i + 1);
#elif defined(Q_OS_MACOS)
    for (Binding &b : m_bindings)
        if (b.platformRef)
            UnregisterEventHotKey(static_cast<EventHotKeyRef>(b.platformRef));
    g_macIdToIndex.clear();
#else
    Display *d = static_cast<Display *>(m_display);
    if (d) {
        const Window root = DefaultRootWindow(d);
        for (const Binding &b : m_bindings)
            for (quint32 lock : kLockMasks)
                XUngrabKey(d, b.nativeKey, b.nativeMods | lock, root);
        XSync(d, False);
    }
#endif
    m_bindings.clear();
    m_pollTimer->stop();
}

void HotkeyManager::handleNativePress(int index)
{
    if (index < 0 || index >= m_bindings.size())
        return;
    Binding &b = m_bindings[index];
    if (b.wantRelease) {
        if (b.active)
            return; // repetición automática de teclado: ignorar
        b.active = true;
        if (!m_pollTimer->isActive())
            m_pollTimer->start();
    }
    emit pressed(b.id);
}

void HotkeyManager::pollReleases()
{
    bool anyActive = false;
    for (Binding &b : m_bindings) {
        if (!b.active)
            continue;
        if (!KeyState::isPressed(b.qtKey)) {
            b.active = false;
            emit released(b.id);
        } else {
            anyActive = true;
        }
    }
    if (!anyActive)
        m_pollTimer->stop();
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
#if defined(Q_OS_WIN)
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            handleNativePress(static_cast<int>(msg->wParam) - 1);
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

#if defined(Q_OS_LINUX)
void HotkeyManager::processX11Events()
{
    Display *d = static_cast<Display *>(m_display);
    if (!d)
        return;

    while (XPending(d)) {
        XEvent ev;
        XNextEvent(d, &ev);
        if (ev.type != KeyPress)
            continue;
        const quint32 cleanMods = ev.xkey.state & ~(LockMask | Mod2Mask);
        for (int i = 0; i < m_bindings.size(); ++i) {
            const Binding &b = m_bindings.at(i);
            if (b.nativeKey == ev.xkey.keycode && b.nativeMods == cleanMods) {
                handleNativePress(i);
                break;
            }
        }
    }
}
#endif

#if defined(Q_OS_MACOS)
static OSStatus macHotkeyHandler(EventHandlerCallRef, EventRef event, void *)
{
    EventHotKeyID hkId;
    if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                          sizeof(hkId), nullptr, &hkId) != noErr)
        return eventNotHandledErr;
    if (g_macManager && g_macIdToIndex.contains(hkId.id)) {
        const int index = g_macIdToIndex.value(hkId.id);
        HotkeyManager *mgr = g_macManager;
        QMetaObject::invokeMethod(mgr, [mgr, index]() { mgr->handleNativePress(index); },
                                  Qt::QueuedConnection);
    }
    return noErr;
}
#endif
