/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandClient module of the Qt Toolkit.
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


#include "qwaylanddatadevice_p.h"

#include "qwaylanddatadevicemanager_p.h"
#include "qwaylanddataoffer_p.h"
#include "qwaylanddatasource_p.h"
#include "qwaylanddnd_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandabstractdecoration_p.h"

#include <QtCore/QMimeData>
#include <QtGui/QGuiApplication>
#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qplatformclipboard.h>
#include <qpa/qplatformdrag.h>
#include <qpa/qwindowsysteminterface.h>

#if QT_CONFIG(draganddrop)

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandDataDevice::QWaylandDataDevice(QWaylandDataDeviceManager *manager, QWaylandInputDevice *inputDevice)
    : QtWayland::wl_data_device(manager->get_data_device(inputDevice->wl_seat()))
    , m_display(manager->display())
    , m_inputDevice(inputDevice)
    , m_enterSerial(0)
    , m_dragWindow(0)
    , m_dragPoint()
    , m_dragOffer()
    , m_selectionOffer()
{
}

QWaylandDataDevice::~QWaylandDataDevice()
{
}

QWaylandDataOffer *QWaylandDataDevice::selectionOffer() const
{
    return m_selectionOffer.data();
}

void QWaylandDataDevice::invalidateSelectionOffer()
{
    m_selectionOffer.reset();
}

QWaylandDataSource *QWaylandDataDevice::selectionSource() const
{
    return m_selectionSource.data();
}

void QWaylandDataDevice::setSelectionSource(QWaylandDataSource *source)
{
    if (source)
        connect(source, &QWaylandDataSource::cancelled, this, &QWaylandDataDevice::selectionSourceCancelled);
    set_selection(source ? source->object() : Q_NULLPTR, m_inputDevice->serial());
    m_selectionSource.reset(source);
}

QWaylandDataOffer *QWaylandDataDevice::dragOffer() const
{
    return m_dragOffer.data();
}

void QWaylandDataDevice::startDrag(QMimeData *mimeData, QWaylandWindow *icon)
{
    m_dragSource.reset(new QWaylandDataSource(m_display->dndSelectionHandler(), mimeData));
    connect(m_dragSource.data(), &QWaylandDataSource::cancelled, this, &QWaylandDataDevice::dragSourceCancelled);

    QWaylandWindow *origin = m_display->currentInputDevice()->pointerFocus();
    if (!origin)
        origin = m_display->currentInputDevice()->touchFocus();

    start_drag(m_dragSource->object(), origin->object(), icon->object(), m_display->currentInputDevice()->serial());
}

void QWaylandDataDevice::cancelDrag()
{
    m_dragSource.reset();
}

void QWaylandDataDevice::data_device_data_offer(struct ::wl_data_offer *id)
{
    new QWaylandDataOffer(m_display, id);
}

void QWaylandDataDevice::data_device_drop()
{
    QDrag *drag = static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->currentDrag();

    QMimeData *dragData = 0;
    Qt::DropActions supportedActions;
    if (drag) {
        dragData = drag->mimeData();
        supportedActions = drag->supportedActions();
    } else if (m_dragOffer) {
        dragData = m_dragOffer->mimeData();
        supportedActions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    } else {
        return;
    }

    QPlatformDropQtResponse response = QWindowSystemInterface::handleDrop(m_dragWindow, dragData, m_dragPoint, supportedActions);

    if (drag) {
        static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->finishDrag(response);
    }
}

void QWaylandDataDevice::data_device_enter(uint32_t serial, wl_surface *surface, wl_fixed_t x, wl_fixed_t y, wl_data_offer *id)
{
    m_enterSerial = serial;
    m_dragWindow = QWaylandWindow::fromWlSurface(surface)->window();
    m_dragPoint = calculateDragPosition(x, y, m_dragWindow);

    QMimeData *dragData = Q_NULLPTR;
    Qt::DropActions supportedActions;

    m_dragOffer.reset(static_cast<QWaylandDataOffer *>(wl_data_offer_get_user_data(id)));
    QDrag *drag = static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->currentDrag();
    if (drag) {
        dragData = drag->mimeData();
        supportedActions = drag->supportedActions();
    } else if (m_dragOffer) {
        dragData = m_dragOffer->mimeData();
        supportedActions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    }

    const QPlatformDragQtResponse &response = QWindowSystemInterface::handleDrag(m_dragWindow, dragData, m_dragPoint, supportedActions);

    if (drag) {
        static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->setResponse(response);
    }

    if (response.isAccepted()) {
        wl_data_offer_accept(m_dragOffer->object(), m_enterSerial, m_dragOffer->firstFormat().toUtf8().constData());
    } else {
        wl_data_offer_accept(m_dragOffer->object(), m_enterSerial, 0);
    }
}

void QWaylandDataDevice::data_device_leave()
{
    if (m_dragWindow)
        QWindowSystemInterface::handleDrag(m_dragWindow, 0, QPoint(), Qt::IgnoreAction);

    QDrag *drag = static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->currentDrag();
    if (!drag) {
        m_dragOffer.reset();
    }
}

void QWaylandDataDevice::data_device_motion(uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    Q_UNUSED(time);

    QDrag *drag = static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->currentDrag();

    if (!drag && !m_dragOffer)
        return;

    m_dragPoint = calculateDragPosition(x, y, m_dragWindow);

    QMimeData *dragData;
    Qt::DropActions supportedActions;
    if (drag) {
        dragData = drag->mimeData();
        supportedActions = drag->supportedActions();
    } else {
        dragData = m_dragOffer->mimeData();
        supportedActions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    }

    QPlatformDragQtResponse response = QWindowSystemInterface::handleDrag(m_dragWindow, dragData, m_dragPoint, supportedActions);

    if (drag) {
        static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->setResponse(response);
    }

    if (response.isAccepted()) {
        wl_data_offer_accept(m_dragOffer->object(), m_enterSerial, m_dragOffer->firstFormat().toUtf8().constData());
    } else {
        wl_data_offer_accept(m_dragOffer->object(), m_enterSerial, 0);
    }
}

void QWaylandDataDevice::data_device_selection(wl_data_offer *id)
{
    if (id)
        m_selectionOffer.reset(static_cast<QWaylandDataOffer *>(wl_data_offer_get_user_data(id)));
    else
        m_selectionOffer.reset();

    QGuiApplicationPrivate::platformIntegration()->clipboard()->emitChanged(QClipboard::Clipboard);
}

void QWaylandDataDevice::selectionSourceCancelled()
{
    m_selectionSource.reset();
    QGuiApplicationPrivate::platformIntegration()->clipboard()->emitChanged(QClipboard::Clipboard);
}

void QWaylandDataDevice::dragSourceCancelled()
{
    m_dragSource.reset();
}

void QWaylandDataDevice::dragSourceTargetChanged(const QString &mimeType)
{
    static_cast<QWaylandDrag *>(QGuiApplicationPrivate::platformIntegration()->drag())->updateTarget(mimeType);
}

QPoint QWaylandDataDevice::calculateDragPosition(int x, int y, QWindow *wnd) const
{
    QPoint pnt(wl_fixed_to_int(x), wl_fixed_to_int(y));
    if (wnd) {
        QWaylandWindow *wwnd = static_cast<QWaylandWindow*>(m_dragWindow->handle());
        if (wwnd && wwnd->decoration()) {
            pnt -= QPoint(wwnd->decoration()->margins().left(),
                          wwnd->decoration()->margins().top());
        }
    }
    return pnt;
}

}

QT_END_NAMESPACE

#endif  // draganddrop
