/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtTest/QTest>
#include "Qt3DRender/QRenderAspect"
#include "Qt3DRender/private/renderer_p.h"
#include "Qt3DRender/private/nodemanagers_p.h"
#include "Qt3DRender/private/rendercapture_p.h"
#include <Qt3DRender/private/sendrendercapturejob_p.h>
#include "testpostmanarbiter.h"

class tst_SendRenderCaptureJob : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSendRenderCaptureRequest()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::Render::RenderCapture *renderCapture = new Qt3DRender::Render::RenderCapture;
        Qt3DCore::QBackendNodePrivate::get(renderCapture)->setArbiter(&arbiter);

        QImage image(10, 10, QImage::Format_ARGB32);

        Qt3DRender::Render::Renderer renderer(Qt3DRender::QRenderAspect::Synchronous);
        Qt3DRender::Render::SendRenderCaptureJob job(&renderer);

        Qt3DRender::Render::NodeManagers nodeManagers;
        nodeManagers.frameGraphManager()->appendNode(renderCapture->peerId(), renderCapture);
        renderer.setNodeManagers(&nodeManagers);
        job.setManagers(&nodeManagers);

        renderCapture->requestCapture(42);
        renderCapture->acknowledgeCaptureRequest();
        renderCapture->addRenderCapture(image);
        renderer.addRenderCaptureSendRequest(renderCapture->peerId());

        //WHEN
        job.run();

        //THEN
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->subjectId(), renderCapture->peerId());
        QCOMPARE(change->propertyName(), "renderCaptureData");
        auto data = change->value().value<Qt3DRender::RenderCaptureDataPtr>();
        QCOMPARE(data.data()->captureId, 42);
        QCOMPARE(data.data()->image.width(), 10);
        QCOMPARE(data.data()->image.height(), 10);
        QCOMPARE(data.data()->image.format(), QImage::Format_ARGB32);

        // renderCapture will be deallocated by the nodeManagers destructor
    }
};

QTEST_APPLESS_MAIN(tst_SendRenderCaptureJob)

#include "tst_sendrendercapturejob.moc"
