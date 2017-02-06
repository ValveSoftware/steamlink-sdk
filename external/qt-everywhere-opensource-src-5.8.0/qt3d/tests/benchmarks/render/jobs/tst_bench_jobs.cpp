/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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
#include <QMatrix4x4>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QMaterial>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DRender/QRenderSettings>
#include <Qt3DRender/private/managers_p.h>

#include <Qt3DCore/private/qresourcemanager_p.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/updateworldtransformjob_p.h>
#include <Qt3DQuick/QQmlAspectEngine>
#include <Qt3DCore/private/qaspectjobmanager_p.h>
#include <Qt3DCore/private/qaspectengine_p.h>
#include <Qt3DCore/private/qaspectmanager_p.h>
#include <Qt3DCore/private/qaspectthread_p.h>
#include <Qt3DCore/private/qnodevisitor_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <QQmlComponent>
#include <QScopedPointer>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

    class QRenderAspectTester : public Qt3DRender::QRenderAspect
    {
        Q_OBJECT
    public:
        QRenderAspectTester(bool withWindow = false)
            : Qt3DRender::QRenderAspect(QRenderAspect::Synchronous)
            , m_jobManager(new Qt3DCore::QAspectJobManager())
        {
            Qt3DCore::QAbstractAspectPrivate::get(this)->m_jobManager = m_jobManager.data();
            if (withWindow) {
                m_window.reset(new QWindow());
                m_window->resize(1024, 768);

                QSurfaceFormat format;
                if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
                    format.setVersion(4, 3);
                    format.setProfile(QSurfaceFormat::CoreProfile);
                }
                format.setDepthBufferSize( 24 );
                format.setSamples( 4 );
                format.setStencilBufferSize(8);
                m_window->setFormat(format);
                m_window->create();

                QRenderAspect::onRegistered();
            }
        }

        QVector<Qt3DCore::QAspectJobPtr> worldTransformJob()
        {
            static_cast<Render::Renderer *>(d_func()->m_renderer)->m_worldTransformJob->setRoot(d_func()->m_renderer->sceneRoot());
            return QVector<Qt3DCore::QAspectJobPtr>() << static_cast<Render::Renderer *>(d_func()->m_renderer)->m_worldTransformJob;
        }

        QVector<Qt3DCore::QAspectJobPtr> updateBoundingJob()
        {
            static_cast<Render::Renderer *>(d_func()->m_renderer)->m_updateWorldBoundingVolumeJob->setManager(d_func()->m_renderer->nodeManagers()->renderNodesManager());
            return QVector<Qt3DCore::QAspectJobPtr>() << static_cast<Render::Renderer *>(d_func()->m_renderer)->m_updateWorldBoundingVolumeJob;
        }

        QVector<Qt3DCore::QAspectJobPtr> calculateBoundingVolumeJob()
        {
            static_cast<Render::Renderer *>(d_func()->m_renderer)->m_calculateBoundingVolumeJob->setRoot(d_func()->m_renderer->sceneRoot());
            return QVector<Qt3DCore::QAspectJobPtr>() << static_cast<Render::Renderer *>(d_func()->m_renderer)->m_calculateBoundingVolumeJob;
        }

        QVector<Qt3DCore::QAspectJobPtr> framePreparationJob()
        {
            static_cast<Render::Renderer *>(d_func()->m_renderer)->m_updateShaderDataTransformJob->setManagers(d_func()->m_renderer->nodeManagers());
            return QVector<Qt3DCore::QAspectJobPtr>() << static_cast<Render::Renderer *>(d_func()->m_renderer)->m_updateShaderDataTransformJob;
        }

        QVector<Qt3DCore::QAspectJobPtr> frameCleanupJob()
        {
            static_cast<Render::Renderer *>(d_func()->m_renderer)->m_cleanupJob->setRoot(d_func()->m_renderer->sceneRoot());
            return QVector<Qt3DCore::QAspectJobPtr>() << static_cast<Render::Renderer *>(d_func()->m_renderer)->m_cleanupJob;
        }

        QVector<Qt3DCore::QAspectJobPtr> renderBinJobs()
        {
            return d_func()->m_renderer->renderBinJobs();
        }

        void onRootEntityChanged(Qt3DCore::QEntity *root)
        {
            if (!m_window) {
                const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
                const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

                for (const Qt3DCore::QNodeCreatedChangeBasePtr change : creationChanges)
                    d_func()->createBackendNode(change);

                static_cast<Qt3DRender::Render::Renderer *>(d_func()->m_renderer)->m_renderSceneRoot =
                        d_func()->m_renderer->nodeManagers()
                        ->lookupResource<Qt3DRender::Render::Entity, Qt3DRender::Render::EntityManager>(root->id());
            }
        }

    private:
        QScopedPointer<Qt3DCore::QAspectJobManager> m_jobManager;
        QScopedPointer<QWindow> m_window;
    };

}

QT_END_NAMESPACE

using namespace Qt3DRender;

Qt3DCore::QEntity *loadFromQML(const QUrl &source)
{
    QQmlEngine qmlEngine;
    QQmlComponent *component = new QQmlComponent(&qmlEngine, source);

    while (component->isLoading())
        QThread::usleep(250);

    QObject *rootObj = component->create();
    return qobject_cast<Qt3DCore::QEntity *>(rootObj);
}

Qt3DCore::QEntity *buildBigScene()
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    // Camera
    Qt3DRender::QCamera *cameraEntity = new Qt3DRender::QCamera(root);
    cameraEntity->setObjectName(QStringLiteral("cameraEntity"));
    cameraEntity->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cameraEntity->setPosition(QVector3D(0, -250.0f, -50.0f));
    cameraEntity->setUpVector(QVector3D(0, 1, 0));
    cameraEntity->setViewCenter(QVector3D(0, 0, 0));

    // FrameGraph
    Qt3DRender::QRenderSettings* renderSettings = new Qt3DRender::QRenderSettings();
    Qt3DExtras::QForwardRenderer *forwardRenderer = new Qt3DExtras::QForwardRenderer();
    forwardRenderer->setCamera(cameraEntity);
    forwardRenderer->setClearColor(Qt::black);
    renderSettings->setActiveFrameGraph(forwardRenderer);
    root->addComponent(renderSettings);

    const float radius = 100.0f;
    const int max = 1000;
    const float det = 1.0f / max;
    // Scene
    for (int i = 0; i < max; i++) {
        Qt3DCore::QEntity *e = new Qt3DCore::QEntity();
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
        Qt3DExtras::QCylinderMesh *mesh = new Qt3DExtras::QCylinderMesh();
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        mesh->setRings(50.0f);
        mesh->setSlices(30.0f);
        mesh->setRadius(2.5f);
        mesh->setLength(5.0f);

        const float angle = M_PI * 2.0f * i * det * 10.;

        material->setDiffuse(QColor(qFabs(qCos(angle)) * 255, 204, 75));
        material->setAmbient(Qt::black);
        material->setSpecular(Qt::white);
        material->setShininess(150.0f);

        QMatrix4x4 m;
        m.translate(QVector3D(radius * qCos(angle), 200.* i * det, radius * qSin(angle)));
        m.rotate(30.0f * i, QVector3D(1.0f, 0.0f, 0.0f));
        m.rotate(45.0f * i, QVector3D(0.0f, 0.0f, 1.0f));
        transform->setMatrix(m);

        e->addComponent(transform);
        e->addComponent(mesh);
        e->addComponent(material);
        e->setParent(root);
    }

    return root;
}

class tst_benchJobs : public QObject
{
    Q_OBJECT

private:
    Qt3DCore::QEntity *m_bigSceneRoot;

public:
    tst_benchJobs()
        : m_bigSceneRoot(buildBigScene())
    {}

private Q_SLOTS:

    void updateTransformJob_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void updateTransformJob()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QRenderAspectTester aspect;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(rootEntity),
                                                                              QVector<Qt3DCore::QNodeCreatedChangeBasePtr>());

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.worldTransformJob();

        QBENCHMARK {
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }

    void updateBoundingJob_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void updateBoundingJob()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QRenderAspectTester aspect;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(rootEntity),
                                                                              QVector<Qt3DCore::QNodeCreatedChangeBasePtr>());

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.updateBoundingJob();

        QBENCHMARK {
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }

    void calculateBoundingVolumeJob_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void calculateBoundingVolumeJob()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QRenderAspectTester aspect;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(rootEntity),
                                                                              QVector<Qt3DCore::QNodeCreatedChangeBasePtr>());

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.calculateBoundingVolumeJob();

        QBENCHMARK {
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }

    void framePreparationJob_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void framePreparationJob()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QRenderAspectTester aspect;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(rootEntity),
                                                                              QVector<Qt3DCore::QNodeCreatedChangeBasePtr>());

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.framePreparationJob();

        QBENCHMARK {
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }

    void frameCleanupJob_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void frameCleanupJob()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QRenderAspectTester aspect;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(rootEntity),
                                                                              QVector<Qt3DCore::QNodeCreatedChangeBasePtr>());

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.frameCleanupJob();
        QBENCHMARK {
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }

    /*  Note: The renderer still needs to be simplified to run
        these jobs
    void renderBinJobs_data()
    {
        QTest::addColumn<Qt3DCore::QEntity*>("rootEntity");
        QTest::newRow("bigscene") << m_bigSceneRoot;
    }

    void renderBinJobs()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity*, rootEntity);
        QScopedPointer<QThread> aspectThread(new QThread);
        QRenderAspectTester aspect(true);
        aspect.moveToThread(aspectThread.data());

        qDebug() << 1;

        Qt3DCore::QAbstractAspectPrivate::get(&aspect)->registerAspect(qobject_cast<Qt3DCore::QEntity *>(rootEntity));
        qDebug() << 2;

        // WHEN
        QVector<Qt3DCore::QAspectJobPtr> jobs = aspect.renderBinJobs();
        qDebug() << 3;
        QBENCHMARK {
            qDebug() << 4;
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->enqueueJobs(jobs);
            Qt3DCore::QAbstractAspectPrivate::get(&aspect)->jobManager()->waitForAllJobs();
        }
    }
    */
};

QTEST_MAIN(tst_benchJobs)

#include "tst_bench_jobs.moc"
