#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;
class QSocketNotifier;

// Registra atajos de teclado a nivel de sistema operativo: funcionan aunque la
// aplicación no tenga el foco. Cada atajo se identifica con un id entero que
// elige quien llama, y se devuelve tal cual en las señales.
//
// Windows: RegisterHotKey       (user32)
// Linux:   XGrabKey sobre root  (libX11, sesión Xorg)
// macOS:   RegisterEventHotKey  (Carbon)
class HotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    // sequence en formato QKeySequence ("Ctrl+Alt+1"). wantRelease activa la
    // detección de soltada, necesaria para el modo "mantener pulsado".
    bool bind(int id, const QString &sequence, bool wantRelease);
    void unbindAll();

    // Comprueba si una secuencia es registrable sin dejarla registrada.
    static bool isSequenceSupported(const QString &sequence);

    // Público sólo porque el handler de eventos de Carbon (macOS) es una función
    // libre en C y necesita invocarlo. No llamar desde fuera.
    void handleNativePress(int index);

signals:
    void pressed(int id);
    void released(int id);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void pollReleases();

private:
    struct Binding {
        int id = 0;
        int qtKey = 0;
        quint32 nativeMods = 0;
        quint32 nativeKey = 0;
        bool wantRelease = false;
        bool active = false;
        void *platformRef = nullptr; // EventHotKeyRef en macOS
    };

    QVector<Binding> m_bindings;
    QTimer *m_pollTimer = nullptr;

#if defined(Q_OS_LINUX)
    void *m_display = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    void processX11Events();
#endif
};
