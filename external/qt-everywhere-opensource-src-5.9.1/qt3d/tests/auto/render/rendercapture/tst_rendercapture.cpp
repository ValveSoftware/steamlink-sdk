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
#include <qbackendnodetester.h>
#include <Qt3DRender/private/rendercapture_p.h>
#include <Qt3DRender/qrendercapture.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "testpostmanarbiter.h"
#include "testrenderer.h"

class tst_RenderCapture : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::QRenderCapture frontend;
        Qt3DRender::Render::RenderCapture backend;

        // WHEN
        simulateInitialization(&frontend, &backend);

        // THEN
        QVERIFY(!backend.peerId().isNull());
        QCOMPARE(backend.wasCaptureRequested(), false);
        QCOMPARE(backend.isEnabled(), true);
    }

    void checkEnabledPropertyChange()
    {
        // GIVEN
        Qt3DRender::Render::RenderCapture renderCapture;
        TestRenderer renderer;
        renderCapture.setRenderer(&renderer);

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr change(new Qt3DCore::QPropertyUpdatedChange(renderCapture.peerId()));
        change->setPropertyName(QByteArrayLiteral("enabled"));
        change->setValue(QVariant::fromValue(true));
        sceneChangeEvent(&renderCapture, change);

        // THEN
        QCOMPARE(renderCapture.isEnabled(), true);
    }

    void checkReceiveRenderCaptureRequest()
    {
        // GIVEN
        Qt3DRender::Render::RenderCapture renderCapture;
        TestRenderer renderer;
        renderCapture.setRenderer(&renderer);
        renderCapture.setEnabled(true);

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr change(new Qt3DCore::QPropertyUpdatedChange(renderCapture.peerId()));
        change->setPropertyName(QByteArrayLiteral("renderCaptureRequest"));
        change->setValue(QVariant::fromValue(32));
        sceneChangeEvent(&renderCapture, change);

        // THEN
        QCOMPARE(renderCapture.wasCaptureRequested(), true);
    }

    void checkAcknowledgeCaptureRequest()
    {
        // GIVEN
        Qt3DRender::Render::RenderCapture renderCapture;
        TestRenderer renderer;
        renderCapture.setRenderer(&renderer);
        renderCapture.setEnabled(true);

        // WHEN
        renderCapture.requestCapture(2);
        renderCapture.requestCapture(4);

        // THEN
        QCOMPARE(renderCapture.wasCaptureRequested(), true);

        // WHEN
        renderCapture.acknowledgeCaptureRequest();

        // THEN
        QCOMPARE(renderCapture.wasCaptureRequested(), true);

        // WHEN
        renderCapture.acknowledgeCaptureRequest();

        // THEN
        QCOMPARE(renderCapture.wasCaptureRequested(), false);
    }
};


QTEST_APPLESS_MAIN(tst_RenderCapture)

#include "tst_rendercapture.moc"
