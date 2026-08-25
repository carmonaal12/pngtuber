// Prueba funcional: carga un GIF, aplica escalas y anclajes, e imprime geometría.
#include "../src/overlaywindow.h"
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>

static void check(const char* label, bool ok) {
    qInfo().noquote() << (ok ? "  OK  " : " FALLO") << label;
    if (!ok) qApp->exit(1);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QScreen* s = QGuiApplication::primaryScreen();
    qInfo().noquote() << "Pantalla:" << s->name() << s->geometry();

    Profile p;
    p.name = "test";
    p.idleGif = "/tmp/test.gif";
    p.scalePercent = 100;
    p.align = Align::End;
    p.barMode = BarMode::Above;

    OverlayWindow w(s);
    w.applyProfile(p);
    qInfo().noquote() << "escala 100%:" << w.geometry();
    check("GIF cargado con su tamaño nativo 120x160", w.size() == QSize(120,160));
    check("anclado al borde derecho", w.geometry().right() == s->geometry().right());
    check("apoyado en el borde inferior", w.geometry().bottom() == s->geometry().bottom());

    p.scalePercent = 250;
    w.applyProfile(p);
    qInfo().noquote() << "escala 250%:" << w.geometry();
    check("escala 250% -> 300x400", w.size() == QSize(300,400));

    p.align = Align::Center;
    p.offsetX = 40; p.offsetY = -25;
    w.applyProfile(p);
    qInfo().noquote() << "centrado + desplazamiento:" << w.geometry();
    const int expectedX = s->geometry().center().x() - 300/2 + 40;
    check("centrado horizontalmente con desplazamiento X", w.geometry().x() == expectedX);
    check("desplazamiento Y aplicado", w.geometry().bottom() == s->geometry().bottom() - 25);

    // Variación
    Variation v; v.name="alt"; v.gifPath="/tmp/test.gif"; v.mode=TriggerMode::Hold;
    p.variations.append(v);
    w.applyProfile(p);
    w.showVariation(0);
    check("cambia a la variación 0", w.currentVariation() == 0);
    w.showVariation(-1);
    check("vuelve al idle", w.currentVariation() == -1);

    w.setClickThrough(false);
    check("click-through desactivable", w.clickThrough() == false);
    w.setClickThrough(true);
    check("click-through reactivable", w.clickThrough() == true);

    QTimer::singleShot(300, &app, [](){ qApp->quit(); });
    return app.exec();
}
