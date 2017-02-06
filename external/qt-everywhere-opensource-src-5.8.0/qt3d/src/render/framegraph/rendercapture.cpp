/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <Qt3DRender/private/qrendercapture_p.h>
#include <Qt3DRender/private/rendercapture_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

RenderCapture::RenderCapture()
    : FrameGraphNode(FrameGraphNode::RenderCapture, QBackendNode::ReadWrite)
{

}

void RenderCapture::requestCapture(int captureId)
{
    QMutexLocker lock(&m_mutex);
    m_requestedCaptures.push_back(captureId);
}

bool RenderCapture::wasCaptureRequested() const
{
    return m_requestedCaptures.size() > 0 && isEnabled();
}

void RenderCapture::acknowledgeCaptureRequest()
{
    m_acknowledgedCaptures.push_back(m_requestedCaptures.takeFirst());
}

void RenderCapture::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("renderCaptureRequest")) {
            requestCapture(propertyChange->value().toInt());
        }
    }
    markDirty(AbstractRenderer::AllDirty);
    FrameGraphNode::sceneChangeEvent(e);
}

// called by render thread
void RenderCapture::addRenderCapture(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    auto data = RenderCaptureDataPtr::create();
    data.data()->captureId = m_acknowledgedCaptures.takeFirst();
    data.data()->image = image;
    m_renderCaptureData.push_back(data);
}

// called by send render capture job thread
void RenderCapture::sendRenderCaptures()
{
    QMutexLocker lock(&m_mutex);

    for (const RenderCaptureDataPtr data : m_renderCaptureData) {
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("renderCaptureData");
        e->setValue(QVariant::fromValue(data));
        notifyObservers(e);
    }
    m_renderCaptureData.clear();
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
