/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/qtest.h>
#include <QtCore/qtemporarydir.h>
#include <QtGui/qimage.h>

#include <private/qsceneimporter_p.h>
#include <private/qsceneexportfactory_p.h>
#include <private/qsceneexporter_p.h>
#include <private/qsceneimportfactory_p.h>
#include <private/qsceneimporter_p.h>

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>

#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qtextureimage.h>
#include <Qt3DRender/qspotlight.h>
#include <Qt3DRender/qdirectionallight.h>
#include <Qt3DRender/qpointlight.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qcolormask.h>
#include <Qt3DRender/qblendequation.h>

#include <Qt3DExtras/qconemesh.h>
#include <Qt3DExtras/qcuboidmesh.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/qplanemesh.h>
#include <Qt3DExtras/qspheremesh.h>
#include <Qt3DExtras/qtorusmesh.h>
#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <Qt3DExtras/qphongalphamaterial.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qnormaldiffusemapmaterial.h>
#include <Qt3DExtras/qnormaldiffusemapalphamaterial.h>
#include <Qt3DExtras/qnormaldiffusespecularmapmaterial.h>
#include <Qt3DExtras/qgoochmaterial.h>
#include <Qt3DExtras/qpervertexcolormaterial.h>
#include <Qt3DExtras/qforwardrenderer.h>

//#define VISUAL_CHECK 5000  // The value indicates the time for visual check in ms
//#define PRESERVE_EXPORT  // Uncomment to preserve export directory contents for analysis

class tst_gltfPlugins : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase();
    void init();
    void cleanup();
    void exportAndImport_data();
    void exportAndImport();

private:
    void createTestScene();
    Qt3DCore::QEntity *findCameraChild(Qt3DCore::QEntity *entity,
                                       Qt3DRender::QCameraLens::ProjectionType type);
    void walkEntity(Qt3DCore::QEntity *entity, int depth);
    void createAndAddEntity(const QString &name,
                            Qt3DCore::QComponent *comp1 = nullptr,
                            Qt3DCore::QComponent *comp2 = nullptr,
                            Qt3DCore::QComponent *comp3 = nullptr,
                            Qt3DCore::QEntity *parent = nullptr);
    void addPositionAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                        Qt3DRender::QBuffer *buffer, int count);
    void addIndexAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                     Qt3DRender::QBuffer *buffer, int count);
    void addColorAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                     Qt3DRender::QBuffer *buffer, int count);
    Qt3DCore::QEntity *findChildEntity(Qt3DCore::QEntity *entity, const QString &name);
    Qt3DCore::QTransform *transformComponent(Qt3DCore::QEntity *entity);
    Qt3DRender::QAbstractLight *lightComponent(Qt3DCore::QEntity *entity);
    Qt3DRender::QCameraLens *cameraComponent(Qt3DCore::QEntity *entity);
    Qt3DRender::QGeometryRenderer *meshComponent(Qt3DCore::QEntity *entity);
    Qt3DRender::QMaterial *materialComponent(Qt3DCore::QEntity *entity);
    void compareComponents(Qt3DCore::QComponent *c1, Qt3DCore::QComponent *c2);
    Qt3DRender::QAttribute *findAttribute(const QString &name,
                                          Qt3DRender::QAttribute::AttributeType type,
                                          Qt3DRender::QGeometry *geometry);
    void compareAttributes(Qt3DRender::QAttribute *a1, Qt3DRender::QAttribute *a2);
    void compareParameters(const QVector<Qt3DRender::QParameter *> &params1,
                           const QVector<Qt3DRender::QParameter *> &params2);
    void compareRenderPasses(const QVector<Qt3DRender::QRenderPass *> &passes1,
                             const QVector<Qt3DRender::QRenderPass *> &passes2);
    void compareFilterKeys(const QVector<Qt3DRender::QFilterKey *> &keys1,
                           const QVector<Qt3DRender::QFilterKey *> &keys2);
    QUrl getTextureUrl(Qt3DRender::QAbstractTexture *tex);
    Qt3DRender::QGeometryRenderer *createCustomCube();
    Qt3DRender::QEffect *createOnTopEffect();

    QTemporaryDir *m_exportDir;
#ifdef VISUAL_CHECK
    Qt3DExtras::Qt3DWindow *m_view1;
    Qt3DExtras::Qt3DWindow *m_view2;
#endif
    Qt3DCore::QEntity *m_sceneRoot1;
    Qt3DCore::QEntity *m_sceneRoot2;
    QHash<QString, Qt3DCore::QEntity *> m_entityMap;
};

void tst_gltfPlugins::initTestCase()
{
#ifndef VISUAL_CHECK
    // QEffect doesn't get registered unless aspects are initialized, generating warnings in
    // material comparisons.
    qRegisterMetaType<Qt3DRender::QEffect *>();
#endif
}

void tst_gltfPlugins::init()
{
    m_exportDir = new QTemporaryDir;
#ifdef VISUAL_CHECK
    m_view1 = new Qt3DExtras::Qt3DWindow;
    m_view1->setTitle(QStringLiteral("Original scene"));
    m_view2 = new Qt3DExtras::Qt3DWindow;
    m_view2->setTitle(QStringLiteral("Imported scene"));
#endif
}

void tst_gltfPlugins::cleanup()
{
    delete m_sceneRoot1;
    delete m_sceneRoot2;
    m_entityMap.clear();
#ifdef VISUAL_CHECK
    delete m_view1;
    delete m_view2;
#endif
    delete m_exportDir;
    // Make sure the slate is clean for the next case
    QCoreApplication::processEvents();
    QTest::qWait(0);
}

void tst_gltfPlugins::walkEntity(Qt3DCore::QEntity *entity, int depth)
{
    QString indent;
    indent.fill(' ', depth * 2);
    qDebug().noquote() << indent << "Entity:" << entity << "Components:" << entity->components();
    for (auto child : entity->children()) {
        if (auto childEntity = qobject_cast<Qt3DCore::QEntity *>(child))
            walkEntity(childEntity, depth + 1);
    }
}

void tst_gltfPlugins::createAndAddEntity(const QString &name,
                                         Qt3DCore::QComponent *comp1,
                                         Qt3DCore::QComponent *comp2,
                                         Qt3DCore::QComponent *comp3,
                                         Qt3DCore::QEntity *parent)
{
    Qt3DCore::QEntity *parentEntity = parent ? parent : m_sceneRoot1;
    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parentEntity);
    entity->setObjectName(name);
    if (comp1) {
        entity->addComponent(comp1);
        comp1->setObjectName(comp1->metaObject()->className());
    }
    if (comp2) {
        entity->addComponent(comp2);
        comp2->setObjectName(comp2->metaObject()->className());
    }
    if (comp3) {
        entity->addComponent(comp3);
        comp3->setObjectName(comp3->metaObject()->className());
    }

    m_entityMap.insert(name, entity);
}

void tst_gltfPlugins::createTestScene()
{
#ifdef VISUAL_CHECK
    m_view1->defaultFrameGraph()->setClearColor(Qt::lightGray);
    m_view2->defaultFrameGraph()->setClearColor(Qt::lightGray);
#endif
    m_sceneRoot1 = new Qt3DCore::QEntity();
    m_sceneRoot1->setObjectName(QStringLiteral("Scene root"));
    m_sceneRoot2 = new Qt3DCore::QEntity();
    m_sceneRoot2->setObjectName(QStringLiteral("Imported scene parent"));

    // Perspective camera
    {
        Qt3DRender::QCamera *camera = new Qt3DRender::QCamera(m_sceneRoot1);
        camera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
        camera->setViewCenter(QVector3D(0.0f, 1.5f, 0.0f));
        camera->setPosition(QVector3D(0.0f, 3.5f, 15.0f));
        camera->setNearPlane(0.001f);
        camera->setFarPlane(10000.0f);
        camera->setObjectName(QStringLiteral("Main camera"));
        camera->transform()->setObjectName(QStringLiteral("Main camera transform"));
        camera->lens()->setObjectName(QStringLiteral("Main camera lens"));
        camera->setFieldOfView(30.0f);
        camera->setAspectRatio(1.0f);
        m_entityMap.insert(camera->objectName(), camera);
    }
    // Ortho camera
    {
        Qt3DCore::QEntity *camera = new Qt3DCore::QEntity(m_sceneRoot1);
        camera->setObjectName(QStringLiteral("Ortho camera"));

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 0.0f, -15.0f));
        transform->setObjectName(QStringLiteral("Ortho camera transform"));
        camera->addComponent(transform);

        Qt3DRender::QCameraLens *lens = new Qt3DRender::QCameraLens;
        lens->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
        lens->setNearPlane(0.001f);
        lens->setFarPlane(10000.0f);
        lens->setRight(7.0f);
        lens->setLeft(-7.0f);
        lens->setTop(5.0f);
        lens->setBottom(-5.0f);
        lens->setObjectName(QStringLiteral("Ortho camera lens"));
        camera->addComponent(lens);

        m_entityMap.insert(camera->objectName(), camera);
#ifdef VISUAL_CHECK
        m_view1->defaultFrameGraph()->setCamera(camera);
#endif
    }

    // Point light
    {
        Qt3DRender::QPointLight *light = new Qt3DRender::QPointLight;
        light->setColor(QColor("#FFDDAA"));
        light->setIntensity(0.9f);
        light->setConstantAttenuation(0.03f);
        light->setLinearAttenuation(0.04f);
        light->setQuadraticAttenuation(0.01f);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(-6.0f, 2.0f, 3.0f));
        createAndAddEntity(QStringLiteral("Point Light"), light, transform);
    }
    // Directional light
    {
        Qt3DRender::QDirectionalLight *light = new Qt3DRender::QDirectionalLight;
        light->setColor(QColor("#BBCCEE"));
        light->setIntensity(0.75f);
        light->setWorldDirection(QVector3D(-1.0f, -1.0f, -1.0f));
        createAndAddEntity(QStringLiteral("Directional Light"), light);
    }
    // Spot light
    {
        Qt3DRender::QSpotLight *light = new Qt3DRender::QSpotLight;
        light->setColor(QColor("#5599DD"));
        light->setIntensity(2.0f);
        light->setConstantAttenuation(0.03f);
        light->setLinearAttenuation(0.04f);
        light->setQuadraticAttenuation(0.01f);
        light->setLocalDirection(QVector3D(0.0f, -1.0f, -1.0f));
        light->setCutOffAngle(30.0f);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 5.0f, 0.0f));
        createAndAddEntity(QStringLiteral("Spot Light"), light, transform);
    }
    // Cube with DiffuseMap
    {
        Qt3DExtras::QDiffuseMapMaterial *material = new Qt3DExtras::QDiffuseMapMaterial();
        Qt3DRender::QTextureImage *diffuseTextureImage = new Qt3DRender::QTextureImage();
        material->diffuse()->addTextureImage(diffuseTextureImage);
        material->setAmbient(QColor("#000088"));
        material->setSpecular(QColor("#FFFF00"));
        material->setShininess(30.0);
        material->setTextureScale(2.0f);
        diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo.png")));
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setScale(0.75f);
        transform->setTranslation(QVector3D(2.0f, 1.0f, -1.0f));
        Qt3DExtras::QCuboidMesh *mesh = new Qt3DExtras::QCuboidMesh;
        mesh->setXExtent(1.2f);
        mesh->setYExtent(1.1f);
        mesh->setZExtent(0.9f);
        mesh->setYZMeshResolution(QSize(2, 2));
        mesh->setYZMeshResolution(QSize(2, 3));
        mesh->setYZMeshResolution(QSize(3, 2));
        createAndAddEntity(QStringLiteral("Cube with DiffuseMap"), mesh, material, transform);
    }
    // Cone with PhongAlpha
    {
        Qt3DExtras::QPhongAlphaMaterial *material = new Qt3DExtras::QPhongAlphaMaterial();
        material->setAlpha(0.6f);
        material->setAmbient(QColor("#550000"));
        material->setDiffuse(QColor("#00FFFF"));
        material->setSpecular(QColor("#FFFF00"));
        material->setShininess(20.0f);
        material->setSourceRgbArg(Qt3DRender::QBlendEquationArguments::Source1Color);
        material->setSourceAlphaArg(Qt3DRender::QBlendEquationArguments::Source1Alpha);
        material->setDestinationRgbArg(Qt3DRender::QBlendEquationArguments::DestinationColor);
        material->setDestinationAlphaArg(Qt3DRender::QBlendEquationArguments::DestinationAlpha);
        material->setBlendFunctionArg(Qt3DRender::QBlendEquation::ReverseSubtract);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(2.0f, 1.0f, 1.0f));
        transform->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -20.0f));
        Qt3DExtras::QConeMesh *mesh = new Qt3DExtras::QConeMesh;
        mesh->setRings(2);
        mesh->setSlices(16);
        mesh->setTopRadius(0.5f);
        mesh->setBottomRadius(1.5f);
        mesh->setLength(0.9f);
        createAndAddEntity(QStringLiteral("Cone with PhongAlpha"), mesh, material, transform);
    }
    // Cylinder with Phong
    {
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        material->setAmbient(QColor("#220022"));
        material->setDiffuse(QColor("#6633AA"));
        material->setSpecular(QColor("#66AA33"));
        material->setShininess(50.0f);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 1.0f, 1.0f));
        transform->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -45.0f));
        Qt3DExtras::QCylinderMesh *mesh = new Qt3DExtras::QCylinderMesh;
        mesh->setRadius(0.5f);
        mesh->setRings(3);
        mesh->setLength(1.2f);
        mesh->setSlices(16);
        createAndAddEntity(QStringLiteral("Cylinder with Phong"), mesh, material, transform);
    }
    // Plane with DiffuseSpecularMap
    {
        Qt3DExtras::QDiffuseSpecularMapMaterial *material =
                new Qt3DExtras::QDiffuseSpecularMapMaterial();
        Qt3DRender::QTextureImage *diffuseTextureImage = new Qt3DRender::QTextureImage();
        material->diffuse()->addTextureImage(diffuseTextureImage);
        diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo.png")));
        Qt3DRender::QTextureImage *specularTextureImage = new Qt3DRender::QTextureImage();
        material->specular()->addTextureImage(specularTextureImage);
        specularTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_specular.png")));

        material->setAmbient(QColor("#0000FF"));
        material->setTextureScale(3.0f);
        material->setShininess(15.0f);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(-1.0f, 1.0f, 1.0f));
        transform->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 45.0f));

        Qt3DExtras::QPlaneMesh *mesh = new Qt3DExtras::QPlaneMesh;
        mesh->setMeshResolution(QSize(3, 3));
        mesh->setHeight(1.5f);
        mesh->setWidth(1.2f);
        createAndAddEntity(QStringLiteral("Plane with DiffuseSpecularMap"),
                           mesh, material, transform);
    }
    // Sphere with NormalDiffuseMap
    {
        Qt3DExtras::QNormalDiffuseMapMaterial *material =
                new Qt3DExtras::QNormalDiffuseMapMaterial();

        Qt3DRender::QTextureImage *normalTextureImage = new Qt3DRender::QTextureImage();
        material->normal()->addTextureImage(normalTextureImage);
        normalTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_normal.png")));

        Qt3DRender::QTextureImage *diffuseTextureImage = new Qt3DRender::QTextureImage();
        material->diffuse()->addTextureImage(diffuseTextureImage);
        diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo.png")));

        material->setAmbient(QColor("#000044"));
        material->setSpecular(QColor("#0000CC"));
        material->setShininess(9.0f);
        material->setTextureScale(4.0f);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 1.0f, -10.0f));

        Qt3DExtras::QSphereMesh *mesh = new Qt3DExtras::QSphereMesh;
        mesh->setRadius(2.0f);
        mesh->setRings(16);
        mesh->setSlices(16);
        mesh->setGenerateTangents(true);
        createAndAddEntity(QStringLiteral("Sphere with NormalDiffuseMap"),
                           mesh, material, transform);
    }
    // Sphere with NormalDiffuseMapAlpha
    {
        Qt3DExtras::QNormalDiffuseMapAlphaMaterial *material =
                new Qt3DExtras::QNormalDiffuseMapAlphaMaterial();

        Qt3DRender::QTextureImage *normalTextureImage = new Qt3DRender::QTextureImage();
        material->normal()->addTextureImage(normalTextureImage);
        normalTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_normal.png")));

        Qt3DRender::QTextureImage *diffuseTextureImage = new Qt3DRender::QTextureImage();
        material->diffuse()->addTextureImage(diffuseTextureImage);
        diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_with_alpha.png")));

        material->setAmbient(QColor("#000044"));
        material->setSpecular(QColor("#0000CC"));
        material->setShininess(9.0f);
        material->setTextureScale(4.0f);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(4.0f, 1.0f, -10.0f));

        Qt3DExtras::QSphereMesh *mesh = new Qt3DExtras::QSphereMesh;
        mesh->setRadius(2.0f);
        mesh->setRings(16);
        mesh->setSlices(16);
        mesh->setGenerateTangents(true);
        createAndAddEntity(QStringLiteral("Sphere with NormalDiffuseMapAlpha"),
                           mesh, material, transform);
    }
    // Sphere with NormalDiffuseSpecularMap
    {
        Qt3DExtras::QNormalDiffuseSpecularMapMaterial *material =
                new Qt3DExtras::QNormalDiffuseSpecularMapMaterial();

        Qt3DRender::QTextureImage *normalTextureImage = new Qt3DRender::QTextureImage();
        material->normal()->addTextureImage(normalTextureImage);
        normalTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_normal.png")));

        Qt3DRender::QTextureImage *diffuseTextureImage = new Qt3DRender::QTextureImage();
        material->diffuse()->addTextureImage(diffuseTextureImage);
        diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo.png")));

        Qt3DRender::QTextureImage *specularTextureImage = new Qt3DRender::QTextureImage();
        material->specular()->addTextureImage(specularTextureImage);
        specularTextureImage->setSource(QUrl(QStringLiteral("qrc:/qtlogo_specular.png")));

        material->setAmbient(QColor("#000044"));
        material->setShininess(9.0f);
        material->setTextureScale(4.0f);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(-4.0f, 1.0f, -10.0f));

        Qt3DExtras::QSphereMesh *mesh = new Qt3DExtras::QSphereMesh;
        mesh->setRadius(2.0f);
        mesh->setRings(16);
        mesh->setSlices(16);
        mesh->setGenerateTangents(true);
        createAndAddEntity(QStringLiteral("Sphere with NormalDiffuseSpecularMap"),
                           mesh, material, transform);
    }
    // Torus with Gooch
    {
        Qt3DExtras::QGoochMaterial *material = new Qt3DExtras::QGoochMaterial();

        material->setDiffuse(QColor("#333333"));
        material->setSpecular(QColor("#550055"));
        material->setCool(QColor("#0055AA"));
        material->setWarm(QColor("#FF3300"));
        material->setAlpha(0.2f);
        material->setBeta(0.4f);
        material->setShininess(22.0f);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 4.0f, -10.0f));

        Qt3DExtras::QTorusMesh *mesh = new Qt3DExtras::QTorusMesh;
        mesh->setRadius(1.0f);
        mesh->setMinorRadius(0.5f);
        mesh->setRings(16);
        mesh->setSlices(16);
        createAndAddEntity(QStringLiteral("Torus with Gooch"), mesh, material, transform);
    }
    // Custom cube with per-vertex colors
    {
        Qt3DExtras::QPerVertexColorMaterial *material = new Qt3DExtras::QPerVertexColorMaterial();
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(4.0f, 3.0f, -15.0f));
        transform->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(1.0f, 1.0f, 1.0f, 270.0f));

        Qt3DRender::QGeometryRenderer *boxMesh = createCustomCube();
        Qt3DRender::QBuffer *colorDataBuffer =
                new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, boxMesh->geometry());
        QByteArray colorBufferData;
        colorBufferData.resize(8 * 4 * sizeof(float));

        float *cPtr = reinterpret_cast<float *>(colorBufferData.data());
        for (int i = 0; i < 8; i++) {
            cPtr[i * 4] = float(i) / 8.0f;
            cPtr[i * 4 + 1] = float(8 - i) / 8.0f;
            cPtr[i * 4 + 2] = float((i + 4) % 8) / 8.0f;
            cPtr[i * 4 + 3] = 1.0f;
        }

        colorDataBuffer->setData(colorBufferData);

        addColorAttributeToGeometry(boxMesh->geometry(), colorDataBuffer, 8);

        createAndAddEntity(QStringLiteral("Custom cube with per-vertex colors"),
                           boxMesh, material, transform);
    }
    // Child cylinder with Phong
    {
        Qt3DCore::QEntity *parentEntity = findChildEntity(m_sceneRoot1,
                                                          QStringLiteral("Cylinder with Phong"));
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        material->setAmbient(QColor("#333333"));
        material->setDiffuse(QColor("#88FF00"));
        material->setSpecular(QColor("#000088"));
        material->setShininess(150.0f);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 4.0f, 0.0f));
        Qt3DExtras::QCylinderMesh *mesh = new Qt3DExtras::QCylinderMesh;
        mesh->setRadius(0.25f);
        mesh->setRings(3);
        mesh->setLength(1.5f);
        mesh->setSlices(16);
        createAndAddEntity(QStringLiteral("Child with Phong"),
                           mesh, material, transform, parentEntity);
    }
    // Cube with custom material
    {
        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
        material->setEffect(createOnTopEffect());
        material->addParameter(new Qt3DRender::QParameter(QStringLiteral("globalOffset"),
                                                          QVector3D(-3.0f, 0.0f, 3.0f)));
        material->addParameter(new Qt3DRender::QParameter(QStringLiteral("extraYOffset"), 3));
        material->effect()->addParameter(new Qt3DRender::QParameter(QStringLiteral("handleColor"),
                                                                    QColor(Qt::magenta)));
        material->effect()->addParameter(new Qt3DRender::QParameter(QStringLiteral("reverseOffset"),
                                                                    QVariant::fromValue(true)));

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setTranslation(QVector3D(0.0f, 2.0f, -40.0f));
        transform->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(1.0f, 2.0f, 3.0f, 90.0f));
        Qt3DRender::QGeometryRenderer *boxMesh = createCustomCube();
        Qt3DRender::QBuffer *offsetBuffer =
                new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, boxMesh->geometry());
        QByteArray offsetBufferData;
        offsetBufferData.resize(8 * 3 * sizeof(float));

        float *oPtr = reinterpret_cast<float *>(offsetBufferData.data());
        for (int i = 0; i < 8; i++) {
            oPtr[i * 3] = float(i) / 4.0f;
            oPtr[i * 3 + 1] = float(8 - i) / 4.0f + 2.0f;
            oPtr[i * 3 + 2] = float((i + 4) % 8) / 4.0f;
        }

        offsetBuffer->setData(offsetBufferData);

        Qt3DRender::QAttribute *customAttribute = new Qt3DRender::QAttribute();
        customAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        customAttribute->setBuffer(offsetBuffer);
        customAttribute->setDataType(Qt3DRender::QAttribute::Float);
        customAttribute->setDataSize(3);
        customAttribute->setByteOffset(0);
        customAttribute->setByteStride(0);
        customAttribute->setCount(8);
        customAttribute->setName(QStringLiteral("vertexOffset"));

        boxMesh->geometry()->addAttribute(customAttribute);

        createAndAddEntity(QStringLiteral("Custom cube with on-top material"),
                           boxMesh, material, transform);
    }

#ifdef VISUAL_CHECK
    m_view1->setGeometry(30, 30, 400, 400);
    m_view1->setRootEntity(m_sceneRoot1);
    m_view1->show();

    m_view2->setGeometry(450, 30, 400, 400);
    m_view2->setRootEntity(m_sceneRoot2);
    m_view2->show();

    QTest::qWaitForWindowExposed(m_view1);
    QTest::qWaitForWindowExposed(m_view2);
#endif
}

void tst_gltfPlugins::addPositionAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                                     Qt3DRender::QBuffer *buffer, int count)
{
    Qt3DRender::QAttribute *posAttribute = new Qt3DRender::QAttribute();
    posAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    posAttribute->setBuffer(buffer);
    posAttribute->setDataType(Qt3DRender::QAttribute::Float);
    posAttribute->setDataSize(3);
    posAttribute->setByteOffset(0);
    posAttribute->setByteStride(0);
    posAttribute->setCount(count);
    posAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());

    geometry->addAttribute(posAttribute);
}

void tst_gltfPlugins::addIndexAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                                  Qt3DRender::QBuffer *buffer, int count)
{
    Qt3DRender::QAttribute *indexAttribute = new Qt3DRender::QAttribute();
    indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    indexAttribute->setBuffer(buffer);
    indexAttribute->setDataType(Qt3DRender::QAttribute::UnsignedShort);
    indexAttribute->setDataSize(1);
    indexAttribute->setByteOffset(0);
    indexAttribute->setByteStride(0);
    indexAttribute->setCount(count);

    geometry->addAttribute(indexAttribute);
}

void tst_gltfPlugins::addColorAttributeToGeometry(Qt3DRender::QGeometry *geometry,
                                                  Qt3DRender::QBuffer *buffer, int count)
{
    Qt3DRender::QAttribute *colorAttribute = new Qt3DRender::QAttribute();
    colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    colorAttribute->setBuffer(buffer);
    colorAttribute->setDataType(Qt3DRender::QAttribute::Float);
    colorAttribute->setDataSize(4);
    colorAttribute->setByteOffset(0);
    colorAttribute->setByteStride(0);
    colorAttribute->setCount(count);
    colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());

    geometry->addAttribute(colorAttribute);
}

Qt3DCore::QEntity *tst_gltfPlugins::findChildEntity(Qt3DCore::QEntity *entity, const QString &name)
{
    for (auto child : entity->children()) {
        if (auto childEntity = qobject_cast<Qt3DCore::QEntity *>(child)) {
            if (childEntity->objectName() == name)
                return childEntity;
            if (auto foundEntity = findChildEntity(childEntity, name))
                return foundEntity;
        }
    }
    return nullptr;
}

Qt3DCore::QTransform *tst_gltfPlugins::transformComponent(Qt3DCore::QEntity *entity)
{
    for (auto component : entity->components()) {
        if (auto castedComponent = qobject_cast<Qt3DCore::QTransform *>(component))
            return castedComponent;
    }
    return nullptr;
}

Qt3DRender::QAbstractLight *tst_gltfPlugins::lightComponent(Qt3DCore::QEntity *entity)
{
    for (auto component : entity->components()) {
        if (auto castedComponent = qobject_cast<Qt3DRender::QAbstractLight *>(component))
            return castedComponent;
    }
    return nullptr;
}

Qt3DRender::QCameraLens *tst_gltfPlugins::cameraComponent(Qt3DCore::QEntity *entity)
{
    for (auto component : entity->components()) {
        if (auto castedComponent = qobject_cast<Qt3DRender::QCameraLens *>(component))
            return castedComponent;
    }
    return nullptr;
}

Qt3DRender::QGeometryRenderer *tst_gltfPlugins::meshComponent(Qt3DCore::QEntity *entity)
{
    for (auto component : entity->components()) {
        if (auto castedComponent = qobject_cast<Qt3DRender::QGeometryRenderer *>(component))
            return castedComponent;
    }
    return nullptr;
}

Qt3DRender::QMaterial *tst_gltfPlugins::materialComponent(Qt3DCore::QEntity *entity)
{
    for (auto component : entity->components()) {
        if (auto castedComponent = qobject_cast<Qt3DRender::QMaterial *>(component))
            return castedComponent;
    }
    return nullptr;
}

void tst_gltfPlugins::compareComponents(Qt3DCore::QComponent *c1, Qt3DCore::QComponent *c2)
{
    // Make sure component classes are the same and the non-pointer properties are the same
    QCOMPARE((c1 == nullptr), (c2 == nullptr));
    if (c1) {
        // Transform names are lost in export, as the transform is just part of the node item
        if (!qobject_cast<Qt3DCore::QTransform *>(c1))
            QCOMPARE(c1->objectName(), c2->objectName());
        QCOMPARE(c1->metaObject()->className(), c2->metaObject()->className());
        // Meshes are all imported as generic meshes
        if (auto mesh1 = qobject_cast<Qt3DRender::QGeometryRenderer *>(c1)) {
            auto mesh2 = qobject_cast<Qt3DRender::QGeometryRenderer *>(c2);
            QVERIFY(mesh2 != nullptr);
            auto geometry1 = mesh1->geometry();
            auto geometry2 = mesh2->geometry();
            // Check that attributes match.
            compareAttributes(
                        findAttribute(Qt3DRender::QAttribute::defaultPositionAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry1),
                        findAttribute(Qt3DRender::QAttribute::defaultPositionAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry2));
            compareAttributes(
                        findAttribute(Qt3DRender::QAttribute::defaultNormalAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry1),
                        findAttribute(Qt3DRender::QAttribute::defaultNormalAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry2));
            compareAttributes(
                        findAttribute(Qt3DRender::QAttribute::defaultTangentAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry1),
                        findAttribute(Qt3DRender::QAttribute::defaultTangentAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry2));
            compareAttributes(
                        findAttribute(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry1),
                        findAttribute(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry2));
            compareAttributes(
                        findAttribute(Qt3DRender::QAttribute::defaultColorAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry1),
                        findAttribute(Qt3DRender::QAttribute::defaultColorAttributeName(),
                                      Qt3DRender::QAttribute::VertexAttribute,
                                      geometry2));
            compareAttributes(
                        findAttribute(QStringLiteral(""),
                                      Qt3DRender::QAttribute::IndexAttribute,
                                      geometry1),
                        findAttribute(QStringLiteral(""),
                                      Qt3DRender::QAttribute::IndexAttribute,
                                      geometry2));
        } else {
            int count = c1->metaObject()->propertyCount();
            for (int i = 0; i < count; i++) {
                auto property = c1->metaObject()->property(i);
                auto v1 = c1->property(property.name());
                auto v2 = c2->property(property.name());
                if (v1.type() == QVariant::Bool) {
                    QCOMPARE(v1.toBool(), v2.toBool());
                } else if (v1.type() == QVariant::Color) {
                    QCOMPARE(v1.value<QColor>(), v2.value<QColor>());
                } else if (v1.type() == QVariant::Vector3D) {
                    QCOMPARE(v1.value<QVector3D>(), v2.value<QVector3D>());
                } else if (v1.type() == QVariant::Matrix4x4) {
                    QCOMPARE(v1.value<QMatrix4x4>(), v2.value<QMatrix4x4>());
                } else if (v1.canConvert(QMetaType::Float)) {
                    QVERIFY(qFuzzyCompare(v1.toFloat(), v2.toFloat()));
                }
            }
            if (QString::fromLatin1(c1->metaObject()->className())
                    .endsWith(QStringLiteral("Qt3DRender::QMaterial"))) {
                auto m1 = qobject_cast<Qt3DRender::QMaterial *>(c1);
                auto m2 = qobject_cast<Qt3DRender::QMaterial *>(c2);
                QVERIFY(m1);
                QVERIFY(m2);
                auto e1 = m1->effect();
                auto e2 = m2->effect();
                QVERIFY(e1);
                QVERIFY(e2);
                QCOMPARE(e1->objectName(), e2->objectName());
                QCOMPARE(e1->techniques().size(), e2->techniques().size());

                compareParameters(m1->parameters(), m2->parameters());
                compareParameters(e1->parameters(), e2->parameters());

                for (auto t1 : e1->techniques()) {
                    bool techMatch = false;
                    for (auto t2 : e2->techniques()) {
                        if (t1->objectName() == t2->objectName()) {
                            techMatch = true;
                            compareParameters(t1->parameters(), t2->parameters());
                            compareFilterKeys(t1->filterKeys(), t2->filterKeys());
                            compareRenderPasses(t1->renderPasses(), t2->renderPasses());
                            QCOMPARE(t1->graphicsApiFilter()->api(),
                                     t2->graphicsApiFilter()->api());
                            QCOMPARE(t1->graphicsApiFilter()->profile(),
                                     t2->graphicsApiFilter()->profile());
                            QCOMPARE(t1->graphicsApiFilter()->minorVersion(),
                                     t2->graphicsApiFilter()->minorVersion());
                            QCOMPARE(t1->graphicsApiFilter()->majorVersion(),
                                     t2->graphicsApiFilter()->majorVersion());
                            QCOMPARE(t1->graphicsApiFilter()->extensions(),
                                     t2->graphicsApiFilter()->extensions());
                            QCOMPARE(t1->graphicsApiFilter()->vendor(),
                                     t2->graphicsApiFilter()->vendor());
                        }
                    }
                    QVERIFY(techMatch);
                }
            }
        }
    }
}

Qt3DRender::QAttribute *tst_gltfPlugins::findAttribute(const QString &name,
                                                       Qt3DRender::QAttribute::AttributeType type,
                                                       Qt3DRender::QGeometry *geometry)
{
    for (auto att : geometry->attributes()) {
        if ((type == Qt3DRender::QAttribute::IndexAttribute && type == att->attributeType())
                || name == att->name()) {
            return att;
        }
    }
    return nullptr;
}

void tst_gltfPlugins::compareAttributes(Qt3DRender::QAttribute *a1, Qt3DRender::QAttribute *a2)
{
    QCOMPARE(a1 == nullptr, a2 == nullptr);
    if (a1) {
        QCOMPARE(a1->attributeType(), a2->attributeType());
        QCOMPARE(a1->vertexBaseType(), a2->vertexBaseType());
        QCOMPARE(a1->vertexSize(), a2->vertexSize());
        QCOMPARE(a1->count(), a2->count());
    }
}

void tst_gltfPlugins::compareParameters(const QVector<Qt3DRender::QParameter *> &params1,
                                        const QVector<Qt3DRender::QParameter *> &params2)
{
    QCOMPARE(params1.size(), params2.size());
    for (auto p1 : params1) {
        bool pMatch = false;
        for (auto p2 : params2) {
            if (p1->name() == p2->name()) {
                pMatch = true;
                if (p1->value().type() == QVariant::Color) {
                    // Colors are imported as QVector4Ds
                    QColor color = p1->value().value<QColor>();
                    QVector4D vec = p2->value().value<QVector4D>();
                    QCOMPARE(color.redF(), vec.x());
                    QCOMPARE(color.greenF(), vec.y());
                    QCOMPARE(color.blueF(), vec.z());
                    QCOMPARE(color.alphaF(), vec.w());
                } else if (p1->value().canConvert<Qt3DRender::QAbstractTexture *>()) {
                    QUrl u1 = getTextureUrl(p1->value().value<Qt3DRender::QAbstractTexture *>());
                    QUrl u2 = getTextureUrl(p2->value().value<Qt3DRender::QAbstractTexture *>());
                    QCOMPARE(u1.fileName(), u2.fileName());
                } else {
                    QCOMPARE(p1->value(), p2->value());
                }
            }
        }
        QVERIFY(pMatch);
    }
}

void tst_gltfPlugins::compareRenderPasses(const QVector<Qt3DRender::QRenderPass *> &passes1,
                                          const QVector<Qt3DRender::QRenderPass *> &passes2)
{
    QCOMPARE(passes1.size(), passes2.size());
    for (auto pass1 : passes1) {
        bool passMatch = false;
        for (auto pass2 : passes2) {
            if (pass1->objectName() == pass2->objectName()) {
                passMatch = true;
                compareFilterKeys(pass1->filterKeys(), pass2->filterKeys());
                compareParameters(pass1->parameters(), pass2->parameters());

                QVector<Qt3DRender::QRenderState *> states1 = pass1->renderStates();
                QVector<Qt3DRender::QRenderState *> states2 = pass2->renderStates();
                QCOMPARE(states1.size(), states2.size());
                for (auto state1 : states1) {
                    bool stateMatch = false;
                    for (auto state2 : states2) {
                        if (state1->metaObject()->className()
                                == state2->metaObject()->className()) {
                            stateMatch = true;
                        }
                    }
                    QVERIFY(stateMatch);
                }

                QCOMPARE(pass1->shaderProgram()->vertexShaderCode(),
                         pass2->shaderProgram()->vertexShaderCode());
                QCOMPARE(pass1->shaderProgram()->fragmentShaderCode(),
                         pass2->shaderProgram()->fragmentShaderCode());
            }
        }
        QVERIFY(passMatch);
    }
}

void tst_gltfPlugins::compareFilterKeys(const QVector<Qt3DRender::QFilterKey *> &keys1,
                                        const QVector<Qt3DRender::QFilterKey *> &keys2)
{
    QCOMPARE(keys1.size(), keys2.size());
    for (auto k1 : keys1) {
        bool kMatch = false;
        for (auto k2 : keys2) {
            if (k1->name() == k2->name()) {
                kMatch = true;
                QCOMPARE(k1->value(), k2->value());
            }
        }
        QVERIFY(kMatch);
    }
}

QUrl tst_gltfPlugins::getTextureUrl(Qt3DRender::QAbstractTexture *tex)
{
    QUrl url;
    if (tex->textureImages().size()) {
        Qt3DRender::QTextureImage *img =
                qobject_cast<Qt3DRender::QTextureImage *>(
                    tex->textureImages().at(0));
        if (img)
            url = img->source();
    }
    return url;
}

Qt3DRender::QGeometryRenderer *tst_gltfPlugins::createCustomCube()
{
    Qt3DRender::QGeometryRenderer *boxMesh = new Qt3DRender::QGeometryRenderer;
    Qt3DRender::QGeometry *boxGeometry = new Qt3DRender::QGeometry(boxMesh);
    Qt3DRender::QBuffer *boxDataBuffer =
            new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, boxGeometry);
    Qt3DRender::QBuffer *indexDataBuffer =
            new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, boxGeometry);
    QByteArray vertexBufferData;
    QByteArray indexBufferData;

    vertexBufferData.resize(8 * 3 * sizeof(float));
    indexBufferData.resize(12 * 3 * sizeof(ushort));

    float dimension = 1.0f;

    float *vPtr = reinterpret_cast<float *>(vertexBufferData.data());
    vPtr[0]  = -dimension; vPtr[1]  = -dimension; vPtr[2]  = -dimension;
    vPtr[3]  = dimension;  vPtr[4]  = -dimension; vPtr[5]  = -dimension;
    vPtr[6]  = dimension;  vPtr[7]  = -dimension; vPtr[8]  = dimension;
    vPtr[9]  = -dimension; vPtr[10] = -dimension; vPtr[11] = dimension;
    vPtr[12] = -dimension; vPtr[13] = dimension;  vPtr[14] = -dimension;
    vPtr[15] = dimension;  vPtr[16] = dimension;  vPtr[17] = -dimension;
    vPtr[18] = dimension;  vPtr[19] = dimension;  vPtr[20] = dimension;
    vPtr[21] = -dimension; vPtr[22] = dimension;  vPtr[23] = dimension;

    ushort *iPtr = reinterpret_cast<ushort *>(indexBufferData.data());
    iPtr[0]  = 2; iPtr[1]  = 0; iPtr[2]  = 1;
    iPtr[3]  = 2; iPtr[4]  = 3; iPtr[5]  = 0;
    iPtr[6]  = 1; iPtr[7]  = 6; iPtr[8]  = 2;
    iPtr[9]  = 1; iPtr[10] = 5; iPtr[11] = 6;
    iPtr[12] = 2; iPtr[13] = 7; iPtr[14] = 3;
    iPtr[15] = 2; iPtr[16] = 6; iPtr[17] = 7;
    iPtr[18] = 6; iPtr[19] = 5; iPtr[20] = 4;
    iPtr[21] = 6; iPtr[22] = 4; iPtr[23] = 7;
    iPtr[24] = 7; iPtr[25] = 0; iPtr[26] = 3;
    iPtr[27] = 7; iPtr[28] = 4; iPtr[29] = 0;
    iPtr[30] = 4; iPtr[31] = 1; iPtr[32] = 0;
    iPtr[33] = 4; iPtr[34] = 5; iPtr[35] = 1;

    boxDataBuffer->setData(vertexBufferData);
    indexDataBuffer->setData(indexBufferData);

    addPositionAttributeToGeometry(boxGeometry, boxDataBuffer, 8);
    addIndexAttributeToGeometry(boxGeometry, indexDataBuffer, 36);

    boxMesh->setInstanceCount(1);
    boxMesh->setIndexOffset(0);
    boxMesh->setFirstInstance(0);
    boxMesh->setVertexCount(36);
    boxMesh->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    boxMesh->setGeometry(boxGeometry);

    return boxMesh;
}

Qt3DRender::QEffect *tst_gltfPlugins::createOnTopEffect()
{
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;

    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique();
    technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);
    technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(2);
    technique->graphicsApiFilter()->setMinorVersion(1);

    Qt3DRender::QTechnique *techniqueCore = new Qt3DRender::QTechnique();
    techniqueCore->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);
    techniqueCore->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    techniqueCore->graphicsApiFilter()->setMajorVersion(3);
    techniqueCore->graphicsApiFilter()->setMinorVersion(1);

    Qt3DRender::QTechnique *techniqueES2 = new Qt3DRender::QTechnique();
    techniqueES2->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    techniqueES2->graphicsApiFilter()->setMajorVersion(2);
    techniqueES2->graphicsApiFilter()->setMinorVersion(0);
    techniqueES2->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);

    Qt3DRender::QFilterKey *filterkey1 = new Qt3DRender::QFilterKey(effect);
    Qt3DRender::QFilterKey *filterkey2 = new Qt3DRender::QFilterKey();
    filterkey1->setName(QStringLiteral("renderingStyle"));
    filterkey1->setValue(QStringLiteral("forward"));
    filterkey2->setName(QStringLiteral("dummyKey"));
    filterkey2->setValue(QStringLiteral("dummyValue"));

    Qt3DRender::QParameter *parameter1 = new Qt3DRender::QParameter(QStringLiteral("handleColor"),
                                                                    QColor(Qt::yellow));
    Qt3DRender::QParameter *parameter2 = new Qt3DRender::QParameter(QStringLiteral("customAlpha"),
                                                                    1.0f);
    Qt3DRender::QParameter *parameter3 = new Qt3DRender::QParameter(QStringLiteral("handleColor"),
                                                                    QColor(Qt::blue));
    Qt3DRender::QTexture2D *texture = new Qt3DRender::QTexture2D;
    Qt3DRender::QParameter *parameter4 =
            new Qt3DRender::QParameter(QStringLiteral("customTexture"), texture);
    Qt3DRender::QTextureImage *ti = new Qt3DRender::QTextureImage();
    parameter4->value().value<Qt3DRender::QAbstractTexture *>()->addTextureImage(ti);
    ti->setSource(QUrl(QStringLiteral("qrc:/qtlogo.png")));

    technique->addFilterKey(filterkey1);
    technique->addFilterKey(filterkey2);
    techniqueES2->addFilterKey(filterkey1);
    techniqueES2->addFilterKey(filterkey2);
    techniqueCore->addFilterKey(filterkey1);

    technique->addParameter(parameter1);
    technique->addParameter(parameter2);
    technique->addParameter(parameter4);
    techniqueES2->addParameter(parameter1);
    techniqueES2->addParameter(parameter2);

    Qt3DRender::QShaderProgram *shader = new Qt3DRender::QShaderProgram();
    Qt3DRender::QShaderProgram *shaderES2 = new Qt3DRender::QShaderProgram();
    shader->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(
                                    QUrl(QStringLiteral("qrc:/ontopmaterial.vert"))));
    shader->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(
                                      QUrl(QStringLiteral("qrc:/ontopmaterial.frag"))));
    shaderES2->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(
                                       QUrl(QStringLiteral("qrc:/ontopmaterialES2.vert"))));
    shaderES2->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(
                                         QUrl(QStringLiteral("qrc:/ontopmaterialES2.frag"))));
    shader->setObjectName(QStringLiteral("Basic shader"));
    shaderES2->setObjectName(QStringLiteral("ES2 shader"));

    Qt3DRender::QRenderPass *renderPass = new Qt3DRender::QRenderPass();
    Qt3DRender::QRenderPass *renderPassES2 = new Qt3DRender::QRenderPass();
    renderPass->setShaderProgram(shader);
    renderPassES2->setShaderProgram(shaderES2);
    renderPass->addFilterKey(filterkey2);
    renderPass->addParameter(parameter3);
    Qt3DRender::QColorMask *cmask = new Qt3DRender::QColorMask;
    cmask->setRedMasked(false);
    renderPass->addRenderState(cmask);
    Qt3DRender::QBlendEquation *be = new Qt3DRender::QBlendEquation;
    be->setBlendFunction(Qt3DRender::QBlendEquation::Subtract);
    renderPass->addRenderState(be);
    technique->addRenderPass(renderPassES2);
    techniqueES2->addRenderPass(renderPassES2);
    techniqueCore->addRenderPass(renderPass);
    technique->setObjectName(QStringLiteral("Basic technique"));
    techniqueES2->setObjectName(QStringLiteral("ES2 technique"));
    techniqueCore->setObjectName(QStringLiteral("Core technique"));
    renderPass->setObjectName(QStringLiteral("Basic pass"));
    renderPassES2->setObjectName(QStringLiteral("ES2 pass"));

    effect->addTechnique(technique);
    effect->addTechnique(techniqueES2);
    effect->addTechnique(techniqueCore);
    effect->setObjectName(QStringLiteral("OnTopEffect"));

    return effect;
}

Qt3DCore::QEntity *tst_gltfPlugins::findCameraChild(Qt3DCore::QEntity *entity,
                                                    Qt3DRender::QCameraLens::ProjectionType type)
{
    for (auto child : entity->children()) {
        if (auto childEntity = qobject_cast<Qt3DCore::QEntity *>(child)) {
            for (auto component : childEntity->components()) {
                if (auto cameraLens = qobject_cast<Qt3DRender::QCameraLens *>(component)) {
                    if (cameraLens->projectionType() == type)
                        return childEntity;
                }
            }
            if (auto cameraEntity = findCameraChild(childEntity, type))
                return cameraEntity;
        }
    }
    return nullptr;
}

void tst_gltfPlugins::exportAndImport_data()
{
    QTest::addColumn<bool>("binaryJson");
    QTest::addColumn<bool>("compactJson");

    QTest::newRow("No options") << false << false;
#ifndef VISUAL_CHECK
    QTest::newRow("Binary json") << true << false;
    QTest::newRow("Compact json") << false << true;
    QTest::newRow("Binary/Compact json") << true << true; // Compact is ignored in this case
#endif
}

void tst_gltfPlugins::exportAndImport()
{
    QFETCH(bool, binaryJson);
    QFETCH(bool, compactJson);

    createTestScene();

#ifdef PRESERVE_EXPORT
    m_exportDir->setAutoRemove(false);
    qDebug() << "Export Directory:" << m_exportDir->path();
#endif

    const QString sceneName = QStringLiteral("MyGLTFScene");
    const QString exportDir = m_exportDir->path();

    // Export the created scene using GLTF export plugin
    QStringList keys = Qt3DRender::QSceneExportFactory::keys();
    for (const QString &key : keys) {
        Qt3DRender::QSceneExporter *exporter =
                Qt3DRender::QSceneExportFactory::create(key, QStringList());
        if (exporter != nullptr && key == QStringLiteral("gltfexport")) {
            QVariantHash options;
            options.insert(QStringLiteral("binaryJson"), QVariant(binaryJson));
            options.insert(QStringLiteral("compactJson"), QVariant(compactJson));
            exporter->exportScene(m_sceneRoot1, exportDir, sceneName, options);
            break;
        }
    }

    QCoreApplication::processEvents();

    // Import the exported scene using GLTF import plugin
    Qt3DCore::QEntity *importedScene = nullptr;
    keys = Qt3DRender::QSceneImportFactory::keys();
    for (auto key : keys) {
        Qt3DRender::QSceneImporter *importer =
                Qt3DRender::QSceneImportFactory::create(key, QStringList());
        if (importer != nullptr && key == QStringLiteral("gltf")) {
            QString sceneSource = exportDir;
            if (!sceneSource.endsWith(QLatin1Char('/')))
                sceneSource.append(QLatin1Char('/'));
            sceneSource += sceneName;
            sceneSource += QLatin1Char('/');
            sceneSource += sceneName;
            sceneSource += QStringLiteral(".qgltf");
            importer->setSource(QUrl::fromLocalFile(sceneSource));
            importedScene = importer->scene();
            break;
        }
    }

    importedScene->setParent(m_sceneRoot2);

    // Compare contents of the original scene and the exported one.
    for (auto it = m_entityMap.begin(), end = m_entityMap.end(); it != end; ++it) {
        QString name = it.key();
        Qt3DCore::QEntity *exportedEntity = it.value();
        Qt3DCore::QEntity *importedEntity = findChildEntity(importedScene, name);
        QVERIFY(importedEntity != nullptr);
        if (importedEntity) {
            compareComponents(transformComponent(exportedEntity),
                              transformComponent(importedEntity));
            compareComponents(lightComponent(exportedEntity),
                              lightComponent(importedEntity));
            compareComponents(cameraComponent(exportedEntity),
                              cameraComponent(importedEntity));
            compareComponents(meshComponent(exportedEntity),
                              meshComponent(importedEntity));
            compareComponents(materialComponent(exportedEntity),
                              materialComponent(importedEntity));
            Qt3DRender::QCamera *exportedCamera =
                    qobject_cast<Qt3DRender::QCamera *>(exportedEntity);
            if (exportedCamera) {
                Qt3DRender::QCamera *importedCamera =
                        qobject_cast<Qt3DRender::QCamera *>(importedEntity);
                QVERIFY(importedCamera != nullptr);
                QCOMPARE(exportedCamera->position(), importedCamera->position());
                QCOMPARE(exportedCamera->upVector(), importedCamera->upVector());
                QCOMPARE(exportedCamera->viewCenter(), importedCamera->viewCenter());
            }
        }
    }


#ifdef VISUAL_CHECK
    qDebug() << "Dumping original entity tree:";
    walkEntity(m_sceneRoot1, 0);
    qDebug() << "Dumping imported entity tree:";
    walkEntity(importedScene, 0);

    // Find the camera to actually show the scene
    m_view2->defaultFrameGraph()->setCamera(
                findCameraChild(m_sceneRoot2, Qt3DRender::QCameraLens::OrthographicProjection));
    QTest::qWait(VISUAL_CHECK);

    m_view1->defaultFrameGraph()->setCamera(
                findCameraChild(m_sceneRoot1, Qt3DRender::QCameraLens::PerspectiveProjection));
    m_view2->defaultFrameGraph()->setCamera(
                findCameraChild(m_sceneRoot2, Qt3DRender::QCameraLens::PerspectiveProjection));
    QTest::qWait(VISUAL_CHECK);
#endif
}

QTEST_MAIN(tst_gltfPlugins)

#include "tst_gltfplugins.moc"
