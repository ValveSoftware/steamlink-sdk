/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "qdesigner_dnditem_p.h"
#include "formwindowbase_p.h"
#include <QtDesigner/private/ui4_p.h>

#include <QtGui/QPainter>
#include <QtGui/QBitmap>
#include <QtGui/QPixmap>
#include <QtGui/QImage>
#include <QtWidgets/QLabel>
#include <QtGui/QDrag>
#include <QtGui/QCursor>
#include <QtGui/QDropEvent>
#include <QtGui/QRgb>

#include <QtCore/QMultiMap>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

QDesignerDnDItem::QDesignerDnDItem(DropType type, QWidget *source) :
    m_source(source),
    m_type(type),
    m_dom_ui(0),
    m_widget(0),
    m_decoration(0)
{
}

void QDesignerDnDItem::init(DomUI *ui, QWidget *widget, QWidget *decoration,
                                    const QPoint &global_mouse_pos)
{
    Q_ASSERT(widget != 0 || ui != 0);
    Q_ASSERT(decoration != 0);

    m_dom_ui = ui;
    m_widget = widget;
    m_decoration = decoration;

    m_hot_spot = global_mouse_pos - m_decoration->geometry().topLeft();
}

QDesignerDnDItem::~QDesignerDnDItem()
{
    if (m_decoration != 0)
        m_decoration->deleteLater();
    delete m_dom_ui;
}

DomUI *QDesignerDnDItem::domUi() const
{
    return m_dom_ui;
}

QWidget *QDesignerDnDItem::decoration() const
{
    return m_decoration;
}

QPoint QDesignerDnDItem::hotSpot() const
{
    return m_hot_spot;
}

QWidget *QDesignerDnDItem::widget() const
{
    return m_widget;
}

QDesignerDnDItem::DropType QDesignerDnDItem::type() const
{
    return m_type;
}

QWidget *QDesignerDnDItem::source() const
{
    return m_source;
}

void QDesignerDnDItem::setDomUi(DomUI *dom_ui)
{
    delete m_dom_ui;
    m_dom_ui = dom_ui;
}

// ---------- QDesignerMimeData

// Make pixmap transparent on Windows only. Mac is transparent by default, Unix usually does not work.
#ifdef Q_OS_WIN
#  define TRANSPARENT_DRAG_PIXMAP
#endif

QDesignerMimeData::QDesignerMimeData(const QDesignerDnDItems &items, QDrag *drag) :
    m_items(items)
{
    enum { Alpha = 200 };
    QPoint decorationTopLeft;
    switch (m_items.size()) {
    case 0:
        break;
    case 1: {
        QWidget *deco = m_items.first()->decoration();
        decorationTopLeft = deco->pos();
        const QPixmap widgetPixmap = deco->grab(QRect(0, 0, -1, -1));
#ifdef TRANSPARENT_DRAG_PIXMAP
        QImage image(widgetPixmap.size(), QImage::Format_ARGB32);
        image.setDevicePixelRatio(widgetPixmap.devicePixelRatio());
        image.fill(QColor(Qt::transparent).rgba());
        QPainter painter(&image);
        painter.drawPixmap(QPoint(0, 0), widgetPixmap);
        painter.end();
        setImageTransparency(image, Alpha);
        drag->setPixmap(QPixmap::fromImage(image));
#else
        drag->setPixmap(widgetPixmap);
#endif
    }
        break;
    default: {
        // determine size of drag decoration by uniting all geometries
        const QDesignerDnDItems::const_iterator cend = m_items.constEnd();
        QDesignerDnDItems::const_iterator it =m_items.constBegin();
        QRect unitedGeometry = (*it)->decoration()->geometry();
        const qreal devicePixelRatio = (*it)->decoration()->devicePixelRatioF();
        for (++it; it != cend; ++it )
            unitedGeometry  = unitedGeometry .united((*it)->decoration()->geometry());

        // paint with offset. At the same time, create a mask bitmap, containing widget rectangles.
        const QSize imageSize = (QSizeF(unitedGeometry.size()) * devicePixelRatio).toSize();
        QImage image(imageSize, QImage::Format_ARGB32);
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(QColor(Qt::transparent).rgba());
        QBitmap mask(imageSize);
        mask.setDevicePixelRatio(devicePixelRatio);
        mask.clear();
        // paint with offset, determine action
        QPainter painter(&image);
        QPainter maskPainter(&mask);
        decorationTopLeft = unitedGeometry.topLeft();
        for (it = m_items.constBegin() ; it != cend; ++it ) {
            QWidget *w = (*it)->decoration();
            const QPixmap wp = w->grab(QRect(0, 0, -1, -1));
            const QPoint pos = w->pos() - decorationTopLeft;
            painter.drawPixmap(pos, wp);
            maskPainter.fillRect(QRect(pos, w->size()), Qt::color1);
        }
        painter.end();
        maskPainter.end();
#ifdef TRANSPARENT_DRAG_PIXMAP
        setImageTransparency(image, Alpha);
#endif
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setMask(mask);
        drag->setPixmap(pixmap);
    }
        break;
    }
    // determine hot spot and reconstruct the exact starting position as form window
    // introduces some offset when detecting DnD
    m_globalStartPos =  m_items.first()->decoration()->pos() +  m_items.first()->hotSpot();
    m_hotSpot = m_globalStartPos - decorationTopLeft;
    drag->setHotSpot(m_hotSpot);

    drag->setMimeData(this);
}

QDesignerMimeData::~QDesignerMimeData()
{
    const QDesignerDnDItems::const_iterator cend = m_items.constEnd();
    for (QDesignerDnDItems::const_iterator it = m_items.constBegin(); it != cend; ++it )
        delete *it;
}

Qt::DropAction QDesignerMimeData::proposedDropAction() const
{
   return m_items.first()->type() == QDesignerDnDItemInterface::CopyDrop ? Qt::CopyAction : Qt::MoveAction;
}

Qt::DropAction QDesignerMimeData::execDrag(const QDesignerDnDItems &items, QWidget * dragSource)
{
    if (items.empty())
        return Qt::IgnoreAction;

    QDrag *drag = new QDrag(dragSource);
    QDesignerMimeData *mimeData = new QDesignerMimeData(items, drag);

    // Store pointers to widgets that are to be re-shown if a move operation is canceled
    QWidgetList reshowWidgets;
    const QDesignerDnDItems::const_iterator cend = items.constEnd();
    for (QDesignerDnDItems::const_iterator it = items.constBegin(); it != cend; ++it )
        if (QWidget *w = (*it)->widget())
            if ((*it)->type() ==  QDesignerDnDItemInterface::MoveDrop)
                reshowWidgets.push_back(w);

    const Qt::DropAction executedAction = drag->exec(Qt::CopyAction|Qt::MoveAction, mimeData->proposedDropAction());

    if (executedAction == Qt::IgnoreAction && !reshowWidgets.empty())
        foreach (QWidget *w, reshowWidgets)
            w->show();

    return executedAction;
}


void QDesignerMimeData::moveDecoration(const QPoint &globalPos) const
{
    const QPoint relativeDistance = globalPos - m_globalStartPos;
    const QDesignerDnDItems::const_iterator cend = m_items.constEnd();
    for (QDesignerDnDItems::const_iterator it =m_items.constBegin(); it != cend; ++it ) {
        QWidget *w = (*it)->decoration();
        w->move(w->pos() + relativeDistance);
    }
}

void QDesignerMimeData::removeMovedWidgetsFromSourceForm(const QDesignerDnDItems &items)
{
    typedef QMultiMap<FormWindowBase *, QWidget *> FormWidgetMap;
    FormWidgetMap formWidgetMap;
    // Find moved widgets per form
    const QDesignerDnDItems::const_iterator cend = items.constEnd();
    for (QDesignerDnDItems::const_iterator it = items.constBegin(); it != cend; ++it )
        if ((*it)->type() ==  QDesignerDnDItemInterface::MoveDrop)
            if (QWidget *w = (*it)->widget())
                if (FormWindowBase *fb = qobject_cast<FormWindowBase *>((*it)->source()))
                    formWidgetMap.insert(fb, w);
    if (formWidgetMap.empty())
        return;

    foreach (FormWindowBase * fb, formWidgetMap.keys())
        fb->deleteWidgetList(formWidgetMap.values(fb));
}

void QDesignerMimeData::acceptEventWithAction(Qt::DropAction desiredAction, QDropEvent *e)
{
    if (e->proposedAction() == desiredAction) {
        e->acceptProposedAction();
    } else {
        e->setDropAction(desiredAction);
        e->accept();
    }
}

void QDesignerMimeData::acceptEvent(QDropEvent *e) const
{
    acceptEventWithAction(proposedDropAction(), e);
}

void QDesignerMimeData::setImageTransparency(QImage &image, int alpha)
{
    const int height = image.height();
    for (int l = 0; l < height; l++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(l));
        QRgb *lineEnd = line + image.width();
        for ( ; line < lineEnd; line++) {
            const QRgb rgba = *line;
            *line = qRgba(qRed(rgba), qGreen(rgba), qBlue(rgba), alpha);
        }
    }
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
