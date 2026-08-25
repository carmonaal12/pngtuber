#include "overlaycontroller.h"
#include "hotkeymanager.h"
#include "overlaywindow.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

namespace {
constexpr int kIdClickThrough = -1;
constexpr int kIdVisibility = -2;
constexpr int kVariationsPerProfile = 1000;

// Cada cuánto se recoloca el overlay en el orden Z. La barra de tareas se
// reposiciona sola cada vez que recibe el foco, así que hace falta insistir
// para que el PNGTuber no vuelva a aparecer por delante de ella.
constexpr int kStackingIntervalMs = 400;
}

OverlayController::OverlayController(AppConfig *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_hotkeys = new HotkeyManager(this);
    connect(m_hotkeys, &HotkeyManager::pressed, this, &OverlayController::onHotkeyPressed);
    connect(m_hotkeys, &HotkeyManager::released, this, &OverlayController::onHotkeyReleased);

    connect(qApp, &QGuiApplication::screenAdded, this, &OverlayController::onScreensChanged);
    connect(qApp, &QGuiApplication::screenRemoved, this, &OverlayController::onScreensChanged);

    m_stackTimer = new QTimer(this);
    m_stackTimer->setInterval(kStackingIntervalMs);
    connect(m_stackTimer, &QTimer::timeout, this, &OverlayController::refreshStacking);
    m_stackTimer->start();
}

void OverlayController::setStackReference(WId reference)
{
    if (m_stackReference == reference)
        return;
    m_stackReference = reference;
    refreshStacking();
}

void OverlayController::refreshStacking()
{
    if (!m_visible)
        return;
    for (Instance &inst : m_instances) {
        if (!inst.window)
            continue;
        inst.window->setStackReference(m_stackReference);
        inst.window->applyStacking();
    }
}

int OverlayController::encodeId(int profileIndex, int variationIndex)
{
    // variationIndex == -1 significa "volver al idle" y no se usa como atajo.
    return profileIndex * kVariationsPerProfile + variationIndex + 1;
}

void OverlayController::decodeId(int id, int *profileIndex, int *variationIndex)
{
    *profileIndex = id / kVariationsPerProfile;
    *variationIndex = (id % kVariationsPerProfile) - 1;
}

void OverlayController::rebuild()
{
    rebuildOverlays();
    rebuildHotkeys();
}

void OverlayController::rebuildOverlays()
{
    for (Instance &inst : m_instances) {
        if (inst.window) {
            inst.window->hide();
            inst.window->deleteLater();
        }
    }
    m_instances.clear();

    const QList<QScreen *> screens = QGuiApplication::screens();

    // Sincroniza la lista de monitores de la configuración con los reales.
    for (QScreen *screen : screens) {
        if (!m_config->monitorByName(screen->name())) {
            MonitorAssignment m;
            m.screenName = screen->name();
            m.enabled = m_config->monitors.isEmpty(); // el primero, activo por defecto
            m.profileName = m_config->profiles.isEmpty() ? QString()
                                                         : m_config->profiles.first().name;
            m_config->monitors.append(m);
        }
    }

    for (QScreen *screen : screens) {
        MonitorAssignment *assign = m_config->monitorByName(screen->name());
        if (!assign || !assign->enabled)
            continue;
        const Profile *profile = m_config->profileByName(assign->profileName);
        if (!profile)
            continue;

        auto *window = new OverlayWindow(screen);
        const QString profileName = profile->name;

        connect(window, &OverlayWindow::dragged, this,
                [this, profileName](int dx, int dy) {
                    if (Profile *p = m_config->profileByName(profileName)) {
                        p->offsetX = dx;
                        p->offsetY = dy;
                        emit configChanged();
                    }
                });
        connect(window, &OverlayWindow::settingsRequested,
                this, &OverlayController::settingsRequested);

        window->setStackReference(m_stackReference);
        window->applyProfile(*profile);
        if (!m_visible)
            window->hide();

        Instance inst;
        inst.window = window;
        inst.profileName = profileName;
        m_instances.append(inst);
    }
}

void OverlayController::rebuildHotkeys()
{
    m_hotkeys->unbindAll();
    m_failedHotkeys.clear();

    for (int pi = 0; pi < m_config->profiles.size(); ++pi) {
        const Profile &p = m_config->profiles.at(pi);
        for (int vi = 0; vi < p.variations.size(); ++vi) {
            const Variation &v = p.variations.at(vi);
            if (v.hotkey.trimmed().isEmpty())
                continue;
            const bool hold = (v.mode == TriggerMode::Hold);
            if (!m_hotkeys->bind(encodeId(pi, vi), v.hotkey, hold))
                m_failedHotkeys << QStringLiteral("%1 – %2 (%3)").arg(p.name, v.name, v.hotkey);
        }
    }

    if (!m_config->hotkeyToggleClickThrough.trimmed().isEmpty())
        if (!m_hotkeys->bind(kIdClickThrough, m_config->hotkeyToggleClickThrough, false))
            m_failedHotkeys << QStringLiteral("Alternar clics (%1)").arg(m_config->hotkeyToggleClickThrough);

    if (!m_config->hotkeyToggleVisibility.trimmed().isEmpty())
        if (!m_hotkeys->bind(kIdVisibility, m_config->hotkeyToggleVisibility, false))
            m_failedHotkeys << QStringLiteral("Mostrar/ocultar (%1)").arg(m_config->hotkeyToggleVisibility);
}

void OverlayController::applyToProfile(const QString &profileName, int variationIndex)
{
    for (Instance &inst : m_instances)
        if (inst.profileName == profileName && inst.window)
            inst.window->showVariation(variationIndex);
}

void OverlayController::onHotkeyPressed(int id)
{
    if (id == kIdClickThrough) {
        for (Instance &inst : m_instances) {
            if (!inst.window)
                continue;
            const bool next = !inst.window->clickThrough();
            inst.window->setClickThrough(next);
            if (Profile *p = m_config->profileByName(inst.profileName))
                p->clickThrough = next;
        }
        emit configChanged();
        return;
    }

    if (id == kIdVisibility) {
        setOverlaysVisible(!m_visible);
        return;
    }

    int pi = 0, vi = 0;
    decodeId(id, &pi, &vi);
    if (pi < 0 || pi >= m_config->profiles.size())
        return;
    const Profile &p = m_config->profiles.at(pi);
    if (vi < 0 || vi >= p.variations.size())
        return;

    if (p.variations.at(vi).mode == TriggerMode::Hold) {
        applyToProfile(p.name, vi);
        return;
    }

    // Modo conmutar: si ya está activa, se vuelve al idle.
    bool alreadyActive = false;
    for (const Instance &inst : m_instances)
        if (inst.profileName == p.name && inst.window && inst.window->currentVariation() == vi)
            alreadyActive = true;
    applyToProfile(p.name, alreadyActive ? -1 : vi);
}

void OverlayController::onHotkeyReleased(int id)
{
    if (id < 0)
        return;
    int pi = 0, vi = 0;
    decodeId(id, &pi, &vi);
    if (pi < 0 || pi >= m_config->profiles.size())
        return;
    const Profile &p = m_config->profiles.at(pi);
    if (vi < 0 || vi >= p.variations.size())
        return;
    if (p.variations.at(vi).mode != TriggerMode::Hold)
        return;

    // Sólo vuelve al idle si la variación que se suelta es la que está activa.
    for (Instance &inst : m_instances)
        if (inst.profileName == p.name && inst.window && inst.window->currentVariation() == vi)
            inst.window->showVariation(-1);
}

void OverlayController::setOverlaysVisible(bool visible)
{
    m_visible = visible;
    for (Instance &inst : m_instances) {
        if (!inst.window)
            continue;
        if (visible) {
            inst.window->show();
            inst.window->applyStacking();
        } else {
            inst.window->hide();
        }
    }
}

void OverlayController::onScreensChanged()
{
    rebuild();
}
