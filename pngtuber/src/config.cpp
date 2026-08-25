#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QJsonObject variationToJson(const Variation &v)
{
    QJsonObject o;
    o["name"] = v.name;
    o["gif"] = v.gifPath;
    o["hotkey"] = v.hotkey;
    o["mode"] = (v.mode == TriggerMode::Hold) ? "hold" : "toggle";
    return o;
}

Variation variationFromJson(const QJsonObject &o)
{
    Variation v;
    v.name = o["name"].toString();
    v.gifPath = o["gif"].toString();
    v.hotkey = o["hotkey"].toString();
    v.mode = (o["mode"].toString() == "hold") ? TriggerMode::Hold : TriggerMode::Toggle;
    return v;
}

QString alignToString(Align a)
{
    switch (a) {
    case Align::Start:  return QStringLiteral("start");
    case Align::Center: return QStringLiteral("center");
    case Align::End:    return QStringLiteral("end");
    }
    return QStringLiteral("end");
}

Align alignFromString(const QString &s)
{
    if (s == "start")  return Align::Start;
    if (s == "center") return Align::Center;
    return Align::End;
}

QString barModeToString(BarMode m)
{
    switch (m) {
    case BarMode::Above:  return QStringLiteral("above");
    case BarMode::Screen: return QStringLiteral("screen");
    }
    return QStringLiteral("above");
}

BarMode barModeFromString(const QString &s)
{
    if (s == "screen") return BarMode::Screen;
    // "on" es un modo antiguo (centrado dentro de la barra) que ya no existe:
    // las configuraciones viejas pasan a apoyarse sobre la barra.
    return BarMode::Above;
}

// Devuelve el porcentaje de escala saneado: cualquier valor ausente, corrupto
// o fuera de rango vuelve al 100 %.
int sanitizeScale(const QJsonValue &value)
{
    if (!value.isDouble())
        return kScaleDefault;
    const int scale = value.toInt(kScaleDefault);
    if (scale < kScaleMin || scale > kScaleMax)
        return kScaleDefault;
    return scale;
}

} // namespace

QString AppConfig::filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/config.json");
}

AppConfig AppConfig::load()
{
    AppConfig cfg;

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        // Primera ejecución: perfil vacío por defecto.
        Profile p;
        p.name = QStringLiteral("Principal");
        cfg.profiles.append(p);
        return cfg;
    }

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

    const QJsonArray profiles = root["profiles"].toArray();
    for (const QJsonValue &pv : profiles) {
        const QJsonObject po = pv.toObject();
        Profile p;
        p.name = po["name"].toString();
        p.idleGif = po["idleGif"].toString();
        p.scalePercent = sanitizeScale(po["scale"]);
        p.align = alignFromString(po["align"].toString());
        p.barMode = barModeFromString(po["barMode"].toString());
        p.offsetX = po["offsetX"].toInt(0);
        p.offsetY = po["offsetY"].toInt(0);
        p.clickThrough = po["clickThrough"].toBool(true);
        p.opacityPercent = qBound(5, po["opacity"].toInt(100), 100);
        if (p.name.trimmed().isEmpty())
            p.name = QStringLiteral("Perfil %1").arg(cfg.profiles.size() + 1);
        const QJsonArray vars = po["variations"].toArray();
        for (const QJsonValue &vv : vars)
            p.variations.append(variationFromJson(vv.toObject()));
        cfg.profiles.append(p);
    }

    const QJsonArray monitors = root["monitors"].toArray();
    for (const QJsonValue &mv : monitors) {
        const QJsonObject mo = mv.toObject();
        MonitorAssignment m;
        m.screenName = mo["screen"].toString();
        m.enabled = mo["enabled"].toBool(true);
        m.profileName = mo["profile"].toString();
        cfg.monitors.append(m);
    }

    cfg.hotkeyToggleClickThrough = root["hotkeyClickThrough"].toString(cfg.hotkeyToggleClickThrough);
    cfg.hotkeyToggleVisibility = root["hotkeyVisibility"].toString(cfg.hotkeyToggleVisibility);
    cfg.startMinimized = root["startMinimized"].toBool(false);

    if (cfg.profiles.isEmpty()) {
        Profile p;
        p.name = QStringLiteral("Principal");
        cfg.profiles.append(p);
    }
    return cfg;
}

void AppConfig::save() const
{
    QJsonArray profilesArr;
    for (const Profile &p : profiles) {
        QJsonObject po;
        po["name"] = p.name;
        po["idleGif"] = p.idleGif;
        po["scale"] = p.scalePercent;
        po["align"] = alignToString(p.align);
        po["barMode"] = barModeToString(p.barMode);
        po["offsetX"] = p.offsetX;
        po["offsetY"] = p.offsetY;
        po["clickThrough"] = p.clickThrough;
        po["opacity"] = p.opacityPercent;
        QJsonArray varsArr;
        for (const Variation &v : p.variations)
            varsArr.append(variationToJson(v));
        po["variations"] = varsArr;
        profilesArr.append(po);
    }

    QJsonArray monitorsArr;
    for (const MonitorAssignment &m : monitors) {
        QJsonObject mo;
        mo["screen"] = m.screenName;
        mo["enabled"] = m.enabled;
        mo["profile"] = m.profileName;
        monitorsArr.append(mo);
    }

    QJsonObject root;
    root["profiles"] = profilesArr;
    root["monitors"] = monitorsArr;
    root["hotkeyClickThrough"] = hotkeyToggleClickThrough;
    root["hotkeyVisibility"] = hotkeyToggleVisibility;
    root["startMinimized"] = startMinimized;

    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

Profile *AppConfig::profileByName(const QString &name)
{
    for (Profile &p : profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

const Profile *AppConfig::profileByName(const QString &name) const
{
    for (const Profile &p : profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

MonitorAssignment *AppConfig::monitorByName(const QString &screenName)
{
    for (MonitorAssignment &m : monitors)
        if (m.screenName == screenName)
            return &m;
    return nullptr;
}
