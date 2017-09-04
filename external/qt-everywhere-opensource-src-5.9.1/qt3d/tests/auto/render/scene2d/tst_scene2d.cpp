/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <Qt3DQuickScene2D/qscene2d.h>
#include <private/qscene2d_p.h>
#include <private/scene2d_p.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qbuffer.h>
#include <private/trianglesvisitor_p.h>
#include <private/nodemanagers_p.h>
#include <private/managers_p.h>
#include <private/geometryrenderer_p.h>
#include <private/geometryrenderermanager_p.h>
#include <private/buffermanager_p.h>
#include <Qt3DRender/qpicktriangleevent.h>
#include <private/qpickevent_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <qbackendnodetester.h>
#include "testrenderer.h"

using namespace Qt3DRender::Quick;
using namespace Qt3DRender::Render;
using namespace Qt3DRender::Render::Quick;

bool qFuzzyComparePointF(const QPointF& a, const QPointF& b) {
    return qFuzzyCompare(a.x(), b.x()) && qFuzzyCompare(a.y(), b.y());
}

class TestWindow : public QQuickWindow
{
    Q_OBJECT
public:
    TestWindow()
        : QQuickWindow()
    {

    }

    bool event(QEvent *e) Q_DECL_OVERRIDE
    {
        if (e->type() >= QEvent::MouseButtonPress &&
                e->type() <= QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            m_eventTypes.push_back(e->type());
            m_mousePoints.push_back(me->localPos());
        }
        return QQuickWindow::event(e);
    }

    bool verifyEventPos(uint index, QEvent::Type type, const QPointF& pos)
    {
        if (index >= unsigned(m_eventTypes.size()) ||
            m_eventTypes[index] != type ||
            !qFuzzyComparePointF(pos, m_mousePoints[index]))
            return false;
        return true;
    }

    void clear()
    {
        m_eventTypes.clear();
        m_mousePoints.clear();
    }

private:
    QVector<QEvent::Type> m_eventTypes;
    QVector<QPointF> m_mousePoints;
};

class tst_Scene2D : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Scene2D backendScene2d;

        // THEN
        QCOMPARE(backendScene2d.isEnabled(), false);
        QVERIFY(backendScene2d.peerId().isNull());
        QCOMPARE(backendScene2d.m_context, nullptr);
        QCOMPARE(backendScene2d.m_shareContext, nullptr);
        QCOMPARE(backendScene2d.m_renderThread, nullptr);
        QCOMPARE(backendScene2d.m_outputId, Qt3DCore::QNodeId());
        QCOMPARE(backendScene2d.m_initialized, false);
        QCOMPARE(backendScene2d.m_renderInitialized, false);
        QCOMPARE(backendScene2d.m_renderPolicy, QScene2D::Continuous);
        QCOMPARE(backendScene2d.m_mouseEnabled, true);
        backendScene2d.cleanup();
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::Quick::QScene2D frontend;
        TestRenderer renderer;

        {
            // WHEN
            QScopedPointer<Scene2D> backendScene2d(new Scene2D());
            backendScene2d->setRenderer(&renderer);
            simulateInitialization(&frontend, backendScene2d.data());

            // THEN
            QCOMPARE(backendScene2d->isEnabled(), true);
            QCOMPARE(backendScene2d->peerId(), frontend.id());
            QCOMPARE(backendScene2d->m_outputId, Qt3DCore::QNodeId());
            QVERIFY(backendScene2d->m_sharedObject.data() != nullptr);
            QCOMPARE(backendScene2d->m_renderPolicy, QScene2D::Continuous);
            QCOMPARE(backendScene2d->m_mouseEnabled, true);
            backendScene2d->cleanup();
        }
        {
            // WHEN
            QScopedPointer<Scene2D> backendScene2d(new Scene2D());
            frontend.setEnabled(false);
            backendScene2d->setRenderer(&renderer);
            simulateInitialization(&frontend, backendScene2d.data());

            // THEN
            QCOMPARE(backendScene2d->peerId(), frontend.id());
            QCOMPARE(backendScene2d->isEnabled(), false);
            backendScene2d->cleanup();
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        QScopedPointer<Scene2D> backendScene2d(new Scene2D());
        TestRenderer renderer;
        QScopedPointer<Qt3DRender::QRenderTargetOutput> output(new Qt3DRender::QRenderTargetOutput());
        backendScene2d->setRenderer(&renderer);

        {
             // WHEN
             const bool newValue = false;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("enabled");
             change->setValue(newValue);
             backendScene2d->sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendScene2d->isEnabled(), newValue);
        }
        {
             // WHEN
             const Qt3DCore::QNodeId newValue = output.data()->id();
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("output");
             change->setValue(QVariant::fromValue(newValue));
             backendScene2d->sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendScene2d->m_outputId, newValue);
        }
        {
             // WHEN
             const QScene2D::RenderPolicy newValue = QScene2D::SingleShot;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("renderPolicy");
             change->setValue(QVariant::fromValue(newValue));
             backendScene2d->sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendScene2d->m_renderPolicy, newValue);
        }
        {
             // WHEN
             const bool newValue = false;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("mouseEnabled");
             change->setValue(newValue);
             backendScene2d->sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendScene2d->isEnabled(), newValue);
        }

        backendScene2d->cleanup();
    }


    void testCoordinateCalculation()
    {
        // GIVEN
        QScopedPointer<TestWindow> testWindow(new TestWindow());
        Scene2DSharedObjectPtr sharedObject(new Scene2DSharedObject(nullptr));
        QScopedPointer<Scene2D> scene2d(new Scene2D());
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        Qt3DRender::QGeometry *geometry = new Qt3DRender::QGeometry();
        Qt3DRender::QGeometryRenderer *geometryRenderer = new Qt3DRender::QGeometryRenderer();
        Qt3DRender::QAttribute *positionAttribute = new Qt3DRender::QAttribute();
        Qt3DRender::QAttribute *texcoordAttribute = new Qt3DRender::QAttribute();
        Qt3DRender::QBuffer *dataBuffer =new Qt3DRender::QBuffer();
        QScopedPointer<Qt3DCore::QEntity> entity(new Qt3DCore::QEntity());
        entity->addComponent(geometryRenderer);
        TestRenderer renderer;
        renderer.setNodeManagers(nodeManagers.data());
        scene2d->setRenderer(&renderer);
        scene2d->setEnabled(true);
        sharedObject->m_quickWindow = testWindow.data();
        scene2d->setSharedObject(sharedObject);
        testWindow->setGeometry(0,0,1024,1024);

        QByteArray data;
        data.resize(sizeof(float) * 5 * 6);
        float *dataPtr = reinterpret_cast<float *>(data.data());
        int i = 0;
        dataPtr[i++] = -1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 0;
        dataPtr[i++] = 0;

        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 0;

        dataPtr[i++] = 1.0f;
        dataPtr[i++] = -1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;

        dataPtr[i++] = -1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 0;
        dataPtr[i++] = 0;

        dataPtr[i++] = 1.0f;
        dataPtr[i++] = -1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 1.0f;

        dataPtr[i++] = -1.0f;
        dataPtr[i++] = -1.0f;
        dataPtr[i++] = 1.0f;
        dataPtr[i++] = 0.0f;
        dataPtr[i++] = 1.0f;

        dataBuffer->setData(data);
        Buffer *backendBuffer = nodeManagers->bufferManager()->getOrCreateResource(dataBuffer->id());
        backendBuffer->setRenderer(&renderer);
        backendBuffer->setManager(nodeManagers->bufferManager());
        simulateInitialization(dataBuffer, backendBuffer);

        positionAttribute->setBuffer(dataBuffer);
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setCount(6);
        positionAttribute->setByteStride(sizeof(float) * 5);
        positionAttribute->setByteOffset(0);
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(positionAttribute);

        texcoordAttribute->setBuffer(dataBuffer);
        texcoordAttribute->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        texcoordAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        texcoordAttribute->setVertexSize(2);
        texcoordAttribute->setCount(6);
        texcoordAttribute->setByteStride(sizeof(float) * 5);
        texcoordAttribute->setByteOffset(sizeof(float) * 3);
        texcoordAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(texcoordAttribute);

        geometryRenderer->setGeometry(geometry);
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

        Attribute *backendAttribute = nodeManagers->attributeManager()->getOrCreateResource(
                                        positionAttribute->id());
        backendAttribute->setRenderer(&renderer);
        simulateInitialization(positionAttribute, backendAttribute);

        Attribute *backendTexcoordAttribute = nodeManagers->attributeManager()
                                              ->getOrCreateResource(texcoordAttribute->id());
        backendTexcoordAttribute->setRenderer(&renderer);
        simulateInitialization(texcoordAttribute, backendTexcoordAttribute);

        Geometry *backendGeometry = nodeManagers->geometryManager()
                                    ->getOrCreateResource(geometry->id());
        backendGeometry->setRenderer(&renderer);
        simulateInitialization(geometry, backendGeometry);

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer, backendRenderer);

        Entity *backendEntity = nodeManagers->renderNodesManager()->getOrCreateResource(entity->id());
        backendEntity->setRenderer(&renderer);
        backendEntity->setNodeManagers(nodeManagers.data());
        simulateInitialization(entity.data(), backendEntity);

#define PICK_TRIANGLE(tri, v0, v1, v2, uvw)     \
    new Qt3DRender::QPickTriangleEvent(QPointF(), QVector3D(), QVector3D(), 0.0f,   \
    tri, v0, v1, v2, Qt3DRender::QPickEvent::LeftButton, Qt::LeftButton, 0, uvw)

        {
            // WHEN
            QVector3D uvw(1.0, 0.0f, 0.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(0, 0, 1, 2, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(0,1024)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.0, 1.0f, 0.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(0, 0, 1, 2, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(1024,1024)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.0, 0.0f, 1.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(0, 0, 1, 2, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(1024,0)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(1.0, 0.0f, 0.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(1, 3, 4, 5, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(0,1024)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.0, 1.0f, 0.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(1, 3, 4, 5, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(1024,0)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.0, 0.0f, 1.0f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(1, 3, 4, 5, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(0,0)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.5f, 0.25f, 0.25f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(0, 0, 1, 2, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(512.0f, 768.0f)));
            testWindow->clear();
        }

        {
            // WHEN
            QVector3D uvw(0.875f, 0.09375f, 0.03125f);
            Qt3DRender::QPickEventPtr ev = Qt3DRender::QPickEventPtr(PICK_TRIANGLE(1, 3, 4, 5, uvw));
            Qt3DRender::QPickEventPrivate::get(ev.data())->m_entity = entity->id();
            scene2d->handlePickEvent(QEvent::MouseButtonPress, ev);

            QCoreApplication::processEvents();

            // THEN
            QVERIFY(testWindow->verifyEventPos(0, QEvent::MouseButtonPress, QPointF(96.0f, 896.0f)));
            testWindow->clear();
        }
    }
};

QTEST_MAIN(tst_Scene2D)

#include "tst_scene2d.moc"
