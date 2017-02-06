/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DRender/private/qrenderstate_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/QEffect>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseMapMaterial>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/QNormalDiffuseMapMaterial>
#include <Qt3DExtras/QDiffuseSpecularMapMaterial>
#include <Qt3DExtras/QNormalDiffuseMapAlphaMaterial>
#include <Qt3DExtras/QNormalDiffuseSpecularMapMaterial>

#include <Qt3DRender/private/qmaterial_p.h>

#include "testpostmanarbiter.h"

class TestMaterial : public Qt3DRender::QMaterial
{
public:
    explicit TestMaterial(Qt3DCore::QNode *parent = 0)
        : Qt3DRender::QMaterial(parent)
        , m_effect(new Qt3DRender::QEffect(this))
        , m_technique(new Qt3DRender::QTechnique(m_effect))
        , m_renderPass(new Qt3DRender::QRenderPass(m_technique))
        , m_shaderProgram(new Qt3DRender::QShaderProgram(m_renderPass))
    {
        m_renderPass->setShaderProgram(m_shaderProgram);
        m_technique->addRenderPass(m_renderPass);
        m_effect->addTechnique(m_technique);
        setEffect(m_effect);
    }

    Qt3DRender::QEffect *m_effect;
    Qt3DRender::QTechnique *m_technique;
    Qt3DRender::QRenderPass *m_renderPass;
    Qt3DRender::QShaderProgram *m_shaderProgram;
};

class tst_QMaterial : public QObject
{
    Q_OBJECT
public:
    tst_QMaterial()
        : QObject()
    {
        qRegisterMetaType<Qt3DCore::QNode*>();
        qRegisterMetaType<Qt3DRender::QEffect*>("Qt3DRender::QEffect*");
    }

private:

    void compareEffects(const Qt3DRender::QEffect *original, const Qt3DRender::QEffect *clone)
    {
        bool isEffectNull = (original == nullptr);
        if (isEffectNull) {
            QVERIFY(!clone);
        } else {
            QVERIFY(clone);
            QCOMPARE(original->id(), clone->id());

            compareParameters(original->parameters(), clone->parameters());

            const int techniqueCounts = original->techniques().size();
            QCOMPARE(techniqueCounts, clone->techniques().size());

            for (int i = 0; i < techniqueCounts; ++i)
                compareTechniques(original->techniques().at(i), clone->techniques().at(i));
        }
    }

    void compareTechniques(const Qt3DRender::QTechnique *original, const Qt3DRender::QTechnique *clone)
    {
        QCOMPARE(original->id(), clone->id());

        compareParameters(original->parameters(), clone->parameters());

        const int passesCount = original->renderPasses().size();
        QCOMPARE(passesCount, clone->renderPasses().size());

        for (int i = 0; i < passesCount; ++i)
            compareRenderPasses(original->renderPasses().at(i), clone->renderPasses().at(i));
    }

    void compareRenderPasses(const Qt3DRender::QRenderPass *original, const Qt3DRender::QRenderPass *clone)
    {
        QCOMPARE(original->id(), clone->id());

        compareParameters(original->parameters(), clone->parameters());
        compareRenderStates(original->renderStates(), clone->renderStates());
        compareFilterKeys(original->filterKeys(), clone->filterKeys());
        compareShaderPrograms(original->shaderProgram(), clone->shaderProgram());
    }

    void compareParameters(const Qt3DRender::ParameterList &original, const Qt3DRender::ParameterList &clone)
    {
        QCOMPARE(original.size(), clone.size());
        const int parametersCount = original.size();
        for (int i = 0; i < parametersCount; ++i) {
            const Qt3DRender::QParameter *originParam = original.at(i);
            const Qt3DRender::QParameter *cloneParam = clone.at(i);
            QCOMPARE(originParam->id(), cloneParam->id());
            QCOMPARE(cloneParam->name(), originParam->name());
            QCOMPARE(cloneParam->value(), originParam->value());
        }
    }

    void compareFilterKeys(const QVector<Qt3DRender::QFilterKey *> &original, const QVector<Qt3DRender::QFilterKey *> &clone)
    {
        const int annotationsCount = original.size();
        QCOMPARE(annotationsCount, clone.size());

        for (int i = 0; i < annotationsCount; ++i) {
            const Qt3DRender::QFilterKey *origAnnotation = original.at(i);
            const Qt3DRender::QFilterKey *cloneAnnotation = clone.at(i);
            QCOMPARE(origAnnotation->id(), cloneAnnotation->id());
            QCOMPARE(origAnnotation->name(), cloneAnnotation->name());
            QCOMPARE(origAnnotation->value(), cloneAnnotation->value());
        }
    }

    void compareRenderStates(const QVector<Qt3DRender::QRenderState *> &original, const QVector<Qt3DRender::QRenderState *> &clone)
    {
        const int renderStatesCount = original.size();
        QCOMPARE(renderStatesCount, clone.size());

        for (int i = 0; i < renderStatesCount; ++i) {
            Qt3DRender::QRenderState *originState = original.at(i);
            Qt3DRender::QRenderState *cloneState = clone.at(i);
            QCOMPARE(originState->id(), originState->id());
            QVERIFY(Qt3DRender::QRenderStatePrivate::get(originState)->m_type == Qt3DRender::QRenderStatePrivate::get(cloneState)->m_type);
        }
    }

    void compareShaderPrograms(const Qt3DRender::QShaderProgram *original, const Qt3DRender::QShaderProgram *clone)
    {
        bool isOriginalNull = (original == nullptr);
        if (isOriginalNull) {
            QVERIFY(!clone);
        } else {
            QVERIFY(clone);
            QCOMPARE(original->id(), clone->id());
            QVERIFY(original->vertexShaderCode() == clone->vertexShaderCode());
            QVERIFY(original->fragmentShaderCode() == clone->fragmentShaderCode());
            QVERIFY(original->geometryShaderCode() == clone->geometryShaderCode());
            QVERIFY(original->computeShaderCode() == clone->computeShaderCode());
            QVERIFY(original->tessellationControlShaderCode() == clone->tessellationControlShaderCode());
            QVERIFY(original->tessellationEvaluationShaderCode() == clone->tessellationEvaluationShaderCode());
        }
    }

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QMaterial *>("material");

        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial();
        QTest::newRow("empty material") << material;
        material = new TestMaterial();
        QTest::newRow("test material") << material;
        material = new Qt3DExtras::QPhongMaterial();
        QTest::newRow("QPhongMaterial") << material;
        material = new Qt3DExtras::QDiffuseMapMaterial();
        QTest::newRow("QDiffuseMapMaterial") << material;
        material = new Qt3DExtras::QDiffuseSpecularMapMaterial();
        QTest::newRow("QDiffuseMapSpecularMaterial") << material;
        material = new Qt3DExtras::QPerVertexColorMaterial();
        QTest::newRow("QPerVertexColorMaterial") << material;
        material = new Qt3DExtras::QNormalDiffuseMapMaterial();
        QTest::newRow("QNormalDiffuseMapMaterial") << material;
        material = new Qt3DExtras::QNormalDiffuseMapAlphaMaterial();
        QTest::newRow("QNormalDiffuseMapAlphaMaterial") << material;
        material = new Qt3DExtras::QNormalDiffuseSpecularMapMaterial();
        QTest::newRow("QNormalDiffuseSpecularMapMaterial") << material;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QMaterial *, material);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(material);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QVERIFY(creationChanges.size() >= 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QMaterialData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QMaterialData>>(creationChanges.first());
        const Qt3DRender::QMaterialData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(material->id(), creationChangeData->subjectId());
        QCOMPARE(material->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(material->metaObject(), creationChangeData->metaObject());
        QCOMPARE(material->effect() ? material->effect()->id() : Qt3DCore::QNodeId(), cloneData.effectId);
        QCOMPARE(material->parameters().size(), cloneData.parameterIds.size());

        for (int i = 0, m = material->parameters().size(); i < m; ++i)
            QCOMPARE(material->parameters().at(i)->id(), cloneData.parameterIds.at(i));

        // TO DO: Add unit tests for parameter and effect that do check this
        //        compareParameters(material->parameters(), clone->parameters());
        //        compareEffects(material->effect(), clone->effect());
    }

    void checkEffectUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QMaterial> material(new Qt3DRender::QMaterial());
        arbiter.setArbiterOnNode(material.data());

        // WHEN
        Qt3DRender::QEffect effect;
        material->setEffect(&effect);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "effect");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), effect.id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // GIVEN
        TestArbiter arbiter2;
        QScopedPointer<TestMaterial> material2(new TestMaterial());
        arbiter2.setArbiterOnNode(material2.data());

        QCoreApplication::processEvents();
        // Clear events trigger by child generation of TestMnterial
        arbiter2.events.clear();

        // WHEN
        material2->setEffect(&effect);
        QCoreApplication::processEvents();

        // THEN
        qDebug() << Q_FUNC_INFO << arbiter2.events.size();
        QCOMPARE(arbiter2.events.size(), 1);
        change = arbiter2.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "effect");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), effect.id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
    }

    void checkDynamicParametersAddedUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        TestMaterial *material = new TestMaterial();
        arbiter.setArbiterOnNode(material);

        QCoreApplication::processEvents();
        // Clear events trigger by child generation of TestMnterial
        arbiter.events.clear();

        // WHEN (add parameter to material)
        Qt3DRender::QParameter *param = new Qt3DRender::QParameter("testParamMaterial", QVariant::fromValue(383.0f));
        material->addParameter(param);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(param->parent(), material);

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "parameter");
        QCOMPARE(change->addedNodeId(), param->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN (add parameter to effect)
        param = new Qt3DRender::QParameter("testParamEffect", QVariant::fromValue(383.0f));
        material->effect()->addParameter(param);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "parameter");
        QCOMPARE(change->addedNodeId(), param->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN (add parameter to technique)
        param = new Qt3DRender::QParameter("testParamTechnique", QVariant::fromValue(383.0f));
        material->m_technique->addParameter(param);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "parameter");
        QCOMPARE(change->addedNodeId(), param->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN (add parameter to renderpass)
        param = new Qt3DRender::QParameter("testParamRenderPass", QVariant::fromValue(383.0f));
        material->m_renderPass->addParameter(param);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "parameter");
        QCOMPARE(change->addedNodeId(), param->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();
    }

    void checkShaderProgramUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        TestMaterial *material = new TestMaterial();
        arbiter.setArbiterOnNode(material);

        QCoreApplication::processEvents();
        // Clear events trigger by child generation of TestMnterial
        arbiter.events.clear();

        // WHEN
        const QByteArray vertexCode = QByteArrayLiteral("new vertex shader code");
        material->m_shaderProgram->setVertexShaderCode(vertexCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "vertexShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), vertexCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        const QByteArray fragmentCode = QByteArrayLiteral("new fragment shader code");
        material->m_shaderProgram->setFragmentShaderCode(fragmentCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "fragmentShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), fragmentCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        const QByteArray geometryCode = QByteArrayLiteral("new geometry shader code");
        material->m_shaderProgram->setGeometryShaderCode(geometryCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "geometryShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), geometryCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        const QByteArray computeCode = QByteArrayLiteral("new compute shader code");
        material->m_shaderProgram->setComputeShaderCode(computeCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "computeShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), computeCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        const QByteArray tesselControlCode = QByteArrayLiteral("new tessellation control shader code");
        material->m_shaderProgram->setTessellationControlShaderCode(tesselControlCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "tessellationControlShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), tesselControlCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        const QByteArray tesselEvalCode = QByteArrayLiteral("new tessellation eval shader code");
        material->m_shaderProgram->setTessellationEvaluationShaderCode(tesselEvalCode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "tessellationEvaluationShaderCode");
        QCOMPARE(change->value().value<QByteArray>(), tesselEvalCode);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkEffectBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QMaterial> material(new Qt3DRender::QMaterial);
        {
            // WHEN
            Qt3DRender::QEffect effect;
            material->setEffect(&effect);

            // THEN
            QCOMPARE(effect.parent(), material.data());
            QCOMPARE(material->effect(), &effect);
        }
        // THEN (Should not crash and effect be unset)
        QVERIFY(material->effect() == nullptr);

        {
            // WHEN
            Qt3DRender::QMaterial someOtherMaterial;
            QScopedPointer<Qt3DRender::QEffect> effect(new Qt3DRender::QEffect(&someOtherMaterial));
            material->setEffect(effect.data());

            // THEN
            QCOMPARE(effect->parent(), &someOtherMaterial);
            QCOMPARE(material->effect(), effect.data());

            // WHEN
            material.reset();
            effect.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkParametersBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QMaterial> material(new Qt3DRender::QMaterial);
        {
            // WHEN
            Qt3DRender::QParameter param;
            material->addParameter(&param);

            // THEN
            QCOMPARE(param.parent(), material.data());
            QCOMPARE(material->parameters().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(material->parameters().empty());

        {
            // WHEN
            Qt3DRender::QMaterial someOtherMaterial;
            QScopedPointer<Qt3DRender::QParameter> param(new Qt3DRender::QParameter(&someOtherMaterial));
            material->addParameter(param.data());

            // THEN
            QCOMPARE(param->parent(), &someOtherMaterial);
            QCOMPARE(material->parameters().size(), 1);

            // WHEN
            material.reset();
            param.reset();

            // THEN Should not crash when the parameter is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QMaterial)

#include "tst_qmaterial.moc"
