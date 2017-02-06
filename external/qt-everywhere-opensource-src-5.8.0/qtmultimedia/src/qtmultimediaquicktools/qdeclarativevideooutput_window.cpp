/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qdeclarativevideooutput_window_p.h"
#include "qdeclarativevideooutput_p.h"
#include <QtQuick/qquickwindow.h>
#include <QtMultimedia/qmediaservice.h>
#include <QtMultimedia/qvideowindowcontrol.h>

QT_BEGIN_NAMESPACE

QDeclarativeVideoWindowBackend::QDeclarativeVideoWindowBackend(QDeclarativeVideoOutput *parent)
    : QDeclarativeVideoBackend(parent),
      m_visible(true)
{
}

QDeclarativeVideoWindowBackend::~QDeclarativeVideoWindowBackend()
{
    releaseSource();
    releaseControl();
}

bool QDeclarativeVideoWindowBackend::init(QMediaService *service)
{
    if (QMediaControl *control = service->requestControl(QVideoWindowControl_iid)) {
        if ((m_videoWindowControl = qobject_cast<QVideoWindowControl *>(control))) {
            if (q->window())
                m_videoWindowControl->setWinId(q->window()->winId());
            m_service = service;
            QObject::connect(m_videoWindowControl.data(), SIGNAL(nativeSizeChanged()),
                             q, SLOT(_q_updateNativeSize()));
            return true;
        }
    }
    return false;
}

void QDeclarativeVideoWindowBackend::itemChange(QQuickItem::ItemChange change,
                                                const QQuickItem::ItemChangeData &changeData)
{
    if (!m_videoWindowControl)
        return;

    switch (change) {
    case QQuickItem::ItemVisibleHasChanged:
        m_visible = changeData.boolValue;
        updateGeometry();
        break;
    case QQuickItem::ItemSceneChange:
        if (changeData.window)
            m_videoWindowControl->setWinId(changeData.window->winId());
        else
            m_videoWindowControl->setWinId(0);
        break;
    default: break;
    }
}

void QDeclarativeVideoWindowBackend::releaseSource()
{
}

void QDeclarativeVideoWindowBackend::releaseControl()
{
    if (m_videoWindowControl) {
        m_videoWindowControl->setWinId(0);
        if (m_service)
            m_service->releaseControl(m_videoWindowControl);
        m_videoWindowControl = 0;
    }
}

QSize QDeclarativeVideoWindowBackend::nativeSize() const
{
    return m_videoWindowControl->nativeSize();
}

void QDeclarativeVideoWindowBackend::updateGeometry()
{
    switch (q->fillMode()) {
    case QDeclarativeVideoOutput::PreserveAspectFit:
        m_videoWindowControl->setAspectRatioMode(Qt::KeepAspectRatio); break;
    case QDeclarativeVideoOutput::PreserveAspectCrop:
        m_videoWindowControl->setAspectRatioMode(Qt::KeepAspectRatioByExpanding); break;
    case QDeclarativeVideoOutput::Stretch:
        m_videoWindowControl->setAspectRatioMode(Qt::IgnoreAspectRatio); break;
    };

    const QRectF canvasRect = q->mapRectToScene(QRectF(0, 0, q->width(), q->height()));
    m_videoWindowControl->setDisplayRect(m_visible ? canvasRect.toAlignedRect() : QRect());
}

QSGNode *QDeclarativeVideoWindowBackend::updatePaintNode(QSGNode *oldNode,
                                                         QQuickItem::UpdatePaintNodeData *data)
{
    Q_UNUSED(oldNode);
    Q_UNUSED(data);
    m_videoWindowControl->repaint();
    return 0;
}

QAbstractVideoSurface *QDeclarativeVideoWindowBackend::videoSurface() const
{
    return 0;
}

QRectF QDeclarativeVideoWindowBackend::adjustedViewport() const
{
    // No viewport supported by QVideoWindowControl, so make the viewport the same size
    // as the source
    return QRectF(QPointF(0, 0), nativeSize());
}

QT_END_NAMESPACE
