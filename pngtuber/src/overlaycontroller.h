#pragma once

#include "config.h"

#include <QObject>
#include <QStringList>
#include <QVector>
#include <QtGui/qwindowdefs.h> // WId

class OverlayWindow;
class HotkeyManager;
class QTimer;

// Mantiene un overlay por cada monitor activo, aplica el perfil que le
// corresponde a cada uno y traduce los atajos globales en cambios de variación.
class OverlayController : public QObject
{
    Q_OBJECT

public:
    explicit OverlayController(AppConfig *config, QObject *parent = nullptr);

    // Reconstruye overlays y atajos desde cero a partir de la configuración.
    void rebuild();

    void setOverlaysVisible(bool visible);
    bool overlaysVisible() const { return m_visible; }

    // Ventana que debe quedar siempre por encima de los overlays (la de
    // ajustes mientras está abierta). Con 0 vuelven a colocarse justo por
    // debajo de la barra de tareas.
    void setStackReference(WId reference);

    // Atajos que el sistema rechazó (ya en uso por otra aplicación, o tecla no
    // soportada). Se rellena en cada rebuild().
    QStringList failedHotkeys() const { return m_failedHotkeys; }

signals:
    void configChanged();      // el usuario arrastró un overlay: hay que guardar
    void settingsRequested();  // doble clic sobre un overlay

private slots:
    void onHotkeyPressed(int id);
    void onHotkeyReleased(int id);
    void onScreensChanged();

private:
    struct Instance {
        OverlayWindow *window = nullptr;
        QString profileName;
    };

    void rebuildOverlays();
    void rebuildHotkeys();
    void applyToProfile(const QString &profileName, int variationIndex);
    void refreshStacking();

    static int encodeId(int profileIndex, int variationIndex);
    static void decodeId(int id, int *profileIndex, int *variationIndex);

    AppConfig *m_config = nullptr;
    HotkeyManager *m_hotkeys = nullptr;
    QVector<Instance> m_instances;
    QStringList m_failedHotkeys;
    QTimer *m_stackTimer = nullptr;
    WId m_stackReference = 0;
    bool m_visible = true;
};
