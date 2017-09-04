/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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
#include <Qt3DRender/qmemorybarrier.h>
#include <Qt3DRender/private/qmemorybarrier_p.h>
#include <Qt3DRender/private/memorybarrier_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_MemoryBarrier : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::MemoryBarrier backendMemoryBarrier;

        // THEN
        QCOMPARE(backendMemoryBarrier.nodeType(), Qt3DRender::Render::FrameGraphNode::MemoryBarrier);
        QCOMPARE(backendMemoryBarrier.isEnabled(), false);
        QVERIFY(backendMemoryBarrier.peerId().isNull());
        QCOMPARE(backendMemoryBarrier.waitOperations(), Qt3DRender::QMemoryBarrier::None);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QMemoryBarrier memoryBarrier;
        memoryBarrier.setWaitOperations(Qt3DRender::QMemoryBarrier::VertexAttributeArray);

        {
            // WHEN
            Qt3DRender::Render::MemoryBarrier backendMemoryBarrier;
            simulateInitialization(&memoryBarrier, &backendMemoryBarrier);

            // THEN
            QCOMPARE(backendMemoryBarrier.isEnabled(), true);
            QCOMPARE(backendMemoryBarrier.peerId(), memoryBarrier.id());
            QCOMPARE(backendMemoryBarrier.waitOperations(), Qt3DRender::QMemoryBarrier::VertexAttributeArray);
        }
        {
            // WHEN
            Qt3DRender::Render::MemoryBarrier backendMemoryBarrier;
            memoryBarrier.setEnabled(false);
            simulateInitialization(&memoryBarrier, &backendMemoryBarrier);

            // THEN
            QCOMPARE(backendMemoryBarrier.peerId(), memoryBarrier.id());
            QCOMPARE(backendMemoryBarrier.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::MemoryBarrier backendMemoryBarrier;
        TestRenderer renderer;
        backendMemoryBarrier.setRenderer(&renderer);

        {
             // WHEN
             const bool newValue = false;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("enabled");
             change->setValue(newValue);
             backendMemoryBarrier.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendMemoryBarrier.isEnabled(), newValue);
        }
        {
             // WHEN
             const Qt3DRender::QMemoryBarrier::Operations newValue(Qt3DRender::QMemoryBarrier::AtomicCounter|Qt3DRender::QMemoryBarrier::ElementArray);
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("waitOperations");
             change->setValue(QVariant::fromValue(newValue));
             backendMemoryBarrier.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendMemoryBarrier.waitOperations(), newValue);
        }
    }

};

QTEST_MAIN(tst_MemoryBarrier)

#include "tst_memorybarrier.moc"
