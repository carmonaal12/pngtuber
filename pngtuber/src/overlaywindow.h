#pragma once

#include "config.h"

#include <QPoint>
#include <QRect>
#include <QWidget>

class QMovie;
class QScreen;

// Ventana sin bordes, siempre encima, con transparencia por píxel real.
// Dibuja el fotograma actual del GIF escalado. Una instancia por monitor activo.
class OverlayWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWindow(QScreen *screen, QWidget *parent = nullptr);

    void applyProfile(const Profile &profile);
    void showVariation(int index); // -1 = volver al GIF idle
    int currentVariation() const { return m_currentVariation; }

    void setClickThrough(bool enabled);
    bool clickThrough() const { return m_clickThrough; }

    QScreen *targetScreen() const { return m_screen; }

signals:
    // El usuario ha arrastrado el overlay: nuevos desplazamientos respecto al anclaje.
    void dragged(int offsetX, int offsetY);
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void loadGif(const QString &path);
    void relayout();
    QPoint anchoredPosition(const QSize &size) const;

    // Devuelve la franja ocupada por la barra de tareas en este monitor, o un
    // QRect nulo si no hay barra (monitor secundario, pantalla completa, etc.).
    QRect detectTaskbar(Qt::Edge *edgeOut) const;

    QScreen *m_screen = nullptr;
    QMovie *m_movie = nullptr;
    Profile m_profile;
    int m_currentVariation = -1;
    bool m_clickThrough = true;

    bool m_dragging = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartPos;
};
