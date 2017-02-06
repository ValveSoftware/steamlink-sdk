/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qpixeltool.h"

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qdir.h>
#include <qapplication.h>
#include <qscreen.h>
#ifndef QT_NO_CLIPBOARD
#include <qclipboard.h>
#endif
#include <qpainter.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qsettings.h>
#include <qmenu.h>
#include <qactiongroup.h>
#include <qimagewriter.h>
#include <qstandardpaths.h>
#include <private/qhighdpiscaling_p.h>

#include <qdebug.h>

QT_BEGIN_NAMESPACE

static QPoint initialPos(const QSettings &settings, const QSize &initialSize)
{
    const QDesktopWidget *desktopWidget = QApplication::desktop();
    const QPoint defaultPos = desktopWidget->availableGeometry().topLeft();
    const QPoint savedPos =
        settings.value(QLatin1String("position"), QVariant(defaultPos)).toPoint();
    const int savedScreen = desktopWidget->screenNumber(savedPos);
    return savedScreen >= 0
        && desktopWidget->availableGeometry(savedScreen).intersects(QRect(savedPos, initialSize))
        ? savedPos : defaultPos;
}

QPixelTool::QPixelTool(QWidget *parent)
    : QWidget(parent)
    , m_freeze(false)
    , m_displayZoom(false)
    , m_displayGridSize(false)
    , m_mouseDown(false)
    , m_preview_mode(false)
    , m_displayZoomId(0)
    , m_displayGridSizeId(0)
    , m_currentColor(0)
{
    setWindowTitle(QCoreApplication::applicationName());
    QSettings settings(QLatin1String("QtProject"), QLatin1String("QPixelTool"));
    m_autoUpdate = settings.value(QLatin1String("autoUpdate"), 0).toBool();
    m_gridSize = settings.value(QLatin1String("gridSize"), 1).toInt();
    m_gridActive = settings.value(QLatin1String("gridActive"), 1).toInt();
    m_zoom = settings.value(QLatin1String("zoom"), 4).toInt();
    m_initialSize = settings.value(QLatin1String("initialSize"), QSize(250, 200)).toSize();

    move(initialPos(settings, m_initialSize));

    setMouseTracking(true);
    setAttribute(Qt::WA_NoBackground);
    m_updateId = startTimer(30);
}

QPixelTool::~QPixelTool()
{
    QSettings settings(QLatin1String("QtProject"), QLatin1String("QPixelTool"));
    settings.setValue(QLatin1String("autoUpdate"), int(m_autoUpdate));
    settings.setValue(QLatin1String("gridSize"), m_gridSize);
    settings.setValue(QLatin1String("gridActive"), m_gridActive);
    settings.setValue(QLatin1String("zoom"), m_zoom);
    settings.setValue(QLatin1String("initialSize"), size());
    settings.setValue(QLatin1String("position"), pos());
}

void QPixelTool::setPreviewImage(const QImage &image)
{
    m_preview_mode = true;
    m_preview_image = image;
    m_freeze = true;
}

void QPixelTool::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_updateId && !m_freeze) {
        grabScreen();
    } else if (event->timerId() == m_displayZoomId) {
        killTimer(m_displayZoomId);
        setZoomVisible(false);
    } else if (event->timerId() == m_displayGridSizeId) {
        killTimer(m_displayGridSizeId);
        m_displayGridSize = false;
    }
}

void render_string(QPainter *p, int w, int h, const QString &text, int flags)
{
    p->setBrush(QColor(255, 255, 255, 191));
    p->setPen(Qt::black);
    QRect bounds;
    p->drawText(0, 0, w, h, Qt::TextDontPrint | flags, text, &bounds);

    if (bounds.x() == 0) bounds.adjust(0, 0, 10, 0);
    else bounds.adjust(-10, 0, 0, 0);

    if (bounds.y() == 0) bounds.adjust(0, 0, 0, 10);
    else bounds.adjust(0, -10, 0, 0);

    p->drawRect(bounds);
    p->drawText(bounds, flags, text);
}

void QPixelTool::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    if (m_preview_mode) {
        QPixmap pixmap(40, 40);
        QPainter pt(&pixmap);
        pt.fillRect(0, 0, 20, 20, Qt::white);
        pt.fillRect(20, 20, 20, 20, Qt::white);
        pt.fillRect(20, 0, 20, 20, Qt::lightGray);
        pt.fillRect(0, 20, 20, 20, Qt::lightGray);
        pt.end();
        p.fillRect(0, 0, width(), height(), pixmap);
    }

    int w = width();
    int h = height();

    p.save();
    p.scale(m_zoom, m_zoom);
    p.drawPixmap(0, 0, m_buffer);
    p.scale(1/m_zoom, 1/m_zoom);
    p.restore();

    // Draw the grid on top.
    if (m_gridActive) {
        p.setPen(m_gridActive == 1 ? Qt::black : Qt::white);
        int incr = m_gridSize * m_zoom;
        for (int x=0; x<w; x+=incr)
            p.drawLine(x, 0, x, h);
        for (int y=0; y<h; y+=incr)
            p.drawLine(0, y, w, y);
    }

    QFont f(QLatin1String("courier"));
    f.setBold(true);
    p.setFont(f);

    if (m_displayZoom) {
        render_string(&p, w, h,
                      QLatin1String("Zoom: x") + QString::number(m_zoom),
                      Qt::AlignTop | Qt::AlignRight);
    }

    if (m_displayGridSize) {
        render_string(&p, w, h,
                      QLatin1String("Grid size: ") + QString::number(m_gridSize),
                      Qt::AlignBottom | Qt::AlignLeft);
    }

    if (m_freeze) {
        QString str;
        str.sprintf("%8X (%3d,%3d,%3d,%3d)",
                    m_currentColor,
                    (0xff000000 & m_currentColor) >> 24,
                    (0x00ff0000 & m_currentColor) >> 16,
                    (0x0000ff00 & m_currentColor) >> 8,
                    (0x000000ff & m_currentColor));
        render_string(&p, w, h,
                      str,
                      Qt::AlignBottom | Qt::AlignRight);
    }

    if (m_mouseDown && m_dragStart != m_dragCurrent) {
        int x1 = (m_dragStart.x() / m_zoom) * m_zoom;
        int y1 = (m_dragStart.y() / m_zoom) * m_zoom;
        int x2 = (m_dragCurrent.x() / m_zoom) * m_zoom;
        int y2 = (m_dragCurrent.y() / m_zoom) * m_zoom;
        QRect r = QRect(x1, y1, x2 - x1, y2 - y1).normalized();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::red, 3, Qt::SolidLine));
        p.drawRect(r);
        p.setPen(QPen(Qt::black, 1, Qt::SolidLine));
        p.drawRect(r);

        QString str;
        str.sprintf("Rect: x=%d, y=%d, w=%d, h=%d",
                    r.x() / m_zoom,
                    r.y() / m_zoom,
                    r.width() / m_zoom,
                    r.height() / m_zoom);
        render_string(&p, w, h, str, Qt::AlignBottom | Qt::AlignLeft);
    }


}

void QPixelTool::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Space:
        toggleFreeze();
        break;
    case Qt::Key_Plus:
        setZoom(m_zoom + 1);
        break;
    case Qt::Key_Minus:
        setZoom(m_zoom - 1);
        break;
    case Qt::Key_PageUp:
        setGridSize(m_gridSize + 1);
        break;
    case Qt::Key_PageDown:
        setGridSize(m_gridSize - 1);
        break;
    case Qt::Key_G:
        toggleGrid();
        break;
    case Qt::Key_A:
        m_autoUpdate = !m_autoUpdate;
        break;
#ifndef QT_NO_CLIPBOARD
    case Qt::Key_C:
        if (e->modifiers() & Qt::ControlModifier)
            copyToClipboard();
        break;
#endif
    case Qt::Key_S:
        if (e->modifiers() & Qt::ControlModifier) {
            releaseKeyboard();
            saveToFile();
        }
        break;
    case Qt::Key_Control:
        grabKeyboard();
        break;
    }
}

void QPixelTool::keyReleaseEvent(QKeyEvent *e)
{
    switch(e->key()) {
    case Qt::Key_Control:
        releaseKeyboard();
        break;
    default:
        break;
    }
}

void QPixelTool::resizeEvent(QResizeEvent *)
{
    grabScreen();
}

void QPixelTool::mouseMoveEvent(QMouseEvent *e)
{
    if (m_mouseDown)
        m_dragCurrent = e->pos();

    int x = e->x() / m_zoom;
    int y = e->y() / m_zoom;

    QImage im = m_buffer.toImage().convertToFormat(QImage::Format_ARGB32);
    if (x < im.width() && y < im.height() && x >= 0 && y >= 0) {
        m_currentColor = im.pixel(x, y);
        update();
    }
}

void QPixelTool::mousePressEvent(QMouseEvent *e)
{
    if (!m_freeze)
        return;
    m_mouseDown = true;
    m_dragStart = e->pos();
}

void QPixelTool::mouseReleaseEvent(QMouseEvent *)
{
    m_mouseDown = false;
}

static QAction *addCheckableAction(QMenu &menu, const QString &title,
                                   bool value, const QKeySequence &key)
{
    QAction *result = menu.addAction(title);
    result->setCheckable(true);
    result->setChecked(value);
    result->setShortcut(key);
    return result;
}

static QAction *addCheckableAction(QMenu &menu, const QString &title,
                                   bool value, const QKeySequence &key,
                                   QActionGroup *group)
{
    QAction *result = addCheckableAction(menu, title, value, key);
    result->setActionGroup(group);
    return result;
}

void QPixelTool::contextMenuEvent(QContextMenuEvent *e)
{
    const bool tmpFreeze = m_freeze;
    m_freeze = true;

    QMenu menu;
    menu.addAction(QLatin1String("Qt Pixel Zooming Tool"))->setEnabled(false);
    menu.addSeparator();

    // Grid color options...
    QActionGroup *gridGroup = new QActionGroup(&menu);
    addCheckableAction(menu, QLatin1String("White grid"), m_gridActive == 2,
                       Qt::Key_W, gridGroup);
    QAction *blackGrid = addCheckableAction(menu, QLatin1String("Black grid"),
                                            m_gridActive == 1, Qt::Key_B, gridGroup);
    QAction *noGrid = addCheckableAction(menu, QLatin1String("No grid"), m_gridActive == 0,
                                         Qt::Key_N, gridGroup);
    menu.addSeparator();

    // Grid size options
    menu.addAction(QLatin1String("Increase grid size"),
                   this, &QPixelTool::increaseGridSize, Qt::Key_PageUp);
    menu.addAction(QLatin1String("Decrease grid size"),
                   this, &QPixelTool::decreaseGridSize, Qt::Key_PageDown);
    menu.addSeparator();

    // Zoom options
    menu.addAction(QLatin1String("Zoom in"),
                   this, &QPixelTool::increaseZoom, Qt::Key_Plus);
    menu.addAction(QLatin1String("Zoom out"),
                   this, &QPixelTool::decreaseZoom, Qt::Key_Minus);
    menu.addSeparator();

    // Freeze / Autoupdate
    QAction *freeze = addCheckableAction(menu, QLatin1String("Frozen"),
                                         tmpFreeze, Qt::Key_Space);
    QAction *autoUpdate = addCheckableAction(menu, QLatin1String("Continuous update"),
                                             m_autoUpdate, Qt::Key_A);
    menu.addSeparator();

    // Copy to clipboard / save
    menu.addAction(QLatin1String("Save as image..."),
                   this, &QPixelTool::saveToFile, QKeySequence::SaveAs);
#ifndef QT_NO_CLIPBOARD
    menu.addAction(QLatin1String("Copy to clipboard"),
                   this, &QPixelTool::copyToClipboard, QKeySequence::Copy);
#endif

    menu.addSeparator();
    menu.addAction(QLatin1String("About Qt"), qApp, &QApplication::aboutQt);

    menu.exec(mapToGlobal(e->pos()));

    // Read out grid settings
    if (noGrid->isChecked())
        m_gridActive = 0;
    else if (blackGrid->isChecked())
        m_gridActive = 1;
    else
        m_gridActive = 2;

    m_autoUpdate = autoUpdate->isChecked();
    m_freeze = freeze->isChecked();
}

QSize QPixelTool::sizeHint() const
{
    return m_initialSize;
}

static inline QString pixelToolTitle(QPoint pos)
{
    if (QHighDpiScaling::isActive()) {
        const int screenNumber = QApplication::desktop()->screenNumber(pos);
        pos = QHighDpi::toNativePixels(pos, QGuiApplication::screens().at(screenNumber));
    }
    return QCoreApplication::applicationName() + QLatin1String(" [")
        + QString::number(pos.x())
        + QLatin1String(", ") + QString::number(pos.y()) + QLatin1Char(']');
}

void QPixelTool::grabScreen()
{
    if (m_preview_mode) {
        int w = qMin(width() / m_zoom + 1, m_preview_image.width());
        int h = qMin(height() / m_zoom + 1, m_preview_image.height());
        m_buffer = QPixmap::fromImage(m_preview_image).copy(0, 0, w, h);
        update();
        return;
    }

    QPoint mousePos = QCursor::pos();
    if (mousePos == m_lastMousePos && !m_autoUpdate)
        return;

    if (m_lastMousePos != mousePos)
        setWindowTitle(pixelToolTitle(mousePos));

    int w = int(width() / float(m_zoom));
    int h = int(height() / float(m_zoom));

    if (width() % m_zoom > 0)
        ++w;
    if (height() % m_zoom > 0)
        ++h;

    int x = mousePos.x() - w/2;
    int y = mousePos.y() - h/2;

    const QBrush darkBrush = palette().color(QPalette::Dark);
    const QDesktopWidget *desktopWidget = QApplication::desktop();
    if (QScreen *screen = QGuiApplication::screens().value(desktopWidget->screenNumber(this), Q_NULLPTR)) {
        m_buffer = screen->grabWindow(desktopWidget->winId(), x, y, w, h);
    } else {
        m_buffer = QPixmap(w, h);
        m_buffer.fill(darkBrush.color());
    }
    QRegion geom(x, y, w, h);
    QRect screenRect;
    for (int i = 0; i < desktopWidget->numScreens(); ++i)
        screenRect |= desktopWidget->screenGeometry(i);
    geom -= screenRect;
    QVector<QRect> rects = geom.rects();
    if (rects.size() > 0) {
        QPainter p(&m_buffer);
        p.translate(-x, -y);
        p.setPen(Qt::NoPen);
        p.setBrush(darkBrush);
        p.drawRects(rects);
    }

    update();

    m_lastMousePos = mousePos;
}

void QPixelTool::startZoomVisibleTimer()
{
    if (m_displayZoomId > 0) {
        killTimer(m_displayZoomId);
    }
    m_displayZoomId = startTimer(5000);
    setZoomVisible(true);
}

void QPixelTool::startGridSizeVisibleTimer()
{
    if (m_gridActive) {
        if (m_displayGridSizeId > 0)
            killTimer(m_displayGridSizeId);
        m_displayGridSizeId = startTimer(5000);
        m_displayGridSize = true;
        update();
    }
}

void QPixelTool::setZoomVisible(bool visible)
{
    m_displayZoom = visible;
    update();
}

void QPixelTool::toggleFreeze()
{
    m_freeze = !m_freeze;
    if (!m_freeze)
        m_dragStart = m_dragCurrent = QPoint();
}

void QPixelTool::setZoom(int zoom)
{
    if (zoom > 0) {
        QPoint pos = m_lastMousePos;
        m_lastMousePos = QPoint();
        m_zoom = zoom;
        grabScreen();
        m_lastMousePos = pos;
        m_dragStart = m_dragCurrent = QPoint();
        startZoomVisibleTimer();
    }
}

void QPixelTool::toggleGrid()
{
    if (++m_gridActive > 2)
        m_gridActive = 0;
    update();
}

void QPixelTool::setGridSize(int gridSize)
{
    if (m_gridActive && gridSize > 0) {
        m_gridSize = gridSize;
        startGridSizeVisibleTimer();
        update();
    }
}

#ifndef QT_NO_CLIPBOARD
void QPixelTool::copyToClipboard()
{
    QGuiApplication::clipboard()->setPixmap(m_buffer);
}
#endif

void QPixelTool::saveToFile()
{
    bool oldFreeze = m_freeze;
    m_freeze = true;

    QFileDialog fileDialog(this);
    fileDialog.setWindowTitle(QLatin1String("Save as image"));
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    QStringList mimeTypes;
    foreach (const QByteArray &mimeTypeB, QImageWriter::supportedMimeTypes())
        mimeTypes.append(QString::fromLatin1(mimeTypeB));
    fileDialog.setMimeTypeFilters(mimeTypes);
    const QString pngType = QLatin1String("image/png");
    if (mimeTypes.contains(pngType)) {
        fileDialog.selectMimeTypeFilter(pngType);
        fileDialog.setDefaultSuffix(QLatin1String("png"));
    }

    while (fileDialog.exec() == QDialog::Accepted
        && !m_buffer.save(fileDialog.selectedFiles().first())) {
        QMessageBox::warning(this, QLatin1String("Unable to write image"),
                             QLatin1String("Unable to write ")
                             + QDir::toNativeSeparators(fileDialog.selectedFiles().first()));
    }
    m_freeze = oldFreeze;
}

QT_END_NAMESPACE
