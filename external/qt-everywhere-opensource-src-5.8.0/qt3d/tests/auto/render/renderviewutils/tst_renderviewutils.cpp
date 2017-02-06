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
#include <qbackendnodetester.h>
#include <Qt3DCore/qdynamicpropertyupdatedchange.h>
#include <Qt3DRender/private/renderviewjobutils_p.h>
#include <Qt3DRender/private/shaderdata_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/stringtoint_p.h>
#include <Qt3DRender/qshaderdata.h>

#include "testpostmanarbiter.h"

class tst_RenderViewUtils : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:
    void topLevelScalarValueNoUniforms();
    void topLevelScalarValue();
    void topLevelArrayValue();
    void topLevelStructValue_data();
    void topLevelStructValue();
    void topLevelDynamicProperties();
    void transformedProperties();
    void shouldNotifyDynamicPropertyChanges();

private:
    void initBackendShaderData(Qt3DRender::QShaderData *frontend,
                               Qt3DRender::Render::ShaderDataManager *manager)
    {
        // Create children first
        Q_FOREACH (QObject *c, frontend->children()) {
            Qt3DRender::QShaderData *cShaderData = qobject_cast<Qt3DRender::QShaderData *>(c);
            if (cShaderData)
                initBackendShaderData(cShaderData, manager);
        }

        // Create backend element for frontend one
        Qt3DRender::Render::ShaderData *backend = manager->getOrCreateResource(frontend->id());
        // Init the backend element
        simulateInitialization(frontend, backend);
    }
};

class ScalarShaderData : public Qt3DRender::QShaderData
{
    Q_OBJECT
    Q_PROPERTY(float scalar READ scalar WRITE setScalar NOTIFY scalarChanged)

public:
    ScalarShaderData()
        : Qt3DRender::QShaderData()
        , m_scalar(0.0f)
    {
    }

    void setScalar(float scalar)
    {
        if (scalar != m_scalar) {
            m_scalar = scalar;
            emit scalarChanged();
        }
    }

    float scalar() const
    {
        return m_scalar;
    }

    QHash<QString, Qt3DRender::Render::ShaderUniform> buildUniformMap(const QString &blockName)
    {
        QHash<QString, Qt3DRender::Render::ShaderUniform> uniforms;

        uniforms.insert(blockName + QStringLiteral(".scalar"), Qt3DRender::Render::ShaderUniform());

        return uniforms;
    }

Q_SIGNALS:
    void scalarChanged();

private:
    float m_scalar;
};


class ArrayShaderData : public Qt3DRender::QShaderData
{
    Q_OBJECT
    Q_PROPERTY(QVariantList array READ array WRITE setArray NOTIFY arrayChanged)

public:
    ArrayShaderData()
        : Qt3DRender::QShaderData()
    {
    }

    void setArray(const QVariantList &array)
    {
        if (array != m_array) {
            m_array = array;
            emit arrayChanged();
        }
    }

    QVariantList array() const
    {
        return m_array;
    }

    QHash<QString, Qt3DRender::Render::ShaderUniform> buildUniformMap(const QString &blockName)
    {
        QHash<QString, Qt3DRender::Render::ShaderUniform> uniforms;

        uniforms.insert(blockName + QStringLiteral(".array[0]"), Qt3DRender::Render::ShaderUniform());

        return uniforms;
    }

Q_SIGNALS:
    void arrayChanged();

private:
    QVariantList m_array;
};

class StructShaderData : public Qt3DRender::QShaderData
{
    Q_OBJECT
    Q_PROPERTY(float scalar READ scalar WRITE setScalar NOTIFY scalarChanged)
    Q_PROPERTY(QVariantList array READ array WRITE setArray NOTIFY arrayChanged)

public:
    StructShaderData()
        : Qt3DRender::QShaderData()
        , m_scalar(0.0f)
    {
    }

    void setScalar(float scalar)
    {
        if (scalar != m_scalar) {
            m_scalar = scalar;
            emit scalarChanged();
        }
    }

    float scalar() const
    {
        return m_scalar;
    }

    void setArray(const QVariantList &array)
    {
        if (array != m_array) {
            m_array = array;
            emit arrayChanged();
        }
    }

    QVariantList array() const
    {
        return m_array;
    }

    virtual QHash<QString, Qt3DRender::Render::ShaderUniform> buildUniformMap(const QString &blockName)
    {
        QHash<QString, Qt3DRender::Render::ShaderUniform> uniforms;

        uniforms.insert(blockName + QStringLiteral(".scalar"), Qt3DRender::Render::ShaderUniform());
        uniforms.insert(blockName + QStringLiteral(".array[0]"), Qt3DRender::Render::ShaderUniform());

        return uniforms;
    }

    virtual QHash<QString, QVariant> buildUniformMapValues(const QString &blockName)
    {
        QHash<QString, QVariant> uniforms;

        uniforms.insert(blockName + QStringLiteral(".scalar"), QVariant(scalar()));
        uniforms.insert(blockName + QStringLiteral(".array[0]"), QVariant(array()));

        return uniforms;
    }

Q_SIGNALS:
    void scalarChanged();
    void arrayChanged();

private:
    float m_scalar;
    QVariantList m_array;
};

class MultiLevelStructShaderData : public StructShaderData
{
    Q_OBJECT
    Q_PROPERTY(Qt3DRender::QShaderData *inner READ inner WRITE setInner NOTIFY innerChanged)

public:
    MultiLevelStructShaderData()
        : StructShaderData()
        , m_inner(nullptr)
    {
    }

    void setInner(Qt3DRender::QShaderData *inner)
    {
        if (inner != m_inner) {
            m_inner = inner;
            emit innerChanged();
        }
    }

    Qt3DRender::QShaderData *inner() const
    {
        return m_inner;
    }

    QHash<QString, Qt3DRender::Render::ShaderUniform> buildUniformMap(const QString &blockName) Q_DECL_OVERRIDE
    {
        QHash<QString, Qt3DRender::Render::ShaderUniform> innerUniforms;

        StructShaderData *innerData = nullptr;
        if ((innerData = qobject_cast<StructShaderData *>(m_inner)) != nullptr)
            innerUniforms = innerData->buildUniformMap(QStringLiteral(".inner"));

        QHash<QString, Qt3DRender::Render::ShaderUniform> uniforms = StructShaderData::buildUniformMap(blockName);
        QHash<QString, Qt3DRender::Render::ShaderUniform>::const_iterator it = innerUniforms.begin();
        const QHash<QString, Qt3DRender::Render::ShaderUniform>::const_iterator end = innerUniforms.end();

        while (it != end) {
            uniforms.insert(blockName + it.key(), it.value());
            ++it;
        }
        return uniforms;
    }

    QHash<QString, QVariant> buildUniformMapValues(const QString &blockName) Q_DECL_OVERRIDE
    {
        QHash<QString, QVariant> innerUniformsValues;

        StructShaderData *innerData = nullptr;
        if ((innerData = qobject_cast<StructShaderData *>(m_inner)) != nullptr)
            innerUniformsValues = innerData->buildUniformMapValues(QStringLiteral(".inner"));

        QHash<QString, QVariant> uniformsValues = StructShaderData::buildUniformMapValues(blockName);
        QHash<QString, QVariant>::const_iterator it = innerUniformsValues.begin();
        const QHash<QString, QVariant>::const_iterator end = innerUniformsValues.end();

        while (it != end) {
            uniformsValues.insert(blockName + it.key(), it.value());
            ++it;
        }

        return uniformsValues;
    }

Q_SIGNALS:
    void innerChanged();

private:
    Qt3DRender::QShaderData *m_inner;
};

void tst_RenderViewUtils::topLevelScalarValueNoUniforms()
{
    // GIVEN
    QScopedPointer<ScalarShaderData> shaderData(new ScalarShaderData());
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    shaderData->setScalar(883.0f);
    initBackendShaderData(shaderData.data(), manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);

    // WHEB
    Qt3DRender::Render::UniformBlockValueBuilder blockBuilder;
    blockBuilder.shaderDataManager = manager.data();
    blockBuilder.updatedPropertiesOnly = false;
    // build name-value map
    blockBuilder.buildActiveUniformNameValueMapStructHelper(backendShaderData, QStringLiteral(""));

    // THEN
    // activeUniformNamesToValue should be empty as blockBuilder.uniforms is
    QVERIFY(blockBuilder.activeUniformNamesToValue.isEmpty());
}

void tst_RenderViewUtils::topLevelScalarValue()
{
    // GIVEN
    QScopedPointer<ScalarShaderData> shaderData(new ScalarShaderData());
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    shaderData->setScalar(883.0f);
    initBackendShaderData(shaderData.data(), manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilder blockBuilder;
    blockBuilder.shaderDataManager = manager.data();
    blockBuilder.updatedPropertiesOnly = false;
    blockBuilder.uniforms = shaderData->buildUniformMap(QStringLiteral("MyBlock"));
    // build name-value map
    blockBuilder.buildActiveUniformNameValueMapStructHelper(backendShaderData, QStringLiteral("MyBlock"));

    // THEN
    QVERIFY(blockBuilder.uniforms.count() == 1);
    QCOMPARE(blockBuilder.activeUniformNamesToValue.count(), 1);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator it = blockBuilder.activeUniformNamesToValue.begin();
    const Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator end = blockBuilder.activeUniformNamesToValue.end();

    while (it != end) {
        // THEN
        QVERIFY(blockBuilder.uniforms.contains(Qt3DRender::Render::StringToInt::lookupString(it.key())));
        QCOMPARE(it.value(), QVariant(shaderData->scalar()));
        ++it;
    }
}

void tst_RenderViewUtils::topLevelArrayValue()
{
    // GIVEN
    QScopedPointer<ArrayShaderData> shaderData(new ArrayShaderData());
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    QVariantList arrayValues = QVariantList() << 454 << 350 << 383 << 427 << 552;
    shaderData->setArray(arrayValues);
    initBackendShaderData(shaderData.data(), manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilder blockBuilder;
    blockBuilder.shaderDataManager = manager.data();
    blockBuilder.updatedPropertiesOnly = false;
    blockBuilder.uniforms = shaderData->buildUniformMap(QStringLiteral("MyBlock"));
    // build name-value map
    blockBuilder.buildActiveUniformNameValueMapStructHelper(backendShaderData, QStringLiteral("MyBlock"));

    // THEN
    QVERIFY(blockBuilder.uniforms.count() == 1);
    QCOMPARE(blockBuilder.activeUniformNamesToValue.count(), 1);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator it = blockBuilder.activeUniformNamesToValue.begin();
    const Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator end = blockBuilder.activeUniformNamesToValue.end();

    while (it != end) {
        // THEN
        QVERIFY(blockBuilder.uniforms.contains(Qt3DRender::Render::StringToInt::lookupString(it.key())));
        QCOMPARE(it.value(), QVariant(arrayValues));
        ++it;
    }
}

void tst_RenderViewUtils::topLevelStructValue_data()
{
    QTest::addColumn<StructShaderData*>("shaderData");
    QTest::addColumn<QString>("blockName");

    QVariantList arrayValues2 = QVariantList() << 180 << 220 << 250 << 270 << 300 << 350 << 550;
    QVariantList arrayValues = QVariantList() << 454 << 350 << 383 << 427 << 552;

    MultiLevelStructShaderData *twoLevelsNestedShaderData = new MultiLevelStructShaderData();
    MultiLevelStructShaderData *singleLevelShaderData = new MultiLevelStructShaderData();
    StructShaderData *shaderData = new StructShaderData();

    // Don't forget to set the parent so that initBackendShaderData
    // properly initializes nested members
    shaderData->setParent(singleLevelShaderData);
    shaderData->setArray(arrayValues);
    shaderData->setScalar(1584.0f);

    singleLevelShaderData->setParent(twoLevelsNestedShaderData);
    singleLevelShaderData->setInner(shaderData);
    singleLevelShaderData->setScalar(1200.0f);
    singleLevelShaderData->setArray(arrayValues2);

    twoLevelsNestedShaderData->setInner(singleLevelShaderData);
    twoLevelsNestedShaderData->setArray(arrayValues + arrayValues2);
    twoLevelsNestedShaderData->setScalar(1340.0f);

    QTest::newRow("simple struct") << shaderData << QStringLiteral("Block");
    QTest::newRow("single level inner struct") << (StructShaderData *)singleLevelShaderData << QStringLiteral("Block");
    QTest::newRow("tow level inner struct") << (StructShaderData *)twoLevelsNestedShaderData << QStringLiteral("Block");
}

void tst_RenderViewUtils::topLevelStructValue()
{
    // GIVEN
    QFETCH(StructShaderData *, shaderData);
    QFETCH(QString, blockName);
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    initBackendShaderData(shaderData, manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilder blockBuilder;
    blockBuilder.shaderDataManager = manager.data();
    blockBuilder.updatedPropertiesOnly = false;
    blockBuilder.uniforms = shaderData->buildUniformMap(blockName);
    const QHash<QString, QVariant> expectedValues = shaderData->buildUniformMapValues(blockName);
    // build name-value map
    blockBuilder.buildActiveUniformNameValueMapStructHelper(backendShaderData, blockName);

    // THEN
    QCOMPARE(blockBuilder.activeUniformNamesToValue.count(), blockBuilder.uniforms.count());

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator it = blockBuilder.activeUniformNamesToValue.begin();
    const Qt3DRender::Render::UniformBlockValueBuilderHash::const_iterator end = blockBuilder.activeUniformNamesToValue.end();

    while (it != end) {
        // THEN
        QVERIFY(blockBuilder.uniforms.contains(Qt3DRender::Render::StringToInt::lookupString(it.key())));
        QVERIFY(expectedValues.contains(Qt3DRender::Render::StringToInt::lookupString(it.key())));
        QCOMPARE(it.value(), expectedValues.value(Qt3DRender::Render::StringToInt::lookupString(it.key())));
        ++it;
    }
}

void tst_RenderViewUtils::topLevelDynamicProperties()
{
    // GIVEN
    QScopedPointer<Qt3DRender::QShaderData> shaderData(new Qt3DRender::QShaderData());
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    shaderData->setProperty("scalar", 883.0f);
    shaderData->setProperty("array", QVariantList() << 454 << 350 << 383 << 427 << 552);
    initBackendShaderData(shaderData.data(), manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);

    // WHEN
    Qt3DRender::Render::UniformBlockValueBuilder blockBuilder;
    blockBuilder.shaderDataManager = manager.data();
    blockBuilder.updatedPropertiesOnly = false;
    blockBuilder.uniforms.insert(QStringLiteral("MyBlock.scalar"), Qt3DRender::Render::ShaderUniform());
    blockBuilder.uniforms.insert(QStringLiteral("MyBlock.array[0]"), Qt3DRender::Render::ShaderUniform());
    // build name-value map
    blockBuilder.buildActiveUniformNameValueMapStructHelper(backendShaderData, QStringLiteral("MyBlock"));

    // THEN
    QVERIFY(blockBuilder.uniforms.count() == 2);
    QCOMPARE(blockBuilder.activeUniformNamesToValue.count(), 2);

    QCOMPARE(blockBuilder.activeUniformNamesToValue.value(Qt3DRender::Render::StringToInt::lookupId("MyBlock.scalar")),
             shaderData->property("scalar"));
    QCOMPARE(blockBuilder.activeUniformNamesToValue.value(Qt3DRender::Render::StringToInt::lookupId("MyBlock.array[0]")),
             shaderData->property("array"));
}

void tst_RenderViewUtils::transformedProperties()
{
    // GIVEN
    QScopedPointer<Qt3DRender::QShaderData> shaderData(new Qt3DRender::QShaderData());
    QScopedPointer<Qt3DRender::Render::ShaderDataManager> manager(new Qt3DRender::Render::ShaderDataManager());

    // WHEN
    const QVector3D position = QVector3D(15.0f, -5.0f, 10.0f);
    QMatrix4x4 worldMatrix;
    QMatrix4x4 viewMatrix;

    worldMatrix.translate(-3.0f, 2.0f, 7.5f);
    viewMatrix.translate(9.0f, 6.0f, 12.0f);

    shaderData->setProperty("position0", position);
    shaderData->setProperty("position1", position);
    shaderData->setProperty("position2", position);
    shaderData->setProperty("position3", position);
    shaderData->setProperty("position1Transformed", Qt3DRender::Render::ShaderData::ModelToEye);
    shaderData->setProperty("position2Transformed", Qt3DRender::Render::ShaderData::ModelToWorld);
    shaderData->setProperty("position3Transformed", Qt3DRender::Render::ShaderData::ModelToWorldDirection);
    initBackendShaderData(shaderData.data(), manager.data());

    // THEN
    Qt3DRender::Render::ShaderData *backendShaderData = manager->lookupResource(shaderData->id());
    QVERIFY(backendShaderData != nullptr);
    QCOMPARE(backendShaderData->propertyTransformType(QStringLiteral("position0")), Qt3DRender::Render::ShaderData::NoTransform);
    QCOMPARE(backendShaderData->propertyTransformType(QStringLiteral("position1")), Qt3DRender::Render::ShaderData::ModelToEye);
    QCOMPARE(backendShaderData->propertyTransformType(QStringLiteral("position2")), Qt3DRender::Render::ShaderData::ModelToWorld);
    QCOMPARE(backendShaderData->propertyTransformType(QStringLiteral("position3")), Qt3DRender::Render::ShaderData::ModelToWorldDirection);

    // WHEN
    backendShaderData->updateWorldTransform(worldMatrix);
    const QVector3D position1Value = backendShaderData->getTransformedProperty(QStringLiteral("position1"), viewMatrix).value<QVector3D>();
    const QVector3D position2Value = backendShaderData->getTransformedProperty(QStringLiteral("position2"), viewMatrix).value<QVector3D>();
    const QVector3D position3Value = backendShaderData->getTransformedProperty(QStringLiteral("position3"), viewMatrix).value<QVector3D>();
    const QVariant position0Value = backendShaderData->getTransformedProperty(QStringLiteral("position0"), viewMatrix);

    // THEN
    QCOMPARE(position0Value, QVariant());
    QCOMPARE(position1Value, viewMatrix * worldMatrix * position);
    QCOMPARE(position2Value, worldMatrix * position);
    QCOMPARE(position3Value, (worldMatrix * QVector4D(position, 0.0f)).toVector3D());
}

void tst_RenderViewUtils::shouldNotifyDynamicPropertyChanges()
{
    // GIVEN
    TestArbiter arbiter;
    QScopedPointer<Qt3DRender::QShaderData> shaderData(new Qt3DRender::QShaderData());
    arbiter.setArbiterOnNode(shaderData.data());

    // WHEN
    shaderData->setProperty("scalar", 883.0f);

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    auto change = arbiter.events.first().dynamicCast<Qt3DCore::QDynamicPropertyUpdatedChange>();
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
    QCOMPARE(change->propertyName(), QByteArrayLiteral("scalar"));
    QCOMPARE(change->value().toFloat(), 883.0f);
}

QTEST_MAIN(tst_RenderViewUtils)

#include "tst_renderviewutils.moc"
