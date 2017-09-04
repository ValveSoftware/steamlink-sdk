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
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/private/qmesh_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QMesh : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QMesh mesh;

        // THEN
        QCOMPARE(mesh.source(), QUrl());
        QCOMPARE(mesh.meshName(), QString());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QMesh mesh;

        {
            // WHEN
            QSignalSpy spy(&mesh, SIGNAL(sourceChanged(QUrl)));
            const QUrl newValue(QStringLiteral("qrc:/mesh.obj"));
            mesh.setSource(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mesh.source(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mesh.setSource(newValue);

            // THEN
            QCOMPARE(mesh.source(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&mesh, SIGNAL(meshNameChanged(QString)));
            const QString newValue = QStringLiteral("MainBody");
            mesh.setMeshName(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mesh.meshName(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mesh.setMeshName(newValue);

            // THEN
            QCOMPARE(mesh.meshName(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QMesh mesh;

        mesh.setSource(QUrl(QStringLiteral("http://someRemoteURL.com")));
        mesh.setMeshName(QStringLiteral("RearPropeller"));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mesh);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QGeometryRendererData>>(creationChanges.first());
            const Qt3DRender::QGeometryRendererData cloneData = creationChangeData->data;

            Qt3DRender::MeshFunctor meshFunctor(mesh.source(), mesh.meshName());

            QVERIFY(meshFunctor == *cloneData.geometryFactory);
            QCOMPARE(mesh.id(), creationChangeData->subjectId());
            QCOMPARE(mesh.isEnabled(), true);
            QCOMPARE(mesh.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mesh.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        mesh.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mesh);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QGeometryRendererData>>(creationChanges.first());

            QCOMPARE(mesh.id(), creationChangeData->subjectId());
            QCOMPARE(mesh.isEnabled(), false);
            QCOMPARE(mesh.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mesh.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkSourceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QMesh mesh;
        arbiter.setArbiterOnNode(&mesh);

        {
            // WHEN
            mesh.setSource(QUrl(QStringLiteral("qrc:/toyplane.obj")));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "geometryFactory");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            Qt3DRender::MeshFunctor meshFunctor(mesh.source());
            Qt3DRender::QGeometryFactoryPtr factory = change->value().value<Qt3DRender::QGeometryFactoryPtr>();
            QVERIFY(meshFunctor == *factory);

            arbiter.events.clear();
        }

        {
            // WHEN
            mesh.setSource(QStringLiteral("qrc:/toyplane.obj"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMeshNameUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QMesh mesh;
        arbiter.setArbiterOnNode(&mesh);

        {
            // WHEN
            mesh.setMeshName(QStringLiteral("Phil"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "geometryFactory");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            Qt3DRender::MeshFunctor meshFunctor(QUrl(), mesh.meshName());
            Qt3DRender::QGeometryFactoryPtr factory = change->value().value<Qt3DRender::QGeometryFactoryPtr>();
            QVERIFY(meshFunctor == *factory);

            arbiter.events.clear();
        }

        {
            // WHEN
            mesh.setMeshName(QStringLiteral("Phil"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QMesh)

#include "tst_qmesh.moc"
