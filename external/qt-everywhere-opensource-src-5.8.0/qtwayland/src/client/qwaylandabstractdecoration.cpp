/****************************************************************************
**
** Copyright (C) 2016 Robin Burchell <robin.burchell@viroteck.net>
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

#include "qwaylandabstractdecoration_p.h"

#include <private/qobject_p.h>
#include "qwaylandwindow_p.h"
#include "qwaylandshellsurface_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandscreen_p.h"

#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandAbstractDecorationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWaylandAbstractDecoration)

public:
    QWaylandAbstractDecorationPrivate();
    ~QWaylandAbstractDecorationPrivate();

    QWindow *m_window;
    QWaylandWindow *m_wayland_window;

    bool m_isDirty;
    QImage m_decorationContentImage;

    Qt::MouseButtons m_mouseButtons;
};

QWaylandAbstractDecorationPrivate::QWaylandAbstractDecorationPrivate()
    : m_window(0)
    , m_wayland_window(0)
    , m_isDirty(true)
    , m_decorationContentImage(0)
    , m_mouseButtons(Qt::NoButton)
{
}

QWaylandAbstractDecorationPrivate::~QWaylandAbstractDecorationPrivate()
{
}

QWaylandAbstractDecoration::QWaylandAbstractDecoration()
    : QObject(*new QWaylandAbstractDecorationPrivate)
{
}

QWaylandAbstractDecoration::~QWaylandAbstractDecoration()
{
}

// we do this as a setter to get around plugin factory creates not really
// being a great way to pass arguments
void QWaylandAbstractDecoration::setWaylandWindow(QWaylandWindow *window)
{
    Q_D(QWaylandAbstractDecoration);

    // double initialization is probably not great
    Q_ASSERT(!d->m_window && !d->m_wayland_window);

    d->m_window = window->window();
    d->m_wayland_window = window;
}

const QImage &QWaylandAbstractDecoration::contentImage()
{
    Q_D(QWaylandAbstractDecoration);
    if (d->m_isDirty) {
        //Update the decoration backingstore

        const int scale = waylandWindow()->scale();
        const QSize imageSize = window()->frameGeometry().size() * scale;
        d->m_decorationContentImage = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
        d->m_decorationContentImage.setDevicePixelRatio(scale);
        d->m_decorationContentImage.fill(Qt::transparent);
        this->paint(&d->m_decorationContentImage);

        d->m_isDirty = false;
    }

    return d->m_decorationContentImage;
}

void QWaylandAbstractDecoration::update()
{
    Q_D(QWaylandAbstractDecoration);
    d->m_isDirty = true;
}

void QWaylandAbstractDecoration::setMouseButtons(Qt::MouseButtons mb)
{
    Q_D(QWaylandAbstractDecoration);
    d->m_mouseButtons = mb;
}

void QWaylandAbstractDecoration::startResize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize resize, Qt::MouseButtons buttons)
{
    Q_D(QWaylandAbstractDecoration);
    if (isLeftClicked(buttons) && d->m_wayland_window->shellSurface()) {
        d->m_wayland_window->shellSurface()->resize(inputDevice, resize);
        inputDevice->removeMouseButtonFromState(Qt::LeftButton);
    }
}

void QWaylandAbstractDecoration::startMove(QWaylandInputDevice *inputDevice, Qt::MouseButtons buttons)
{
    Q_D(QWaylandAbstractDecoration);
    if (isLeftClicked(buttons) && d->m_wayland_window->shellSurface()) {
        d->m_wayland_window->shellSurface()->move(inputDevice);
        inputDevice->removeMouseButtonFromState(Qt::LeftButton);
    }
}

bool QWaylandAbstractDecoration::isLeftClicked(Qt::MouseButtons newMouseButtonState)
{
    Q_D(QWaylandAbstractDecoration);
    if (!(d->m_mouseButtons & Qt::LeftButton) && (newMouseButtonState & Qt::LeftButton))
        return true;
    return false;
}

bool QWaylandAbstractDecoration::isLeftReleased(Qt::MouseButtons newMouseButtonState)
{
    Q_D(QWaylandAbstractDecoration);
    if ((d->m_mouseButtons & Qt::LeftButton) && !(newMouseButtonState & Qt::LeftButton))
        return true;
    return false;
}

bool QWaylandAbstractDecoration::isDirty() const
{
    Q_D(const QWaylandAbstractDecoration);
    return d->m_isDirty;
}

QWindow *QWaylandAbstractDecoration::window() const
{
    Q_D(const QWaylandAbstractDecoration);
    return d->m_window;
}

QWaylandWindow *QWaylandAbstractDecoration::waylandWindow() const
{
    Q_D(const QWaylandAbstractDecoration);
    return d->m_wayland_window;
}

}

QT_END_NAMESPACE
