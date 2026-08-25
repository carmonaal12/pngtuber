#include "overlaywindow.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QScreen>
#include <QWindow>

OverlayWindow::OverlayWindow(QScreen *screen, QWidget *parent)
    : QWidget(parent)
    , m_screen(screen)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool                    // fuera de la barra de tareas y del Alt+Tab
                   | Qt::NoDropShadowWindowHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_AlwaysStackOnTop);
}

void OverlayWindow::applyProfile(const Profile &profile)
{
    const bool gifChanged = (m_profile.idleGif != profile.idleGif);
    m_profile = profile;

    setWindowOpacity(qBound(5, m_profile.opacityPercent, 100) / 100.0);

    if (m_currentVariation < 0 || m_currentVariation >= m_profile.variations.size()) {
        m_currentVariation = -1;
        if (gifChanged || !m_movie)
            loadGif(m_profile.idleGif);
    } else {
        loadGif(m_profile.variations.at(m_currentVariation).gifPath);
    }

    setClickThrough(m_profile.clickThrough);
    relayout();
}

void OverlayWindow::showVariation(int index)
{
    if (index >= m_profile.variations.size())
        return;
    if (index == m_currentVariation)
        return;

    m_currentVariation = index;
    const QString path = (index < 0) ? m_profile.idleGif
                                     : m_profile.variations.at(index).gifPath;
    loadGif(path);
    relayout();
}

void OverlayWindow::loadGif(const QString &path)
{
    if (m_movie) {
        m_movie->stop();
        m_movie->deleteLater();
        m_movie = nullptr;
    }
    if (path.isEmpty())
        return;

    m_movie = new QMovie(path, QByteArray(), this);
    if (!m_movie->isValid()) {
        m_movie->deleteLater();
        m_movie = nullptr;
        return;
    }
    m_movie->setCacheMode(QMovie::CacheAll);
    connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
    m_movie->jumpToFrame(0);
    m_movie->start();
}

QRect OverlayWindow::detectTaskbar(Qt::Edge *edgeOut) const
{
    if (!m_screen)
        return QRect();

    const QRect full = m_screen->geometry();
    const QRect avail = m_screen->availableGeometry();

    if (avail.top() > full.top()) {
        if (edgeOut) *edgeOut = Qt::TopEdge;
        return QRect(full.left(), full.top(), full.width(), avail.top() - full.top());
    }
    if (avail.bottom() < full.bottom()) {
        if (edgeOut) *edgeOut = Qt::BottomEdge;
        return QRect(full.left(), avail.bottom() + 1, full.width(), full.bottom() - avail.bottom());
    }
    if (avail.left() > full.left()) {
        if (edgeOut) *edgeOut = Qt::LeftEdge;
        return QRect(full.left(), full.top(), avail.left() - full.left(), full.height());
    }
    if (avail.right() < full.right()) {
        if (edgeOut) *edgeOut = Qt::RightEdge;
        return QRect(avail.right() + 1, full.top(), full.right() - avail.right(), full.height());
    }
    return QRect();
}

QPoint OverlayWindow::anchoredPosition(const QSize &size) const
{
    if (!m_screen)
        return QPoint(0, 0);

    const QRect full = m_screen->geometry();
    Qt::Edge edge = Qt::BottomEdge;
    QRect bar = detectTaskbar(&edge);

    // Sin barra detectada (o modo "pantalla"): se ancla al borde inferior.
    const bool useBar = bar.isValid() && m_profile.barMode != BarMode::Screen;
    if (!useBar) {
        // Barra ficticia de altura cero justo debajo del borde inferior: así el
        // modo "Above" deja el GIF exactamente pegado al borde de la pantalla.
        bar = QRect(full.left(), full.bottom() + 1, full.width(), 1);
        edge = Qt::BottomEdge;
    }

    const bool horizontal = (edge == Qt::TopEdge || edge == Qt::BottomEdge);
    int x = 0;
    int y = 0;

    if (horizontal) {
        switch (m_profile.align) {
        case Align::Start:  x = bar.left(); break;
        case Align::Center: x = bar.center().x() - size.width() / 2; break;
        case Align::End:    x = bar.right() - size.width() + 1; break;
        }
        if (m_profile.barMode == BarMode::On)
            y = bar.center().y() - size.height() / 2;
        else if (edge == Qt::BottomEdge)
            y = bar.top() - size.height();
        else
            y = bar.bottom() + 1;
    } else {
        switch (m_profile.align) {
        case Align::Start:  y = bar.top(); break;
        case Align::Center: y = bar.center().y() - size.height() / 2; break;
        case Align::End:    y = bar.bottom() - size.height() + 1; break;
        }
        if (m_profile.barMode == BarMode::On)
            x = bar.center().x() - size.width() / 2;
        else if (edge == Qt::LeftEdge)
            x = bar.right() + 1;
        else
            x = bar.left() - size.width();
    }

    return QPoint(x, y);
}

void OverlayWindow::relayout()
{
    if (!m_movie) {
        hide();
        return;
    }

    QSize frame = m_movie->currentPixmap().size();
    if (frame.isEmpty())
        frame = m_movie->scaledSize();
    if (frame.isEmpty())
        frame = QSize(200, 200);

    const double factor = qBound(10, m_profile.scalePercent, 400) / 100.0;
    const QSize target(qMax(1, int(frame.width() * factor)),
                       qMax(1, int(frame.height() * factor)));

    const QPoint pos = anchoredPosition(target) + QPoint(m_profile.offsetX, m_profile.offsetY);
    setGeometry(QRect(pos, target));

    if (!isVisible())
        show();
    if (windowHandle() && m_screen)
        windowHandle()->setScreen(m_screen);
    raise();
    update();
}

void OverlayWindow::setClickThrough(bool enabled)
{
    if (m_clickThrough == enabled && isVisible())
        return;
    m_clickThrough = enabled;

    const bool wasVisible = isVisible();
    const QRect geom = geometry();

    // Qt::WindowTransparentForInput se traduce al mecanismo nativo de cada sistema
    // (WS_EX_TRANSPARENT en Windows, input shape vacío en X11, ignoresMouseEvents
    // en macOS). Cambiarlo en caliente exige recrear la ventana nativa.
    setWindowFlag(Qt::WindowTransparentForInput, enabled);

    if (wasVisible) {
        show();
        setGeometry(geom);
        raise();
    }
}

void OverlayWindow::paintEvent(QPaintEvent *)
{
    if (!m_movie)
        return;
    const QPixmap frame = m_movie->currentPixmap();
    if (frame.isNull())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(rect(), frame);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_clickThrough || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    m_dragStartGlobal = event->globalPosition().toPoint();
    m_dragStartPos = pos();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging)
        return;
    const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
    move(m_dragStartPos + delta);
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_dragging)
        return;
    m_dragging = false;

    const QPoint anchor = anchoredPosition(size());
    const QPoint delta = pos() - anchor;
    m_profile.offsetX = delta.x();
    m_profile.offsetY = delta.y();
    emit dragged(delta.x(), delta.y());
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_clickThrough && event->button() == Qt::LeftButton)
        emit settingsRequested();
}
