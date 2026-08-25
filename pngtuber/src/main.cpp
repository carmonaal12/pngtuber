#include "config.h"
#include "configwindow.h"
#include "overlaycontroller.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

// Icono generado en código para no depender de un archivo de recursos.
static QIcon buildTrayIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0x4c, 0x6e, 0xf5));
    p.setPen(Qt::NoPen);
    p.drawEllipse(4, 4, 56, 56);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(34);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("P"));
    p.end();
    return QIcon(pm);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PngtuberDesktop"));
    app.setQuitOnLastWindowClosed(false); // vive en la bandeja

    AppConfig config = AppConfig::load();

    OverlayController controller(&config);
    ConfigWindow window(&config, &controller);

    QObject::connect(&window, &ConfigWindow::requestApply, &controller, [&]() {
        controller.rebuild();
    });
    QObject::connect(&window, &ConfigWindow::requestSave, &app, [&]() {
        config.save();
    });
    QObject::connect(&controller, &OverlayController::configChanged, &app, [&]() {
        config.save();
        window.refreshFromConfig();
    });
    QObject::connect(&controller, &OverlayController::settingsRequested, &window, [&]() {
        window.show();
        window.raise();
        window.activateWindow();
    });

    controller.rebuild();
    window.refreshFromConfig();
    config.save(); // deja el archivo creado ya en el primer arranque

    QSystemTrayIcon tray(buildTrayIcon());
    tray.setToolTip(QStringLiteral("PNGTuber de escritorio"));

    QMenu trayMenu;
    QAction *openAction = trayMenu.addAction(QObject::tr("Configuración…"));
    QAction *toggleAction = trayMenu.addAction(QObject::tr("Mostrar / ocultar PNGTuber"));
    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction(QObject::tr("Salir"));

    QObject::connect(openAction, &QAction::triggered, &app, [&]() {
        window.show();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(toggleAction, &QAction::triggered, &app, [&]() {
        controller.setOverlaysVisible(!controller.overlaysVisible());
    });
    QObject::connect(quitAction, &QAction::triggered, &app, [&]() {
        config.save();
        app.quit();
    });
    QObject::connect(&tray, &QSystemTrayIcon::activated, &app,
                     [&](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger
                             || reason == QSystemTrayIcon::DoubleClick) {
                             window.show();
                             window.raise();
                             window.activateWindow();
                         }
                     });

    tray.setContextMenu(&trayMenu);
    tray.show();

    if (!config.startMinimized)
        window.show();

    return app.exec();
}
