/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "gltfio.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtGui/QVector2D>

#include <Qt3DRender/QCameraLens>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <Qt3DRender/private/qurlhelper_p.h>

#include <Qt3DRender/QAlphaCoverage>
#include <Qt3DRender/QBlendEquation>
#include <Qt3DRender/QBlendEquationArguments>
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QNoDepthMask>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QFrontFace>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QPolygonOffset>
#include <Qt3DRender/QRenderState>
#include <Qt3DRender/QScissorTest>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTexture>

#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseMapMaterial>
#include <Qt3DExtras/QDiffuseSpecularMapMaterial>
#include <Qt3DExtras/QNormalDiffuseMapMaterial>
#include <Qt3DExtras/QNormalDiffuseSpecularMapMaterial>

#ifndef qUtf16PrintableImpl // -Impl is a Qt 5.8 feature
#  define qUtf16PrintableImpl(string) \
    static_cast<const wchar_t*>(static_cast<const void*>(string.utf16()))
#endif

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;
using namespace Qt3DExtras;

namespace Qt3DRender {

Q_LOGGING_CATEGORY(GLTFIOLog, "Qt3D.GLTFIO")

#define KEY_CAMERA       QLatin1String("camera")
#define KEY_CAMERAS      QLatin1String("cameras")
#define KEY_SCENES       QLatin1String("scenes")
#define KEY_NODES        QLatin1String("nodes")
#define KEY_MESHES       QLatin1String("meshes")
#define KEY_CHILDREN     QLatin1String("children")
#define KEY_MATRIX       QLatin1String("matrix")
#define KEY_ROTATION     QLatin1String("rotation")
#define KEY_SCALE        QLatin1String("scale")
#define KEY_TRANSLATION  QLatin1String("translation")
#define KEY_TYPE         QLatin1String("type")
#define KEY_PERSPECTIVE  QLatin1String("perspective")
#define KEY_NAME         QLatin1String("name")
#define KEY_COUNT        QLatin1String("count")
#define KEY_YFOV         QLatin1String("yfov")
#define KEY_ZNEAR        QLatin1String("znear")
#define KEY_ZFAR         QLatin1String("zfar")
#define KEY_MATERIALS    QLatin1String("materials")
#define KEY_EXTENSIONS   QLatin1String("extensions")
#define KEY_COMMON_MAT   QLatin1String("KHR_materials_common")
#define KEY_TECHNIQUE    QLatin1String("technique")
#define KEY_VALUES       QLatin1String("values")
#define KEY_BUFFERS      QLatin1String("buffers")
#define KEY_SHADERS      QLatin1String("shaders")
#define KEY_PROGRAMS     QLatin1String("programs")
#define KEY_PROGRAM      QLatin1String("program")
#define KEY_TECHNIQUES   QLatin1String("techniques")
#define KEY_ACCESSORS    QLatin1String("accessors")
#define KEY_IMAGES       QLatin1String("images")
#define KEY_TEXTURES     QLatin1String("textures")
#define KEY_SCENE        QLatin1String("scene")
#define KEY_BUFFER       QLatin1String("buffer")
#define KEY_TARGET       QLatin1String("target")
#define KEY_BYTE_OFFSET  QLatin1String("byteOffset")
#define KEY_BYTE_LENGTH  QLatin1String("byteLength")
#define KEY_BYTE_STRIDE  QLatin1String("byteStride")
#define KEY_PRIMITIVES   QLatin1String("primitives")
#define KEY_MODE         QLatin1String("mode")
#define KEY_MATERIAL     QLatin1String("material")
#define KEY_ATTRIBUTES   QLatin1String("attributes")
#define KEY_INDICES      QLatin1String("indices")
#define KEY_URI          QLatin1String("uri")
#define KEY_FORMAT       QLatin1String("format")
#define KEY_PASSES       QLatin1String("passes")
#define KEY_SOURCE       QLatin1String("source")
#define KEY_SAMPLER      QLatin1String("sampler")
#define KEY_SAMPLERS     QLatin1String("samplers")
#define KEY_SEMANTIC     QLatin1String("semantic")
#define KEY_STATES       QLatin1String("states")
#define KEY_UNIFORMS     QLatin1String("uniforms")
#define KEY_PARAMETERS   QLatin1String("parameters")
#define KEY_WRAP_S       QLatin1String("wrapS")
#define KEY_MIN_FILTER   QLatin1String("minFilter")
#define KEY_MAG_FILTER   QLatin1String("magFilter")

#define KEY_INSTANCE_TECHNIQUE  QLatin1String("instanceTechnique")
#define KEY_INSTANCE_PROGRAM    QLatin1String("instanceProgram")
#define KEY_BUFFER_VIEWS        QLatin1String("bufferViews")
#define KEY_BUFFER_VIEW         QLatin1String("bufferView")
#define KEY_VERTEX_SHADER       QLatin1String("vertexShader")
#define KEY_FRAGMENT_SHADER     QLatin1String("fragmentShader")
#define KEY_INTERNAL_FORMAT     QLatin1String("internalFormat")
#define KEY_COMPONENT_TYPE      QLatin1String("componentType")
#define KEY_ASPECT_RATIO        QLatin1String("aspect_ratio")
#define KEY_VALUE               QLatin1String("value")
#define KEY_ENABLE              QLatin1String("enable")
#define KEY_FUNCTIONS           QLatin1String("functions")
#define KEY_TECHNIQUE_CORE      QLatin1String("techniqueCore")
#define KEY_TECHNIQUE_GL2       QLatin1String("techniqueGL2")

GLTFIO::GLTFIO() : QSceneIOHandler(),
    m_parseDone(false)
{
}

GLTFIO::~GLTFIO()
{

}

void GLTFIO::setBasePath(const QString& path)
{
    m_basePath = path;
}

bool GLTFIO::setJSON(const QJsonDocument &json )
{
    if ( !json.isObject() ) {
        return false;
    }

    m_json = json;
    m_parseDone = false;

    cleanup();

    return true;
}

/*!
 * Sets the \a path used by the parser to load the scene file.
 * If the file is valid, parsing is automatically triggered.
 */
void GLTFIO::setSource(const QUrl &source)
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    QFileInfo finfo(path);
    if (Q_UNLIKELY(!finfo.exists())) {
        qCWarning(GLTFIOLog, "missing file: %ls", qUtf16PrintableImpl(path));
        return;
    }
    QFile f(path);
    f.open(QIODevice::ReadOnly);

    QByteArray jsonData = f.readAll();
    QJsonDocument sceneDocument = QJsonDocument::fromBinaryData(jsonData);
    if (sceneDocument.isNull())
        sceneDocument = QJsonDocument::fromJson(jsonData);

    if (Q_UNLIKELY(!setJSON(sceneDocument))) {
        qCWarning(GLTFIOLog, "not a JSON document");
        return;
    }

    setBasePath(finfo.dir().absolutePath());
}

/*!
 * Returns true if the extension of \a path is supported by the
 * GLTF parser.
 */
bool GLTFIO::isFileTypeSupported(const QUrl &source) const
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    return GLTFIO::isGLTFPath(path);
}

Qt3DCore::QEntity* GLTFIO::node(const QString &id)
{
    QJsonObject nodes = m_json.object().value(KEY_NODES).toObject();
    const auto jsonVal = nodes.value(id);
    if (Q_UNLIKELY(jsonVal.isUndefined())) {
        qCWarning(GLTFIOLog, "unknown node %ls in GLTF file %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        return NULL;
    }

    const QJsonObject jsonObj = jsonVal.toObject();
    QEntity* result = nullptr;

    // Qt3D has a limitation that a QEntity can only have 1 mesh and 1 material component
    // So if the node has only 1 mesh, we only create 1 QEntity
    // Otherwise if there are n meshes, there is 1 QEntity, with n children for each mesh/material combo
    {
        QVector<QEntity *> entities;

        const auto meshes = jsonObj.value(KEY_MESHES).toArray();
        for (const QJsonValue &mesh : meshes) {
            const QString meshName = mesh.toString();
            const auto geometryRenderers = qAsConst(m_meshDict).equal_range(meshName);
            if (Q_UNLIKELY(geometryRenderers.first == geometryRenderers.second)) {
                qCWarning(GLTFIOLog, "node %ls references unknown mesh %ls",
                          qUtf16PrintableImpl(id), qUtf16PrintableImpl(meshName));
                continue;
            }

            for (auto it = geometryRenderers.first; it != geometryRenderers.second; ++it) {
                QGeometryRenderer *geometryRenderer = it.value();
                QEntity *entity = new QEntity;
                entity->addComponent(geometryRenderer);
                QMaterial *mat = material(m_meshMaterialDict[geometryRenderer]);
                if (mat)
                    entity->addComponent(mat);
                entities.append(entity);
            }

        }

        switch (entities.size()) {
        case 0:
            break;
        case 1:
            result = qAsConst(entities).first();
            break;
        default:
            result = new QEntity;
            for (QEntity *entity : qAsConst(entities))
                entity->setParent(result);
        }
    }

    //If the entity contains no meshes, results will still be null here
    if (result == nullptr)
        result = new QEntity;

    {
        const auto children = jsonObj.value(KEY_CHILDREN).toArray();
        for (const QJsonValue &c : children) {
            QEntity* child = node(c.toString());
            if (!child)
                continue;
            child->setParent(result);
        }
    }

    renameFromJson(jsonObj, result);


    // Node Transforms
    Qt3DCore::QTransform *trans = nullptr;
    const auto matrix = jsonObj.value(KEY_MATRIX);
    if (!matrix.isUndefined()) {
        QMatrix4x4 m(Qt::Uninitialized);

        QJsonArray matrixValues = matrix.toArray();
        for (int i=0; i<16; ++i) {
            double v = matrixValues.at( i ).toDouble();
            m(i % 4, i >> 2) = v;
        }

        // ADD MATRIX TRANSFORM COMPONENT TO ENTITY
        if (trans == nullptr)
            trans = new Qt3DCore::QTransform;
        trans->setMatrix(m);
    }

    // Rotation quaternion
    const auto rotation = jsonObj.value(KEY_ROTATION);
    if (!rotation.isUndefined()) {
        if (!trans)
            trans = new Qt3DCore::QTransform;

        const QJsonArray quaternionValues = rotation.toArray();
        QQuaternion quaternion(quaternionValues[0].toDouble(),
                               quaternionValues[1].toDouble(),
                               quaternionValues[2].toDouble(),
                               quaternionValues[3].toDouble());
        trans->setRotation(quaternion);
    }

    // Translation
    const auto translation = jsonObj.value(KEY_TRANSLATION);
    if (!translation.isUndefined()) {
        if (!trans)
            trans = new Qt3DCore::QTransform;

        const QJsonArray translationValues = translation.toArray();
        trans->setTranslation(QVector3D(translationValues[0].toDouble(),
                                        translationValues[1].toDouble(),
                                        translationValues[2].toDouble()));
    }

    // Scale
    const auto scale = jsonObj.value(KEY_SCALE);
    if (!scale.isUndefined()) {
        if (!trans)
            trans = new Qt3DCore::QTransform;

        const QJsonArray scaleValues = scale.toArray();
        trans->setScale3D(QVector3D(scaleValues[0].toDouble(),
                                    scaleValues[1].toDouble(),
                                    scaleValues[2].toDouble()));
    }

    // Add the Transform component
    if (trans != nullptr)
        result->addComponent(trans);

    const auto cameraVal = jsonObj.value(KEY_CAMERA);
    if (!cameraVal.isUndefined()) {
        QCameraLens* cam = camera(cameraVal.toString());
        if (Q_UNLIKELY(!cam)) {
            qCWarning(GLTFIOLog) << "failed to build camera:" << cameraVal
                                     << "on node" << id;
        } else {
            result->addComponent(cam);
        }
    } // of have camera attribute

    return result;
}

Qt3DCore::QEntity* GLTFIO::scene(const QString &id)
{
    parse();

    QJsonObject scenes = m_json.object().value(KEY_SCENES).toObject();
    const auto sceneVal = scenes.value(id);
    if (Q_UNLIKELY(sceneVal.isUndefined())) {
        if (Q_UNLIKELY(!id.isNull()))
            qCWarning(GLTFIOLog, "GLTF: no such scene %ls in file %ls",
                      qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        return defaultScene();
    }

    QJsonObject sceneObj = sceneVal.toObject();
    QEntity* sceneEntity = new QEntity;
    const auto nodes = sceneObj.value(KEY_NODES).toArray();
    for (const QJsonValue &nnv : nodes) {
        QString nodeName = nnv.toString();
        QEntity* child = node(nodeName);
        if (!child)
            continue;
        child->setParent(sceneEntity);
    }

    return sceneEntity;
}

GLTFIO::BufferData::BufferData()
    : length(0)
    , data(nullptr)
{
}

GLTFIO::BufferData::BufferData(const QJsonObject &json)
    : length(json.value(KEY_BYTE_LENGTH).toInt()),
      path(json.value(KEY_URI).toString()),
      data(nullptr)
{
}

GLTFIO::ParameterData::ParameterData() :
    type(0)
{

}

GLTFIO::ParameterData::ParameterData(const QJsonObject &json)
    : semantic(json.value(KEY_SEMANTIC).toString()),
      type(json.value(KEY_TYPE).toInt())
{
}

GLTFIO::AccessorData::AccessorData()
    : type(QAttribute::Float)
    , dataSize(0)
    , count(0)
    , offset(0)
    , stride(0)
{

}

GLTFIO::AccessorData::AccessorData(const QJsonObject &json)
    : bufferViewName(json.value(KEY_BUFFER_VIEW).toString()),
      type(accessorTypeFromJSON(json.value(KEY_COMPONENT_TYPE).toInt())),
      dataSize(accessorDataSizeFromJson(json.value(KEY_TYPE).toString())),
      count(json.value(KEY_COUNT).toInt()),
      offset(0),
      stride(0)
{
    const auto byteOffset = json.value(KEY_BYTE_OFFSET);
    if (!byteOffset.isUndefined())
        offset = byteOffset.toInt();
    const auto byteStride = json.value(KEY_BYTE_STRIDE);
    if (!byteStride.isUndefined())
        stride = byteStride.toInt();
}

bool GLTFIO::isGLTFPath(const QString& path)
{
    QFileInfo finfo(path);
    if (!finfo.exists())
        return false;

    // might need to detect other things in the future, but would
    // prefer to avoid doing a full parse.
    QString suffix = finfo.suffix().toLower();
    return suffix == QLatin1String("json") || suffix == QLatin1String("gltf") || suffix == QLatin1String("qgltf");
}

void GLTFIO::renameFromJson(const QJsonObject &json, QObject * const object)
{
    const auto name = json.value(KEY_NAME);
    if (!name.isUndefined())
        object->setObjectName(name.toString());
}

bool GLTFIO::hasStandardUniformNameFromSemantic(const QString &semantic)
{
    //Standard Uniforms
    if (semantic.isEmpty())
        return false;
    switch (semantic.at(0).toLatin1()) {
    case 'L':
        // return semantic == QLatin1String("LOCAL");
        return false;
    case 'M':
        return semantic == QLatin1String("MODEL")
            || semantic == QLatin1String("MODELVIEW")
            || semantic == QLatin1String("MODELVIEWPROJECTION")
            || semantic == QLatin1String("MODELINVERSE")
            || semantic == QLatin1String("MODELVIEWPROJECTIONINVERSE")
            || semantic == QLatin1String("MODELINVERSETRANSPOSE")
            || semantic == QLatin1String("MODELVIEWINVERSETRANSPOSE");
    case 'V':
        return semantic == QLatin1String("VIEW")
            || semantic == QLatin1String("VIEWINVERSE")
            || semantic == QLatin1String("VIEWPORT");
    case 'P':
        return semantic == QLatin1String("PROJECTION")
            || semantic == QLatin1String("PROJECTIONINVERSE");
    }
    return false;
}

QString GLTFIO::standardAttributeNameFromSemantic(const QString &semantic)
{
    //Standard Attributes
    if (semantic.startsWith(QLatin1String("POSITION")))
        return QAttribute::defaultPositionAttributeName();
    if (semantic.startsWith(QLatin1String("NORMAL")))
        return QAttribute::defaultNormalAttributeName();
    if (semantic.startsWith(QLatin1String("TEXCOORD")))
        return QAttribute::defaultTextureCoordinateAttributeName();
    if (semantic.startsWith(QLatin1String("COLOR")))
        return QAttribute::defaultColorAttributeName();
    if (semantic.startsWith(QLatin1String("TANGENT")))
        return QAttribute::defaultTangentAttributeName();

//    if (semantic.startsWith(QLatin1String("JOINT")));
//    if (semantic.startsWith(QLatin1String("JOINTMATRIX")));
//    if (semantic.startsWith(QLatin1String("WEIGHT")));

    return QString();
}

QParameter *GLTFIO::parameterFromTechnique(QTechnique *technique, const QString &parameterName)
{
    const auto parameters = technique->parameters();
    for (QParameter *parameter : parameters) {
        if (parameter->name() == parameterName) {
            return parameter;
        }
    }

    return nullptr;
}

Qt3DCore::QEntity* GLTFIO::defaultScene()
{
    if (Q_UNLIKELY(m_defaultScene.isEmpty())) {
        qCWarning(GLTFIOLog, "no default scene");
        return NULL;
    }

    return scene(m_defaultScene);
}

QMaterial *GLTFIO::materialWithCustomShader(const QString &id, const QJsonObject &jsonObj)
{
    //Default ES2 Technique
    QString techniqueName = jsonObj.value(KEY_TECHNIQUE).toString();
    const auto it = qAsConst(m_techniques).find(techniqueName);
    if (Q_UNLIKELY(it == m_techniques.cend())) {
        qCWarning(GLTFIOLog, "unknown technique %ls for material %ls in GLTF file %ls",
                  qUtf16PrintableImpl(techniqueName), qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        return NULL;
    }
    QTechnique *technique = *it;
    technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    technique->graphicsApiFilter()->setMajorVersion(2);
    technique->graphicsApiFilter()->setMinorVersion(0);
    technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);


    //Optional Core technique
    QTechnique *coreTechnique = nullptr;
    QTechnique *gl2Technique = nullptr;
    QString coreTechniqueName = jsonObj.value(KEY_TECHNIQUE_CORE).toString();
    if (!coreTechniqueName.isNull()) {
        const auto it = qAsConst(m_techniques).find(coreTechniqueName);
        if (Q_UNLIKELY(it == m_techniques.cend())) {
            qCWarning(GLTFIOLog, "unknown technique %ls for material %ls in GLTF file %ls",
                      qUtf16PrintableImpl(coreTechniqueName), qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        } else {
            coreTechnique = it.value();
            coreTechnique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
            coreTechnique->graphicsApiFilter()->setMajorVersion(3);
            coreTechnique->graphicsApiFilter()->setMinorVersion(1);
            coreTechnique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);
        }
    }
    //Optional GL2 technique
    QString gl2TechniqueName = jsonObj.value(KEY_TECHNIQUE_GL2).toString();
    if (!gl2TechniqueName.isNull()) {
        const auto it = qAsConst(m_techniques).find(gl2TechniqueName);
        if (Q_UNLIKELY(it == m_techniques.cend())) {
            qCWarning(GLTFIOLog, "unknown technique %ls for material %ls in GLTF file %ls",
                      qUtf16PrintableImpl(gl2TechniqueName), qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        } else {
            gl2Technique = it.value();
            gl2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
            gl2Technique->graphicsApiFilter()->setMajorVersion(2);
            gl2Technique->graphicsApiFilter()->setMinorVersion(0);
            gl2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);
        }
    }


    // glTF doesn't deal in effects, but we need a trivial one to wrap
    // up our techniques
    // However we need to create a unique effect for each material instead
    // of caching because QMaterial does not keep up with effects
    // its not the parent of.
    QEffect* effect = new QEffect;
    effect->setObjectName(techniqueName);
    effect->addTechnique(technique);
    if (coreTechnique != nullptr)
        effect->addTechnique(coreTechnique);
    if (gl2Technique != nullptr)
        effect->addTechnique(gl2Technique);

    QMaterial* mat = new QMaterial;
    mat->setEffect(effect);

    renameFromJson(jsonObj, mat);

    const QJsonObject values = jsonObj.value(KEY_VALUES).toObject();
    for (auto it = values.begin(), end = values.end(); it != end; ++it) {
        const QString vName = it.key();
        QParameter *param = parameterFromTechnique(technique, vName);

        if (param == nullptr && coreTechnique != nullptr) {
            param = parameterFromTechnique(coreTechnique, vName);
        }

        if (param == nullptr && gl2Technique != nullptr) {
            param = parameterFromTechnique(gl2Technique, vName);
        }

        if (Q_UNLIKELY(!param)) {
            qCWarning(GLTFIOLog, "unknown parameter: %ls in technique %ls processing material %ls",
                      qUtf16PrintableImpl(vName), qUtf16PrintableImpl(techniqueName), qUtf16PrintableImpl(id));
            continue;
        }

        ParameterData paramData = m_parameterDataDict.value(param);
        QVariant var = parameterValueFromJSON(paramData.type, it.value());

        mat->addParameter(new QParameter(param->name(), var));
    } // of material technique-instance values iteration

    return mat;
}

static inline QVariant vec4ToRgb(const QVariant &vec4Var)
{
    const QVector4D v = vec4Var.value<QVector4D>();
    return QVariant(QColor::fromRgbF(v.x(), v.y(), v.z()));
}

QMaterial *GLTFIO::commonMaterial(const QJsonObject &jsonObj)
{
    QVariantHash params;
    bool hasDiffuseMap = false;
    bool hasSpecularMap = false;
    bool hasNormalMap = false;

    const QJsonObject values = jsonObj.value(KEY_VALUES).toObject();
    for (auto it = values.begin(), end = values.end(); it != end; ++it) {
        const QString vName = it.key();
        const QJsonValue val = it.value();
        QVariant var;
        QString propertyName = vName;
        if (vName == QLatin1String("ambient") && val.isArray()) {
            var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
        } else if (vName == QLatin1String("diffuse")) {
            if (val.isString()) {
                var = parameterValueFromJSON(GL_SAMPLER_2D, val);
                hasDiffuseMap = true;
            } else if (val.isArray()) {
                var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
            }
        } else if (vName == QLatin1String("specular")) {
            if (val.isString()) {
                var = parameterValueFromJSON(GL_SAMPLER_2D, val);
                hasSpecularMap = true;
            } else if (val.isArray()) {
                var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
            }
        } else if (vName == QLatin1String("shininess") && val.isDouble()) {
            var = parameterValueFromJSON(GL_FLOAT, val);
        } else if (vName == QLatin1String("normalmap") && val.isString()) {
            var = parameterValueFromJSON(GL_SAMPLER_2D, val);
            propertyName = QStringLiteral("normal");
            hasNormalMap = true;
        } else if (vName == QLatin1String("transparency")) {
            qCWarning(GLTFIOLog, "Semi-transparent common materials are not currently supported, ignoring alpha");
        }
        if (var.isValid())
            params[propertyName] = var;
    }

    QMaterial *mat = nullptr;
    if (hasNormalMap) {
        if (hasSpecularMap) {
            mat = new QNormalDiffuseSpecularMapMaterial;
        } else {
            if (Q_UNLIKELY(!hasDiffuseMap))
                qCWarning(GLTFIOLog, "Common material with normal and specular maps needs a diffuse map as well");
            else
                mat = new QNormalDiffuseMapMaterial;
        }
    } else {
        if (hasSpecularMap) {
            if (Q_UNLIKELY(!hasDiffuseMap))
                qCWarning(GLTFIOLog, "Common material with specular map needs a diffuse map as well");
            else
                mat = new QDiffuseSpecularMapMaterial;
        } else if (hasDiffuseMap) {
            mat = new QDiffuseMapMaterial;
        } else {
            mat = new QPhongMaterial;
        }
    }

    if (Q_UNLIKELY(!mat)) {
        qCWarning(GLTFIOLog, "Could not find a suitable built-in material for KHR_materials_common");
    } else {
        for (QVariantHash::const_iterator it = params.constBegin(), itEnd = params.constEnd(); it != itEnd; ++it)
            mat->setProperty(it.key().toUtf8(), it.value());
    }

    return mat;
}

QMaterial* GLTFIO::material(const QString &id)
{
    const auto it = qAsConst(m_materialCache).find(id);
    if (it != m_materialCache.cend())
        return it.value();

    QJsonObject mats = m_json.object().value(KEY_MATERIALS).toObject();
    const auto jsonVal = mats.value(id);
    if (Q_UNLIKELY(jsonVal.isUndefined())) {
        qCWarning(GLTFIOLog, "unknown material %ls in GLTF file %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        return NULL;
    }

    const QJsonObject jsonObj = jsonVal.toObject();

    QMaterial *mat = nullptr;

    // Prefer common materials over custom shaders.
    const auto extensionMat = jsonObj.value(KEY_EXTENSIONS).toObject().value(KEY_COMMON_MAT);
    if (!extensionMat.isUndefined())
        mat = commonMaterial(extensionMat.toObject());

    if (!mat)
        mat = materialWithCustomShader(id, jsonObj);

    m_materialCache[id] = mat;
    return mat;
}

QCameraLens* GLTFIO::camera(const QString &id) const
{
    const auto jsonVal = m_json.object().value(KEY_CAMERAS).toObject().value(id);
    if (Q_UNLIKELY(jsonVal.isUndefined())) {
        qCWarning(GLTFIOLog, "unknown camera %ls in GLTF file %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(m_basePath));
        return nullptr;
    }

    QJsonObject jsonObj = jsonVal.toObject();
    QString camTy = jsonObj.value(KEY_TYPE).toString();

    if (camTy == QLatin1String("perspective")) {
        const auto pVal = jsonObj.value(KEY_PERSPECTIVE);
        if (Q_UNLIKELY(pVal.isUndefined())) {
            qCWarning(GLTFIOLog, "camera: %ls missing 'perspective' object",
                      qUtf16PrintableImpl(id));
            return nullptr;
        }

        const QJsonObject pObj = pVal.toObject();
        double aspectRatio = pObj.value(KEY_ASPECT_RATIO).toDouble();
        double yfov = pObj.value(KEY_YFOV).toDouble();
        double frustumNear = pObj.value(KEY_ZNEAR).toDouble();
        double frustumFar = pObj.value(KEY_ZFAR).toDouble();

        QCameraLens* result = new QCameraLens;
        result->setPerspectiveProjection(yfov, aspectRatio, frustumNear, frustumFar);
        return result;
    } else if (camTy == QLatin1String("orthographic")) {
        qCWarning(GLTFIOLog, "implement me");

        return nullptr;
    } else {
        qCWarning(GLTFIOLog, "camera: %ls has unsupported type: %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(camTy));
        return nullptr;
    }
}


void GLTFIO::parse()
{
    if (m_parseDone)
        return;

    const QJsonObject buffers = m_json.object().value(KEY_BUFFERS).toObject();
    for (auto it = buffers.begin(), end = buffers.end(); it != end; ++it)
        processJSONBuffer(it.key(), it.value().toObject());

    const QJsonObject views = m_json.object().value(KEY_BUFFER_VIEWS).toObject();
    loadBufferData();
    for (auto it = views.begin(), end = views.end(); it != end; ++it)
        processJSONBufferView(it.key(), it.value().toObject());
    unloadBufferData();

    const QJsonObject shaders = m_json.object().value(KEY_SHADERS).toObject();
    for (auto it = shaders.begin(), end = shaders.end(); it != end; ++it)
        processJSONShader(it.key(), it.value().toObject());

    const QJsonObject programs = m_json.object().value(KEY_PROGRAMS).toObject();
    for (auto it = programs.begin(), end = programs.end(); it != end; ++it)
        processJSONProgram(it.key(), it.value().toObject());

    const QJsonObject techniques = m_json.object().value(KEY_TECHNIQUES).toObject();
    for (auto it = techniques.begin(), end = techniques.end(); it != end; ++it)
        processJSONTechnique(it.key(), it.value().toObject());

    const QJsonObject attrs = m_json.object().value(KEY_ACCESSORS).toObject();
    for (auto it = attrs.begin(), end = attrs.end(); it != end; ++it)
        processJSONAccessor(it.key(), it.value().toObject());

    const QJsonObject meshes = m_json.object().value(KEY_MESHES).toObject();
    for (auto it = meshes.begin(), end = meshes.end(); it != end; ++it)
        processJSONMesh(it.key(), it.value().toObject());

    const QJsonObject images = m_json.object().value(KEY_IMAGES).toObject();
    for (auto it = images.begin(), end = images.end(); it != end; ++it)
        processJSONImage(it.key(), it.value().toObject());

    const QJsonObject textures = m_json.object().value(KEY_TEXTURES).toObject();
    for (auto it = textures.begin(), end = textures.end(); it != end; ++it)
        processJSONTexture(it.key(), it.value().toObject());

    m_defaultScene = m_json.object().value(KEY_SCENE).toString();
    m_parseDone = true;
}

namespace {
template <typename C>
void delete_if_without_parent(const C &c)
{
    for (const auto *e : c) {
        if (!e->parent())
            delete e;
    }
}
} // unnamed namespace

void GLTFIO::cleanup()
{
    m_meshDict.clear();
    m_meshMaterialDict.clear();
    m_accessorDict.clear();
    delete_if_without_parent(m_materialCache);
    m_materialCache.clear();
    m_bufferDatas.clear();
    m_buffers.clear();
    m_shaderPaths.clear();
    delete_if_without_parent(m_programs);
    m_programs.clear();
    delete_if_without_parent(m_techniques);
    m_techniques.clear();
    delete_if_without_parent(m_textures);
    m_textures.clear();
    m_imagePaths.clear();
    m_defaultScene.clear();
    m_parameterDataDict.clear();
}

void GLTFIO::processJSONBuffer(const QString &id, const QJsonObject& json)
{
    // simply cache buffers for lookup by buffer-views
    m_bufferDatas[id] = BufferData(json);
}

void GLTFIO::processJSONBufferView(const QString &id, const QJsonObject& json)
{
    QString bufName = json.value(KEY_BUFFER).toString();
    const auto it = qAsConst(m_bufferDatas).find(bufName);
    if (Q_UNLIKELY(it == m_bufferDatas.cend())) {
        qCWarning(GLTFIOLog, "unknown buffer: %ls processing view: %ls",
                  qUtf16PrintableImpl(bufName), qUtf16PrintableImpl(id));
        return;
    }
    const auto &bufferData = *it;

    int target = json.value(KEY_TARGET).toInt();
    Qt3DRender::QBuffer::BufferType ty(Qt3DRender::QBuffer::VertexBuffer);

    switch (target) {
    case GL_ARRAY_BUFFER:           ty = Qt3DRender::QBuffer::VertexBuffer; break;
    case GL_ELEMENT_ARRAY_BUFFER:   ty = Qt3DRender::QBuffer::IndexBuffer; break;
    default:
        qCWarning(GLTFIOLog, "buffer %ls unsupported target: %d",
                  qUtf16PrintableImpl(id), target);
        return;
    }

    quint64 offset = 0;
    const auto byteOffset = json.value(KEY_BYTE_OFFSET);
    if (!byteOffset.isUndefined()) {
        offset = byteOffset.toInt();
        qCDebug(GLTFIOLog, "bv: %ls has offset: %lld", qUtf16PrintableImpl(id), offset);
    }

    quint64 len = json.value(KEY_BYTE_LENGTH).toInt();

    QByteArray bytes = bufferData.data->mid(offset, len);
    if (Q_UNLIKELY(bytes.count() != int(len))) {
        qCWarning(GLTFIOLog, "failed to read sufficient bytes from: %ls for view %ls",
                  qUtf16PrintableImpl(bufferData.path), qUtf16PrintableImpl(id));
    }

    Qt3DRender::QBuffer *b(new Qt3DRender::QBuffer(ty));
    b->setData(bytes);
    m_buffers[id] = b;
}

void GLTFIO::processJSONShader(const QString &id, const QJsonObject &jsonObject)
{
    // shaders are trivial for the moment, defer the real work
    // to the program section
    QString path = jsonObject.value(KEY_URI).toString();

    QFileInfo info(m_basePath, path);
    if (Q_UNLIKELY(!info.exists())) {
        qCWarning(GLTFIOLog, "can't find shader %ls from path %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(path));
        return;
    }

    m_shaderPaths[id] = info.absoluteFilePath();
}

void GLTFIO::processJSONProgram(const QString &id, const QJsonObject &jsonObject)
{
    QString fragName = jsonObject.value(KEY_FRAGMENT_SHADER).toString(),
            vertName = jsonObject.value(KEY_VERTEX_SHADER).toString();
    const auto fragIt = qAsConst(m_shaderPaths).find(fragName),
               vertIt = qAsConst(m_shaderPaths).find(vertName);
    if (Q_UNLIKELY(fragIt == m_shaderPaths.cend() || vertIt == m_shaderPaths.cend())) {
        qCWarning(GLTFIOLog, "program: %ls missing shader: %ls %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(fragName), qUtf16PrintableImpl(vertName));
        return;
    }

    QShaderProgram* prog = new QShaderProgram;
    prog->setObjectName(id);
    prog->setFragmentShaderCode(QShaderProgram::loadSource(QUrl::fromLocalFile(fragIt.value())));
    prog->setVertexShaderCode(QShaderProgram::loadSource(QUrl::fromLocalFile(vertIt.value())));
    m_programs[id] = prog;
}

void GLTFIO::processJSONTechnique(const QString &id, const QJsonObject &jsonObject )
{
    QTechnique *t = new QTechnique;
    t->setObjectName(id);

    // Parameters
    QHash<QString, QParameter*> paramDict;
    const QJsonObject params = jsonObject.value(KEY_PARAMETERS).toObject();
    for (auto it = params.begin(), end = params.end(); it != end; ++it) {
        const QString pname = it.key();
        const QJsonObject po = it.value().toObject();

        //QString semantic = po.value(KEY_SEMANTIC).toString();
        QParameter *p = new QParameter(t);
        p->setName(pname);
        m_parameterDataDict.insert(p, ParameterData(po));

        //If the parameter has default value, set it
        QJsonValue value = po.value(KEY_VALUE);
        if (!value.isUndefined()) {
            int dataType = po.value(KEY_TYPE).toInt();
            p->setValue(parameterValueFromJSON(dataType, value));
        }

        t->addParameter(p);

        paramDict[pname] = p;
    } // of parameters iteration

    // Program
    QRenderPass* pass = new QRenderPass;
    QString programName = jsonObject.value(KEY_PROGRAM).toString();
    const auto progIt = qAsConst(m_programs).find(programName);
    if (Q_UNLIKELY(progIt == m_programs.cend())) {
        qCWarning(GLTFIOLog, "technique %ls: missing program %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(programName));
    } else {
        pass->setShaderProgram(progIt.value());
    }

    // Attributes
    const QJsonObject attrs = jsonObject.value(KEY_ATTRIBUTES).toObject();
    for (auto it = attrs.begin(), end = attrs.end(); it != end; ++it) {
        QString pname = it.value().toString();
        QParameter *parameter = paramDict.value(pname, nullptr);
        QString attributeName = pname;
        if (Q_UNLIKELY(!parameter)) {
            qCWarning(GLTFIOLog, "attribute %ls defined in instanceProgram but not as parameter",
                      qUtf16PrintableImpl(pname));
            continue;
        }
        //Check if the parameter has a standard attribute semantic
        const auto paramDataIt = m_parameterDataDict.find(parameter);
        QString standardAttributeName = standardAttributeNameFromSemantic(paramDataIt->semantic);
        if (!standardAttributeName.isNull()) {
            attributeName = standardAttributeName;
            t->removeParameter(parameter);
            m_parameterDataDict.erase(paramDataIt);
            delete parameter;
        }

    } // of program-instance attributes

    // Uniforms
    const QJsonObject uniforms = jsonObject.value(KEY_UNIFORMS).toObject();
    for (auto it = uniforms.begin(), end = uniforms.end(); it != end; ++it) {
        const QString pname = it.value().toString();
        QParameter *parameter = paramDict.value(pname, nullptr);
        if (Q_UNLIKELY(!parameter)) {
            qCWarning(GLTFIOLog, "uniform %ls defined in instanceProgram but not as parameter",
                      qUtf16PrintableImpl(pname));
            continue;
        }
        //Check if the parameter has a standard uniform semantic
        const auto paramDataIt = m_parameterDataDict.find(parameter);
        if (hasStandardUniformNameFromSemantic(paramDataIt->semantic)) {
            t->removeParameter(parameter);
            m_parameterDataDict.erase(paramDataIt);
            delete parameter;
        }
    } // of program-instance uniforms


    // States
    QJsonObject states = jsonObject.value(KEY_STATES).toObject();

    //Process states to enable
    const QJsonArray enableStatesArray = states.value(KEY_ENABLE).toArray();
    QVector<int> enableStates;
    for (const QJsonValue &enableValue : enableStatesArray)
        enableStates.append(enableValue.toInt());

    //Process the list of state functions
    const QJsonObject functions = states.value(KEY_FUNCTIONS).toObject();
    for (auto it = functions.begin(), end = functions.end(); it != end; ++it) {
        int enableStateType = 0;
        QRenderState *renderState = buildState(it.key(), it.value(), enableStateType);
        if (renderState != nullptr) {
            //Remove the need to set a default state values for enableStateType
            enableStates.removeOne(enableStateType);
            pass->addRenderState(renderState);
        }
    }

    //Create render states with default values for any remaining enable states
    for (int enableState : qAsConst(enableStates)) {
        QRenderState *renderState = buildStateEnable(enableState);
        if (renderState != nullptr)
            pass->addRenderState(renderState);
    }


    t->addRenderPass(pass);

    m_techniques[id] = t;
}

void GLTFIO::processJSONAccessor( const QString &id, const QJsonObject& json )
{
    m_accessorDict[id] = AccessorData(json);
}

void GLTFIO::processJSONMesh(const QString &id, const QJsonObject &json)
{
    const QJsonArray primitivesArray = json.value(KEY_PRIMITIVES).toArray();
    for (const QJsonValue &primitiveValue : primitivesArray) {
        QJsonObject primitiveObject = primitiveValue.toObject();
        int type = primitiveObject.value(KEY_MODE).toInt();
        QString material = primitiveObject.value(KEY_MATERIAL).toString();

        if (Q_UNLIKELY(material.isEmpty())) {
            qCWarning(GLTFIOLog, "malformed primitive on %ls, missing material value %ls",
                      qUtf16PrintableImpl(id), qUtf16PrintableImpl(material));
            continue;
        }

        QGeometryRenderer *geometryRenderer = new QGeometryRenderer;
        QGeometry *meshGeometry = new QGeometry(geometryRenderer);

        //Set Primitive Type
        geometryRenderer->setPrimitiveType(static_cast<QGeometryRenderer::PrimitiveType>(type));

        //Save Material for mesh
        m_meshMaterialDict[geometryRenderer] = material;

        const QJsonObject attrs = primitiveObject.value(KEY_ATTRIBUTES).toObject();
        for (auto it = attrs.begin(), end = attrs.end(); it != end; ++it) {
            QString k = it.value().toString();
            const auto accessorIt = qAsConst(m_accessorDict).find(k);
            if (Q_UNLIKELY(accessorIt == m_accessorDict.cend())) {
                qCWarning(GLTFIOLog, "unknown attribute accessor: %ls on mesh %ls",
                          qUtf16PrintableImpl(k), qUtf16PrintableImpl(id));
                continue;
            }

            const QString attrName = it.key();
            QString attributeName = standardAttributeNameFromSemantic(attrName);
            if (attributeName.isEmpty())
                attributeName = attrName;

            //Get buffer handle for accessor
            Qt3DRender::QBuffer *buffer = m_buffers.value(accessorIt->bufferViewName, nullptr);
            if (Q_UNLIKELY(!buffer)) {
                qCWarning(GLTFIOLog, "unknown buffer-view: %ls processing accessor: %ls",
                          qUtf16PrintableImpl(accessorIt->bufferViewName), qUtf16PrintableImpl(id));
                continue;
            }

            QAttribute *attribute = new QAttribute(buffer,
                                                   attributeName,
                                                   accessorIt->type,
                                                   accessorIt->dataSize,
                                                   accessorIt->count,
                                                   accessorIt->offset,
                                                   accessorIt->stride);
            attribute->setAttributeType(QAttribute::VertexAttribute);
            meshGeometry->addAttribute(attribute);
        }

        const auto indices = primitiveObject.value(KEY_INDICES);
        if (!indices.isUndefined()) {
            QString k = indices.toString();
            const auto accessorIt = qAsConst(m_accessorDict).find(k);
            if (Q_UNLIKELY(accessorIt == m_accessorDict.cend())) {
                qCWarning(GLTFIOLog, "unknown index accessor: %ls on mesh %ls",
                          qUtf16PrintableImpl(k), qUtf16PrintableImpl(id));
            } else {
                //Get buffer handle for accessor
                Qt3DRender::QBuffer *buffer = m_buffers.value(accessorIt->bufferViewName, nullptr);
                if (Q_UNLIKELY(!buffer)) {
                    qCWarning(GLTFIOLog, "unknown buffer-view: %ls processing accessor: %ls",
                              qUtf16PrintableImpl(accessorIt->bufferViewName), qUtf16PrintableImpl(id));
                    continue;
                }

                QAttribute *attribute = new QAttribute(buffer,
                                                       accessorIt->type,
                                                       accessorIt->dataSize,
                                                       accessorIt->count,
                                                       accessorIt->offset,
                                                       accessorIt->stride);
                attribute->setAttributeType(QAttribute::IndexAttribute);
                meshGeometry->addAttribute(attribute);
            }
        } // of has indices

        geometryRenderer->setGeometry(meshGeometry);

        m_meshDict.insert( id, geometryRenderer);
    } // of primitives iteration
}

void GLTFIO::processJSONImage(const QString &id, const QJsonObject &jsonObject)
{
    QString path = jsonObject.value(KEY_URI).toString();
    QFileInfo info(m_basePath, path);
    if (Q_UNLIKELY(!info.exists())) {
        qCWarning(GLTFIOLog, "can't find image %ls from path %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(path));
        return;
    }

    m_imagePaths[id] = info.absoluteFilePath();
}

void GLTFIO::processJSONTexture(const QString &id, const QJsonObject &jsonObject)
{
    int target = jsonObject.value(KEY_TARGET).toInt(GL_TEXTURE_2D);
    //TODO: support other targets that GL_TEXTURE_2D (though the spec doesn't support anything else)
    if (Q_UNLIKELY(target != GL_TEXTURE_2D)) {
        qCWarning(GLTFIOLog, "unsupported texture target: %d", target);
        return;
    }

    QTexture2D* tex = new QTexture2D;

    // TODO: Choose suitable internal format - may vary on OpenGL context type
    //int pixelFormat = jsonObj.value(KEY_FORMAT).toInt(GL_RGBA);
    int internalFormat = jsonObject.value(KEY_INTERNAL_FORMAT).toInt(GL_RGBA);

    tex->setFormat(static_cast<QAbstractTexture::TextureFormat>(internalFormat));

    QString samplerId = jsonObject.value(KEY_SAMPLER).toString();
    QString source = jsonObject.value(KEY_SOURCE).toString();
    const auto imagIt = qAsConst(m_imagePaths).find(source);
    if (Q_UNLIKELY(imagIt == m_imagePaths.cend())) {
        qCWarning(GLTFIOLog, "texture %ls references missing image %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(source));
        return;
    }

    QTextureImage *texImage = new QTextureImage(tex);
    texImage->setSource(QUrl::fromLocalFile(imagIt.value()));
    tex->addTextureImage(texImage);

    const auto samplersDictValue = m_json.object().value(KEY_SAMPLERS).toObject().value(samplerId);
    if (Q_UNLIKELY(samplersDictValue.isUndefined())) {
        qCWarning(GLTFIOLog, "texture %ls references unknown sampler %ls",
                  qUtf16PrintableImpl(id), qUtf16PrintableImpl(samplerId));
        return;
    }

    QJsonObject sampler = samplersDictValue.toObject();

    tex->setWrapMode(QTextureWrapMode(static_cast<QTextureWrapMode::WrapMode>(sampler.value(KEY_WRAP_S).toInt())));
    tex->setMinificationFilter(static_cast<QAbstractTexture::Filter>(sampler.value(KEY_MIN_FILTER).toInt()));
    if (tex->minificationFilter() == QAbstractTexture::NearestMipMapLinear ||
        tex->minificationFilter() == QAbstractTexture::LinearMipMapNearest ||
        tex->minificationFilter() == QAbstractTexture::NearestMipMapNearest ||
        tex->minificationFilter() == QAbstractTexture::LinearMipMapLinear) {

        tex->setGenerateMipMaps(true);
    }
    tex->setMagnificationFilter(static_cast<QAbstractTexture::Filter>(sampler.value(KEY_MAG_FILTER).toInt()));

    m_textures[id] = tex;
}

void GLTFIO::loadBufferData()
{
    for (auto &bufferData : m_bufferDatas) {
        if (!bufferData.data) {
            bufferData.data = new QByteArray(resolveLocalData(bufferData.path));
        }
    }
}

void GLTFIO::unloadBufferData()
{
    for (const auto &bufferData : qAsConst(m_bufferDatas)) {
        QByteArray *data = bufferData.data;
        delete data;
    }
}

QByteArray GLTFIO::resolveLocalData(const QString &path) const
{
    QDir d(m_basePath);
    Q_ASSERT(d.exists());

    QString absPath = d.absoluteFilePath(path);
    QFile f(absPath);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}

QVariant GLTFIO::parameterValueFromJSON(int type, const QJsonValue &value) const
{
    if (value.isBool()) {
        if (type == GL_BOOL)
            return QVariant(static_cast<GLboolean>(value.toBool()));
    } else if (value.isString()) {
        if (type == GL_SAMPLER_2D) {
            //Textures are special because we need to do a lookup to return the
            //QAbstractTexture
            QString textureId = value.toString();
            const auto it = m_textures.find(textureId);
            if (Q_UNLIKELY(it == m_textures.end())) {
                qCWarning(GLTFIOLog, "unknown texture %ls", qUtf16PrintableImpl(textureId));
                return QVariant();
            } else {
                return QVariant::fromValue(it.value());
            }
        }
    } else if (value.isDouble()) {
        switch (type) {
        case GL_BYTE:
            return QVariant(static_cast<GLbyte>(value.toInt()));
        case GL_UNSIGNED_BYTE:
            return QVariant(static_cast<GLubyte>(value.toInt()));
        case GL_SHORT:
            return QVariant(static_cast<GLshort>(value.toInt()));
        case GL_UNSIGNED_SHORT:
            return QVariant(static_cast<GLushort>(value.toInt()));
        case GL_INT:
            return QVariant(static_cast<GLint>(value.toInt()));
        case GL_UNSIGNED_INT:
            return QVariant(static_cast<GLuint>(value.toInt()));
        case GL_FLOAT:
            return QVariant(static_cast<GLfloat>(value.toDouble()));
        }
    } else if (value.isArray()) {

        const QJsonArray valueArray = value.toArray();

        QVector2D vector2D;
        QVector3D vector3D;
        QVector4D vector4D;
        QVector<float> dataMat2(4, 0.0f);
        QVector<float> dataMat3(9, 0.0f);

        switch (type) {
        case GL_BYTE:
            return QVariant(static_cast<GLbyte>(valueArray.first().toInt()));
        case GL_UNSIGNED_BYTE:
            return QVariant(static_cast<GLubyte>(valueArray.first().toInt()));
        case GL_SHORT:
            return QVariant(static_cast<GLshort>(valueArray.first().toInt()));
        case GL_UNSIGNED_SHORT:
            return QVariant(static_cast<GLushort>(valueArray.first().toInt()));
        case GL_INT:
            return QVariant(static_cast<GLint>(valueArray.first().toInt()));
        case GL_UNSIGNED_INT:
            return QVariant(static_cast<GLuint>(valueArray.first().toInt()));
        case GL_FLOAT:
            return QVariant(static_cast<GLfloat>(valueArray.first().toDouble()));
        case GL_FLOAT_VEC2:
            vector2D.setX(static_cast<GLfloat>(valueArray.at(0).toDouble()));
            vector2D.setY(static_cast<GLfloat>(valueArray.at(1).toDouble()));
            return QVariant(vector2D);
        case GL_FLOAT_VEC3:
            vector3D.setX(static_cast<GLfloat>(valueArray.at(0).toDouble()));
            vector3D.setY(static_cast<GLfloat>(valueArray.at(1).toDouble()));
            vector3D.setZ(static_cast<GLfloat>(valueArray.at(2).toDouble()));
            return QVariant(vector3D);
        case GL_FLOAT_VEC4:
            vector4D.setX(static_cast<GLfloat>(valueArray.at(0).toDouble()));
            vector4D.setY(static_cast<GLfloat>(valueArray.at(1).toDouble()));
            vector4D.setZ(static_cast<GLfloat>(valueArray.at(2).toDouble()));
            vector4D.setW(static_cast<GLfloat>(valueArray.at(3).toDouble()));
            return QVariant(vector4D);
        case GL_INT_VEC2:
            vector2D.setX(static_cast<GLint>(valueArray.at(0).toInt()));
            vector2D.setY(static_cast<GLint>(valueArray.at(1).toInt()));
            return QVariant(vector2D);
        case GL_INT_VEC3:
            vector3D.setX(static_cast<GLint>(valueArray.at(0).toInt()));
            vector3D.setY(static_cast<GLint>(valueArray.at(1).toInt()));
            vector3D.setZ(static_cast<GLint>(valueArray.at(2).toInt()));
            return QVariant(vector3D);
        case GL_INT_VEC4:
            vector4D.setX(static_cast<GLint>(valueArray.at(0).toInt()));
            vector4D.setY(static_cast<GLint>(valueArray.at(1).toInt()));
            vector4D.setZ(static_cast<GLint>(valueArray.at(2).toInt()));
            vector4D.setW(static_cast<GLint>(valueArray.at(3).toInt()));
            return QVariant(vector4D);
        case GL_BOOL:
            return QVariant(static_cast<GLboolean>(valueArray.first().toBool()));
        case GL_BOOL_VEC2:
            vector2D.setX(static_cast<GLboolean>(valueArray.at(0).toBool()));
            vector2D.setY(static_cast<GLboolean>(valueArray.at(1).toBool()));
            return QVariant(vector2D);
        case GL_BOOL_VEC3:
            vector3D.setX(static_cast<GLboolean>(valueArray.at(0).toBool()));
            vector3D.setY(static_cast<GLboolean>(valueArray.at(1).toBool()));
            vector3D.setZ(static_cast<GLboolean>(valueArray.at(2).toBool()));
            return QVariant(vector3D);
        case GL_BOOL_VEC4:
            vector4D.setX(static_cast<GLboolean>(valueArray.at(0).toBool()));
            vector4D.setY(static_cast<GLboolean>(valueArray.at(1).toBool()));
            vector4D.setZ(static_cast<GLboolean>(valueArray.at(2).toBool()));
            vector4D.setW(static_cast<GLboolean>(valueArray.at(3).toBool()));
            return QVariant(vector4D);
        case GL_FLOAT_MAT2:
            //Matrix2x2 is in Row Major ordering (so we need to convert)
            dataMat2[0] = static_cast<GLfloat>(valueArray.at(0).toDouble());
            dataMat2[1] = static_cast<GLfloat>(valueArray.at(2).toDouble());
            dataMat2[2] = static_cast<GLfloat>(valueArray.at(1).toDouble());
            dataMat2[3] = static_cast<GLfloat>(valueArray.at(3).toDouble());
            return QVariant::fromValue(QMatrix2x2(dataMat2.constData()));
        case GL_FLOAT_MAT3:
            //Matrix3x3 is in Row Major ordering (so we need to convert)
            dataMat3[0] = static_cast<GLfloat>(valueArray.at(0).toDouble());
            dataMat3[1] = static_cast<GLfloat>(valueArray.at(3).toDouble());
            dataMat3[2] = static_cast<GLfloat>(valueArray.at(6).toDouble());
            dataMat3[3] = static_cast<GLfloat>(valueArray.at(1).toDouble());
            dataMat3[4] = static_cast<GLfloat>(valueArray.at(4).toDouble());
            dataMat3[5] = static_cast<GLfloat>(valueArray.at(7).toDouble());
            dataMat3[6] = static_cast<GLfloat>(valueArray.at(2).toDouble());
            dataMat3[7] = static_cast<GLfloat>(valueArray.at(5).toDouble());
            dataMat3[8] = static_cast<GLfloat>(valueArray.at(8).toDouble());
            return QVariant::fromValue(QMatrix3x3(dataMat3.constData()));
        case GL_FLOAT_MAT4:
            //Matrix4x4 is Column Major ordering
            return QVariant(QMatrix4x4(static_cast<GLfloat>(valueArray.at(0).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(1).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(2).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(3).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(4).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(5).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(6).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(7).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(8).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(9).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(10).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(11).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(12).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(13).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(14).toDouble()),
                                       static_cast<GLfloat>(valueArray.at(15).toDouble())));
        case GL_SAMPLER_2D:
            return QVariant(valueArray.at(0).toString());
        }
    }
    return QVariant();
}

QAttribute::VertexBaseType GLTFIO::accessorTypeFromJSON(int componentType)
{
    if (componentType == GL_BYTE) {
        return QAttribute::Byte;
    } else if (componentType == GL_UNSIGNED_BYTE) {
        return QAttribute::UnsignedByte;
    } else if (componentType == GL_SHORT) {
        return QAttribute::Short;
    } else if (componentType == GL_UNSIGNED_SHORT) {
        return QAttribute::UnsignedShort;
    } else if (componentType == GL_UNSIGNED_INT) {
        return QAttribute::UnsignedInt;
    } else if (componentType == GL_FLOAT) {
        return QAttribute::Float;
    }

    //There shouldn't be an invalid case here
    qCWarning(GLTFIOLog, "unsupported accessor type %d", componentType);
    return QAttribute::Float;
}

uint GLTFIO::accessorDataSizeFromJson(const QString &type)
{
    QString typeName = type.toUpper();
    if (typeName == QLatin1String("SCALAR"))
        return 1;
    if (typeName == QLatin1String("VEC2"))
        return 2;
    if (typeName == QLatin1String("VEC3"))
        return 3;
    if (typeName == QLatin1String("VEC4"))
        return 4;
    if (typeName == QLatin1String("MAT2"))
        return 4;
    if (typeName == QLatin1String("MAT3"))
        return 9;
    if (typeName == QLatin1String("MAT4"))
        return 16;

    return 0;
}

QRenderState *GLTFIO::buildStateEnable(int state)
{
    int type = 0;
    //By calling buildState with QJsonValue(), a Render State with
    //default values is created.

    if (state == GL_BLEND) {
        //It doesn't make sense to handle this state alone
        return nullptr;
    }

    if (state == GL_CULL_FACE) {
        return buildState(QStringLiteral("cullFace"), QJsonValue(), type);
    }

    if (state == GL_DEPTH_TEST) {
        return buildState(QStringLiteral("depthFunc"), QJsonValue(), type);
    }

    if (state == GL_POLYGON_OFFSET_FILL) {
        return buildState(QStringLiteral("polygonOffset"), QJsonValue(), type);
    }

    if (state == GL_SAMPLE_ALPHA_TO_COVERAGE) {
        return new QAlphaCoverage();
    }

    if (state == GL_SCISSOR_TEST) {
        return buildState(QStringLiteral("scissor"), QJsonValue(), type);
    }

    qCWarning(GLTFIOLog, "unsupported render state: %d", state);

    return nullptr;
}

QRenderState* GLTFIO::buildState(const QString& functionName, const QJsonValue &value, int &type)
{
    type = -1;
    QJsonArray values = value.toArray();

    if (functionName == QLatin1String("blendColor")) {
        type = GL_BLEND;
        //TODO: support render state blendColor
        qCWarning(GLTFIOLog, "unsupported render state: %ls", qUtf16PrintableImpl(functionName));
        return nullptr;
    }

    if (functionName == QLatin1String("blendEquationSeparate")) {
        type = GL_BLEND;
        //TODO: support settings blendEquation alpha
        QBlendEquation *blendEquation = new QBlendEquation;
        blendEquation->setBlendFunction((QBlendEquation::BlendFunction)values.at(0).toInt(GL_FUNC_ADD));
        return blendEquation;
    }

    if (functionName == QLatin1String("blendFuncSeparate")) {
        type = GL_BLEND;
        QBlendEquationArguments *blendArgs = new QBlendEquationArguments;
        blendArgs->setSourceRgb((QBlendEquationArguments::Blending)values.at(0).toInt(GL_ONE));
        blendArgs->setSourceAlpha((QBlendEquationArguments::Blending)values.at(1).toInt(GL_ONE));
        blendArgs->setDestinationRgb((QBlendEquationArguments::Blending)values.at(2).toInt(GL_ZERO));
        blendArgs->setDestinationAlpha((QBlendEquationArguments::Blending)values.at(3).toInt(GL_ZERO));
        return blendArgs;
    }

    if (functionName == QLatin1String("colorMask")) {
        QColorMask *colorMask = new QColorMask;
        colorMask->setRedMasked(values.at(0).toBool(true));
        colorMask->setGreenMasked(values.at(1).toBool(true));
        colorMask->setBlueMasked(values.at(2).toBool(true));
        colorMask->setAlphaMasked(values.at(3).toBool(true));
        return colorMask;
    }

    if (functionName == QLatin1String("cullFace")) {
        type = GL_CULL_FACE;
        QCullFace *cullFace = new QCullFace;
        cullFace->setMode((QCullFace::CullingMode)values.at(0).toInt(GL_BACK));
        return cullFace;
    }

    if (functionName == QLatin1String("depthFunc")) {
        type = GL_DEPTH_TEST;
        QDepthTest *depthTest = new QDepthTest;
        depthTest->setDepthFunction((QDepthTest::DepthFunction)values.at(0).toInt(GL_LESS));
        return depthTest;
    }

    if (functionName == QLatin1String("depthMask")) {
        if (!values.at(0).toBool(true)) {
            QNoDepthMask *depthMask = new QNoDepthMask;
            return depthMask;
        }
        return nullptr;
    }

    if (functionName == QLatin1String("depthRange")) {
        //TODO: support render state depthRange
        qCWarning(GLTFIOLog, "unsupported render state: %ls", qUtf16PrintableImpl(functionName));
        return nullptr;
    }

    if (functionName == QLatin1String("frontFace")) {
        QFrontFace *frontFace = new QFrontFace;
        frontFace->setDirection((QFrontFace::WindingDirection)values.at(0).toInt(GL_CCW));
        return frontFace;
    }

    if (functionName == QLatin1String("lineWidth")) {
        //TODO: support render state lineWidth
        qCWarning(GLTFIOLog, "unsupported render state: %ls", qUtf16PrintableImpl(functionName));
        return nullptr;
    }

    if (functionName == QLatin1String("polygonOffset")) {
        type = GL_POLYGON_OFFSET_FILL;
        QPolygonOffset *polygonOffset = new QPolygonOffset;
        polygonOffset->setScaleFactor((float)values.at(0).toDouble(0.0f));
        polygonOffset->setDepthSteps((float)values.at(1).toDouble(0.0f));
        return polygonOffset;
    }

    if (functionName == QLatin1String("scissor")) {
        type = GL_SCISSOR_TEST;
        QScissorTest *scissorTest = new QScissorTest;
        scissorTest->setLeft(values.at(0).toDouble(0.0f));
        scissorTest->setBottom(values.at(1).toDouble(0.0f));
        scissorTest->setWidth(values.at(2).toDouble(0.0f));
        scissorTest->setHeight(values.at(3).toDouble(0.0f));
        return scissorTest;
    }

    qCWarning(GLTFIOLog, "unsupported render state: %ls", qUtf16PrintableImpl(functionName));
    return nullptr;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "moc_gltfio.cpp"
