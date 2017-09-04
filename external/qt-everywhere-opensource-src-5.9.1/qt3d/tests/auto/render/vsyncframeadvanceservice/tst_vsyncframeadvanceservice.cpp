/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
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

#include <QtTest/QtTest>

#include <Qt3DRender/private/vsyncframeadvanceservice_p.h>

class FakeRenderThread Q_DECL_FINAL : public QThread
{
public:
    FakeRenderThread(Qt3DRender::Render::VSyncFrameAdvanceService *tickService)
        : m_tickService(tickService)
    {
    }

    // QThread interface
protected:
    void run() Q_DECL_FINAL
    {
        QThread::msleep(1000);
        m_tickService->proceedToNextFrame();
    }

private:
    Qt3DRender::Render::VSyncFrameAdvanceService *m_tickService;
};

class tst_VSyncFrameAdvanceService : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSynchronisation()
    {
        // GIVEN
        Qt3DRender::Render::VSyncFrameAdvanceService tickService;
        FakeRenderThread renderThread(&tickService);
        QElapsedTimer t;

        // WHEN
        t.start();
        renderThread.start();
        tickService.waitForNextFrame();

        // THEN
        // we allow for a little margin by checking for 950
        // instead of 1000
        QVERIFY(t.elapsed() >= 950);
    }

};

QTEST_MAIN(tst_VSyncFrameAdvanceService)

#include "tst_vsyncframeadvanceservice.moc"
