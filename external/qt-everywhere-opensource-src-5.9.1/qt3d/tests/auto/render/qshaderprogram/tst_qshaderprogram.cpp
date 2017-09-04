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
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/private/qshaderprogram_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

// We need to call QShaderProgram::sceneChangeEvent
class tst_QShaderProgram : public Qt3DRender::QShaderProgram
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QShaderProgram shaderProgram;

        // THEN
        QCOMPARE(shaderProgram.vertexShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.tessellationControlShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.tessellationEvaluationShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.geometryShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.fragmentShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.computeShaderCode(), QByteArray());
        QCOMPARE(shaderProgram.log(), QString());
        QCOMPARE(shaderProgram.status(), Qt3DRender::QShaderProgram::NotReady);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QShaderProgram shaderProgram;

        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(vertexShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "VERTEX";
            shaderProgram.setVertexShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.vertexShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setVertexShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.vertexShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(tessellationControlShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "TESSELATION_CONTROL";
            shaderProgram.setTessellationControlShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.tessellationControlShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setTessellationControlShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.tessellationControlShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(tessellationEvaluationShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "TESSELATION_EVALUTATION";
            shaderProgram.setTessellationEvaluationShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.tessellationEvaluationShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setTessellationEvaluationShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.tessellationEvaluationShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(geometryShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "GEOMETRY";
            shaderProgram.setGeometryShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.geometryShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setGeometryShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.geometryShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(fragmentShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "FRAGMENT";
            shaderProgram.setFragmentShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.fragmentShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setFragmentShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.fragmentShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&shaderProgram, SIGNAL(computeShaderCodeChanged(QByteArray)));
            const QByteArray newValue = "COMPUTE";
            shaderProgram.setComputeShaderCode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(shaderProgram.computeShaderCode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            shaderProgram.setComputeShaderCode(newValue);

            // THEN
            QCOMPARE(shaderProgram.computeShaderCode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QShaderProgram shaderProgram;

        shaderProgram.setVertexShaderCode(QByteArrayLiteral("Vertex"));
        shaderProgram.setTessellationControlShaderCode(QByteArrayLiteral("TesselControl"));
        shaderProgram.setTessellationEvaluationShaderCode(QByteArrayLiteral("TesselEval"));
        shaderProgram.setGeometryShaderCode(QByteArrayLiteral("Geometry"));
        shaderProgram.setFragmentShaderCode(QByteArrayLiteral("Fragment"));
        shaderProgram.setComputeShaderCode(QByteArrayLiteral("Compute"));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&shaderProgram);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QShaderProgramData>>(creationChanges.first());
            const Qt3DRender::QShaderProgramData cloneData = creationChangeData->data;

            QCOMPARE(shaderProgram.vertexShaderCode(), cloneData.vertexShaderCode);
            QCOMPARE(shaderProgram.tessellationControlShaderCode(), cloneData.tessellationControlShaderCode);
            QCOMPARE(shaderProgram.tessellationEvaluationShaderCode(), cloneData.tessellationEvaluationShaderCode);
            QCOMPARE(shaderProgram.geometryShaderCode(), cloneData.geometryShaderCode);
            QCOMPARE(shaderProgram.fragmentShaderCode(), cloneData.fragmentShaderCode);
            QCOMPARE(shaderProgram.computeShaderCode(), cloneData.computeShaderCode);
            QCOMPARE(shaderProgram.id(), creationChangeData->subjectId());
            QCOMPARE(shaderProgram.isEnabled(), true);
            QCOMPARE(shaderProgram.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(shaderProgram.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        shaderProgram.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&shaderProgram);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QShaderProgramData>>(creationChanges.first());
            const Qt3DRender::QShaderProgramData cloneData = creationChangeData->data;

            QCOMPARE(shaderProgram.vertexShaderCode(), cloneData.vertexShaderCode);
            QCOMPARE(shaderProgram.tessellationControlShaderCode(), cloneData.tessellationControlShaderCode);
            QCOMPARE(shaderProgram.tessellationEvaluationShaderCode(), cloneData.tessellationEvaluationShaderCode);
            QCOMPARE(shaderProgram.geometryShaderCode(), cloneData.geometryShaderCode);
            QCOMPARE(shaderProgram.fragmentShaderCode(), cloneData.fragmentShaderCode);
            QCOMPARE(shaderProgram.computeShaderCode(), cloneData.computeShaderCode);
            QCOMPARE(shaderProgram.id(), creationChangeData->subjectId());
            QCOMPARE(shaderProgram.isEnabled(), false);
            QCOMPARE(shaderProgram.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(shaderProgram.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkVertexShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setVertexShaderCode(QByteArrayLiteral("in vec3 toto;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "vertexShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.vertexShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setVertexShaderCode(QByteArrayLiteral("in vec3 toto;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkTessellationControlShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setTessellationControlShaderCode(QByteArrayLiteral("in vec3 toto2;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "tessellationControlShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.tessellationControlShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setTessellationControlShaderCode(QByteArrayLiteral("in vec3 toto2;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkTessellationEvaluationShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setTessellationEvaluationShaderCode(QByteArrayLiteral("in vec3 toto3;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "tessellationEvaluationShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.tessellationEvaluationShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setTessellationEvaluationShaderCode(QByteArrayLiteral("in vec3 toto3;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkGeometryShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setGeometryShaderCode(QByteArrayLiteral("in vec3 toto4;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "geometryShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.geometryShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setGeometryShaderCode(QByteArrayLiteral("in vec3 toto4;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFragmentShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setFragmentShaderCode(QByteArrayLiteral("out vec4 fragColor;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "fragmentShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.fragmentShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setFragmentShaderCode(QByteArrayLiteral("out vec4 fragColor;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkComputeShaderCodeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&shaderProgram);

        {
            // WHEN
            shaderProgram.setComputeShaderCode(QByteArrayLiteral("uniform vec3 temp;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "computeShaderCode");
            QCOMPARE(change->value().value<QByteArray>(), shaderProgram.computeShaderCode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            shaderProgram.setComputeShaderCode(QByteArrayLiteral("uniform vec3 temp;"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLogPropertyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        arbiter.setArbiterOnNode(this);
        QSignalSpy spy(this, SIGNAL(logChanged(QString)));
        const QString logValue = QStringLiteral("Some log...");

        // THEN
        QVERIFY(spy.isValid());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr valueChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("log");
        valueChange->setValue(QVariant::fromValue(logValue));
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 1);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(log(), logValue);

        // WHEN
        spy.clear();
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 0);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(log(), logValue);
    }

    void checkStatusPropertyUpdate()
    {
        // GIVEN
        qRegisterMetaType<Qt3DRender::QShaderProgram::Status>("Status");
        TestArbiter arbiter;
        arbiter.setArbiterOnNode(this);
        QSignalSpy spy(this, SIGNAL(statusChanged(Status)));
        const Qt3DRender::QShaderProgram::Status newStatus = Qt3DRender::QShaderProgram::Error;

        // THEN
        QVERIFY(spy.isValid());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr valueChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("status");
        valueChange->setValue(QVariant::fromValue(newStatus));
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 1);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(status(), newStatus);

        // WHEN
        spy.clear();
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 0);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(status(), newStatus);
    }

};

QTEST_MAIN(tst_QShaderProgram)

#include "tst_qshaderprogram.moc"
