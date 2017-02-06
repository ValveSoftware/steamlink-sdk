/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylanddnd_p.h"

#include "qwaylanddatadevice_p.h"
#include "qwaylanddatadevicemanager_p.h"
#include "qwaylanddataoffer_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylanddisplay_p.h"

#include <QtGui/private/qshapedpixmapdndwindow_p.h>

#include <QDebug>

QT_BEGIN_NAMESPACE
#if QT_CONFIG(draganddrop)
namespace QtWaylandClient {

QWaylandDrag::QWaylandDrag(QWaylandDisplay *display)
    : m_display(display)
{
}

QWaylandDrag::~QWaylandDrag()
{
}

QMimeData * QWaylandDrag::platformDropData()
{
    if (drag())
        return drag()->mimeData();
    return 0;
}

void QWaylandDrag::startDrag()
{
    QBasicDrag::startDrag();
    QWaylandWindow *icon = static_cast<QWaylandWindow *>(shapedPixmapWindow()->handle());
    m_display->currentInputDevice()->dataDevice()->startDrag(drag()->mimeData(), icon);
    icon->addAttachOffset(-drag()->hotSpot());
}

void QWaylandDrag::cancel()
{
    QBasicDrag::cancel();

    m_display->currentInputDevice()->dataDevice()->cancelDrag();
}

void QWaylandDrag::move(const QPoint &globalPos)
{
    Q_UNUSED(globalPos);
    // Do nothing
}

void QWaylandDrag::drop(const QPoint &globalPos)
{
    Q_UNUSED(globalPos);
    // Do nothing
}

void QWaylandDrag::endDrag()
{
    m_display->currentInputDevice()->handleEndDrag();
}

void QWaylandDrag::updateTarget(const QString &mimeType)
{
    setCanDrop(!mimeType.isEmpty());

    if (canDrop()) {
        updateCursor(defaultAction(drag()->supportedActions(), m_display->currentInputDevice()->modifiers()));
    } else {
        updateCursor(Qt::IgnoreAction);
    }
}

void QWaylandDrag::setResponse(const QPlatformDragQtResponse &response)
{
    setCanDrop(response.isAccepted());

    if (canDrop()) {
        updateCursor(defaultAction(drag()->supportedActions(), m_display->currentInputDevice()->modifiers()));
    } else {
        updateCursor(Qt::IgnoreAction);
    }
}

void QWaylandDrag::finishDrag(const QPlatformDropQtResponse &response)
{
    setExecutedDropAction(response.acceptedAction());
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    eventFilter(shapedPixmapWindow(), &event);
}

}
#endif  // draganddrop
QT_END_NAMESPACE
