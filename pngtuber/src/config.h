#pragma once

#include <QString>
#include <QVector>

// Cómo reacciona una variación a su atajo de teclado.
enum class TriggerMode {
    Toggle, // Se pulsa y la variación se queda fija hasta pulsar otra cosa.
    Hold    // Se muestra mientras la tecla está pulsada; al soltar vuelve al idle.
};

// Posición a lo largo del eje largo de la barra de tareas.
enum class Align { Start, Center, End };

// Relación vertical (u horizontal, si la barra está en un lateral) con la barra.
enum class BarMode {
    Above, // El GIF se apoya sobre la barra, por fuera.
    On,    // El GIF se centra dentro de la franja de la barra.
    Screen // Se ignora la barra y se ancla al borde de la pantalla.
};

struct Variation {
    QString name;
    QString gifPath;
    QString hotkey; // QKeySequence en texto, p.ej. "Ctrl+Alt+1"
    TriggerMode mode = TriggerMode::Toggle;
};

struct Profile {
    QString name = QStringLiteral("Nuevo perfil");
    QString idleGif;
    QVector<Variation> variations;

    int scalePercent = 100; // 10 .. 400
    Align align = Align::End;
    BarMode barMode = BarMode::Above;
    int offsetX = 0;
    int offsetY = 0;
    bool clickThrough = true;
    int opacityPercent = 100;
};

struct MonitorAssignment {
    QString screenName;
    bool enabled = true;
    QString profileName;
};

struct AppConfig {
    QVector<Profile> profiles;
    QVector<MonitorAssignment> monitors;

    QString hotkeyToggleClickThrough = QStringLiteral("Ctrl+Alt+C");
    QString hotkeyToggleVisibility = QStringLiteral("Ctrl+Alt+H");
    bool startMinimized = false;

    static QString filePath();
    static AppConfig load();
    void save() const;

    Profile *profileByName(const QString &name);
    const Profile *profileByName(const QString &name) const;
    MonitorAssignment *monitorByName(const QString &screenName);
};
