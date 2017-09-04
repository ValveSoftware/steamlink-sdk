/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "inputselectionhandle.h"
#include "desktopinputselectioncontrol.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/QPainter>

namespace QtVirtualKeyboard {
InputSelectionHandle::InputSelectionHandle(DesktopInputSelectionControl *control, QWindow *eventWindow)
    : QRasterWindow()
    , m_control(control)
    , m_eventWindow(eventWindow)
{
    setFlags(Qt::ToolTip |
             Qt::FramelessWindowHint |
             Qt::WindowStaysOnTopHint |
             Qt::WindowDoesNotAcceptFocus);

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    setFormat(format);
}

void InputSelectionHandle::applyImage(const QSize &windowSize)
{
    resize(windowSize);
    update();   // update paint device in case resize() did not resize.
}

void InputSelectionHandle::paintEvent(QPaintEvent *pe)
{
    Q_UNUSED(pe);
    QPainter painter(this);

    Q_ASSERT(m_control);
    QImage *img = m_control->handleImage();
    const QSize szDelta = size() - img->size();
    // center image onto window
    painter.drawImage(QPoint(szDelta.width(), szDelta.height())/2, *img);
}

bool InputSelectionHandle::event(QEvent *e)
{
    switch (e->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
            return QCoreApplication::sendEvent(m_eventWindow, e);
        default:
            break;
    }
    return QRasterWindow::event(e);
}

} // namespace QtVirtualKeyboard
