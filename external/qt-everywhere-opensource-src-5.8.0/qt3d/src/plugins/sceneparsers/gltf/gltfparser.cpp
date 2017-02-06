/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "gltfparser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtGui/QVector2D>

#include <Qt3DCore/QCameraLens>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <Qt3DRender/private/qurlhelper_p.h>

#include <Qt3DRender/QAlphaCoverage>
#include <Qt3DRender/QBlendEquation>
#include <Qt3DRender/QBlendStateSeparate>
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QDepthMask>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QFrontFace>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QParameterMapping>
#include <Qt3DRender/QPolygonOffset>
#include <Qt3DRender/QRenderState>
#include <Qt3DRender/QScissorTest>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTexture>

#include <Qt3DRender/QPhongMaterial>
#include <Qt3DRender/QDiffuseMapMaterial>
#include <Qt3DRender/QDiffuseSpecularMapMaterial>
#include <Qt3DRender/QNormalDiffuseMapMaterial>
#include <Qt3DRender/QNormalDiffuseSpecularMapMaterial>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

Q_LOGGING_CATEGORY(GLTFParserLog, "Qt3D.GLTFParser")

namespace {

const QString KEY_CAMERA     = QStringLiteral("camera");
const QString KEY_CAMERAS    = QStringLiteral("cameras");
const QString KEY_SCENES     = QStringLiteral("scenes");
const QString KEY_NODES      = QStringLiteral("nodes");
const QString KEY_MESHES     = QStringLiteral("meshes");
const QString KEY_CHILDREN   = QStringLiteral("children");
const QString KEY_MATRIX     = QStringLiteral("matrix");
const QString KEY_ROTATION   = QStringLiteral("rotation");
const QString KEY_SCALE      = QStringLiteral("scale");
const QString KEY_TRANSLATION = QStringLiteral("translation");
const QString KEY_TYPE       = QStringLiteral("type");
const QString KEY_PERSPECTIVE =QStringLiteral("perspective");
const QString KEY_NAME       = QStringLiteral("name");
const QString KEY_COUNT      = QStringLiteral("count");
const QString KEY_YFOV       = QStringLiteral("yfov");
const QString KEY_ZNEAR      = QStringLiteral("znear");
const QString KEY_ZFAR       = QStringLiteral("zfar");
const QString KEY_MATERIALS  = QStringLiteral("materials");
const QString KEY_EXTENSIONS = QStringLiteral("extensions");
const QString KEY_COMMON_MAT = QStringLiteral("KHR_materials_common");
const QString KEY_TECHNIQUE  = QStringLiteral("technique");
const QString KEY_VALUES     = QStringLiteral("values");
const QString KEY_BUFFERS    = QStringLiteral("buffers");
const QString KEY_SHADERS    = QStringLiteral("shaders");
const QString KEY_PROGRAMS   = QStringLiteral("programs");
const QString KEY_PROGRAM    = QStringLiteral("program");
const QString KEY_TECHNIQUES = QStringLiteral("techniques");
const QString KEY_ACCESSORS  = QStringLiteral("accessors");
const QString KEY_IMAGES     = QStringLiteral("images");
const QString KEY_TEXTURES   = QStringLiteral("textures");
const QString KEY_SCENE      = QStringLiteral("scene");
const QString KEY_BUFFER     = QStringLiteral("buffer");
const QString KEY_TARGET     = QStringLiteral("target");
const QString KEY_BYTE_OFFSET = QStringLiteral("byteOffset");
const QString KEY_BYTE_LENGTH = QStringLiteral("byteLength");
const QString KEY_BYTE_STRIDE = QStringLiteral("byteStride");
const QString KEY_PRIMITIVES = QStringLiteral("primitives");
const QString KEY_MODE       = QStringLiteral("mode");
const QString KEY_MATERIAL   = QStringLiteral("material");
const QString KEY_ATTRIBUTES = QStringLiteral("attributes");
const QString KEY_INDICES    = QStringLiteral("indices");
const QString KEY_URI       = QStringLiteral("uri");
const QString KEY_FORMAT     = QStringLiteral("format");
const QString KEY_PASSES     = QStringLiteral("passes");
const QString KEY_SOURCE     = QStringLiteral("source");
const QString KEY_SAMPLER    = QStringLiteral("sampler");
const QString KEY_SAMPLERS   = QStringLiteral("samplers");
const QString KEY_SEMANTIC   = QStringLiteral("semantic");
const QString KEY_STATES     = QStringLiteral("states");
const QString KEY_UNIFORMS   = QStringLiteral("uniforms");
const QString KEY_PARAMETERS = QStringLiteral("parameters");
const QString KEY_WRAP_S     = QStringLiteral("wrapS");
const QString KEY_MIN_FILTER = QStringLiteral("minFilter");
const QString KEY_MAG_FILTER = QStringLiteral("magFilter");

const QString KEY_INSTANCE_TECHNIQUE = QStringLiteral("instanceTechnique");
const QString KEY_INSTANCE_PROGRAM = QStringLiteral("instanceProgram");
const QString KEY_BUFFER_VIEWS    = QStringLiteral("bufferViews");
const QString KEY_BUFFER_VIEW     = QStringLiteral("bufferView");
const QString KEY_VERTEX_SHADER   = QStringLiteral("vertexShader");
const QString KEY_FRAGMENT_SHADER = QStringLiteral("fragmentShader");
const QString KEY_INTERNAL_FORMAT = QStringLiteral("internalFormat");
const QString KEY_COMPONENT_TYPE  = QStringLiteral("componentType");
const QString KEY_ASPECT_RATIO    = QStringLiteral("aspect_ratio");
const QString KEY_VALUE           = QStringLiteral("value");
const QString KEY_ENABLE          = QStringLiteral("enable");
const QString KEY_FUNCTIONS       = QStringLiteral("functions");
const QString KEY_TECHNIQUE_CORE  = QStringLiteral("techniqueCore");
const QString KEY_TECHNIQUE_GL2   = QStringLiteral("techniqueGL2");

} // of anonymous namespace

GLTFParser::GLTFParser() : QAbstractSceneParser(),
    m_parseDone(false)
{
}

GLTFParser::~GLTFParser()
{

}

void GLTFParser::setBasePath(const QString& path)
{
    m_basePath = path;
}

bool GLTFParser::setJSON(const QJsonDocument &json )
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
void GLTFParser::setSource(const QUrl &source)
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    QFile f(path);
    if (Q_UNLIKELY(!f.open(QIODevice::ReadOnly))) {
        qCWarning(GLTFParserLog) << "cannot open " << path << ": " << f.errorString();
        return;
    }

    QByteArray jsonData = f.readAll();
    QJsonDocument sceneDocument = QJsonDocument::fromBinaryData(jsonData);
    if (sceneDocument.isNull())
        sceneDocument = QJsonDocument::fromJson(jsonData);

    if (!setJSON(sceneDocument)) {
        qCWarning(GLTFParserLog) << "not a JSON document";
        return;
    }

    setBasePath(QFileInfo(path).dir().absolutePath());
}

/*!
 * Returns true if the extension of \a path is supported by the
 * GLTF parser.
 */
bool GLTFParser::isExtensionSupported(const QUrl &source) const
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    return GLTFParser::isGLTFPath(path);
}

Qt3DCore::QEntity* GLTFParser::node(const QString &id)
{
    QJsonObject nodes = m_json.object().value(KEY_NODES).toObject();
    if (!nodes.contains(id)) {
        qCWarning(GLTFParserLog) << "unknown node" << id << "in GLTF file" << m_basePath;
        return NULL;
    }

    QJsonObject jsonObj = nodes.value(id).toObject();
    QEntity* result = Q_NULLPTR;

    // Qt3D has a limitation that a QEntity can only have 1 mesh and 1 material component
    // So if the node has only 1 mesh, we only create 1 QEntity
    // Otherwise if there are n meshes, there is 1 QEntity, with n children for each mesh/material combo
    if (jsonObj.contains(KEY_MESHES)) {
        QVector<QEntity *> entities;

        Q_FOREACH (QJsonValue mesh, jsonObj.value(KEY_MESHES).toArray()) {
            if (!m_meshDict.contains(mesh.toString())) {
                qCWarning(GLTFParserLog) << "node" << id << "references unknown mesh" << mesh.toString();
                continue;
            }

            Q_FOREACH (QGeometryRenderer *geometryRenderer, m_meshDict.values(mesh.toString())) {
                QEntity *entity = new QEntity;
                entity->addComponent(geometryRenderer);
                QMaterial *mat = material(m_meshMaterialDict[geometryRenderer]);
                if (mat)
                    entity->addComponent(mat);
                entities.append(entity);
            }

        }

        if (entities.count() == 1) {
            result = entities.first();
        } else {
            result = new QEntity;
            Q_FOREACH (QEntity *entity, entities) {
                entity->setParent(result);
            }
        }
    }

    //If the entity contains no meshes, results will still be null here
    if (result == Q_NULLPTR)
        result = new QEntity;

    if ( jsonObj.contains(KEY_CHILDREN) ) {
        Q_FOREACH (QJsonValue c, jsonObj.value(KEY_CHILDREN).toArray()) {
            QEntity* child = node(c.toString());
            if (!child)
                continue;
            child->setParent(result);
        }
    }

    renameFromJson(jsonObj, result);


    // Node Transforms
    Qt3DCore::QTransform *trans = Q_NULLPTR;
    if ( jsonObj.contains(KEY_MATRIX) ) {
        QMatrix4x4 m(Qt::Uninitialized);

        QJsonArray matrixValues = jsonObj.value(KEY_MATRIX).toArray();
        for (int i=0; i<16; ++i) {
            double v = matrixValues.at( i ).toDouble();
            m(i % 4, i >> 2) = v;
        }

        // ADD MATRIX TRANSFORM COMPONENT TO ENTITY
        if (trans == Q_NULLPTR)
            trans = new Qt3DCore::QTransform;
        trans->setMatrix(m);
    }

    // Rotation quaternion
    if (jsonObj.contains(KEY_ROTATION)) {
        if (trans == Q_NULLPTR)
            trans = new Qt3DCore::QTransform;

        QJsonArray quaternionValues = jsonObj.value(KEY_ROTATION).toArray();
        QQuaternion quaternion(quaternionValues[0].toDouble(),
                               quaternionValues[1].toDouble(),
                               quaternionValues[2].toDouble(),
                               quaternionValues[3].toDouble());
        trans->setRotation(quaternion);
    }

    // Translation
    if (jsonObj.contains(KEY_TRANSLATION)) {
        if (trans == Q_NULLPTR)
            trans = new Qt3DCore::QTransform;

        QJsonArray translationValues = jsonObj.value(KEY_TRANSLATION).toArray();
        trans->setTranslation(QVector3D(translationValues[0].toDouble(),
                                        translationValues[1].toDouble(),
                                        translationValues[2].toDouble()));
    }

    // Scale
    if (jsonObj.contains(KEY_SCALE)) {
        if (trans == Q_NULLPTR)
            trans = new Qt3DCore::QTransform;

        QJsonArray scaleValues = jsonObj.value(KEY_SCALE).toArray();
        trans->setScale3D(QVector3D(scaleValues[0].toDouble(),
                                    scaleValues[1].toDouble(),
                                    scaleValues[2].toDouble()));
    }

    // Add the Transform component
    if (trans != Q_NULLPTR)
        result->addComponent(trans);

    if ( jsonObj.contains(KEY_CAMERA) ) {
        QCameraLens* cam = camera( jsonObj.value(KEY_CAMERA).toString() );
        if (!cam) {
            qCWarning(GLTFParserLog) << "failed to build camera:" << jsonObj.value(KEY_CAMERA)
                                     << "on node" << id;
        } else {
            result->addComponent(cam);
        }
    } // of have camera attribute

    return result;
}

Qt3DCore::QEntity* GLTFParser::scene(const QString &id)
{
    parse();

    QJsonObject scenes = m_json.object().value(KEY_SCENES).toObject();
    if (!scenes.contains(id)) {
        if (!id.isNull())
            qCWarning(GLTFParserLog) << "GLTF: no such scene" << id << "in file" << m_basePath;
        return defaultScene();
    }

    QJsonObject sceneObj = scenes.value(id).toObject();
    QEntity* sceneEntity = new QEntity;
    Q_FOREACH (QJsonValue nnv, sceneObj.value(KEY_NODES).toArray()) {
        QString nodeName = nnv.toString();
        QEntity* child = node(nodeName);
        if (!child)
            continue;
        child->setParent(sceneEntity);
    }

    return sceneEntity;
}

GLTFParser::BufferData::BufferData()
    : length(0)
    , data(Q_NULLPTR)
{
}

GLTFParser::BufferData::BufferData(QJsonObject json)
{
    path = json.value(KEY_URI).toString();
    length = json.value(KEY_BYTE_LENGTH).toInt();
    data = Q_NULLPTR;
}

GLTFParser::ParameterData::ParameterData() :
    type(0)
{

}

GLTFParser::ParameterData::ParameterData(QJsonObject json)
{
    type = json.value(KEY_TYPE).toInt();
    semantic = json.value(KEY_SEMANTIC).toString();
}

GLTFParser::AccessorData::AccessorData()
    : type(QAttribute::Float)
    , dataSize(0)
    , count(0)
    , offset(0)
    , stride(0)
{

}

GLTFParser::AccessorData::AccessorData(const QJsonObject &json)
{
    bufferViewName = json.value(KEY_BUFFER_VIEW).toString();
    offset = 0;
    stride = 0;
    int componentType = json.value(KEY_COMPONENT_TYPE).toInt();
    type = accessorTypeFromJSON(componentType);
    count = json.value(KEY_COUNT).toInt();
    dataSize = accessorDataSizeFromJson(json.value(KEY_TYPE).toString());

    if ( json.contains(KEY_BYTE_OFFSET))
        offset = json.value(KEY_BYTE_OFFSET).toInt();
    if ( json.contains(KEY_BYTE_STRIDE))
        stride = json.value(KEY_BYTE_STRIDE).toInt();
}

bool GLTFParser::isGLTFPath(const QString& path)
{
    QFileInfo finfo(path);
    if (!finfo.exists())
        return false;

    // might need to detect other things in the future, but would
    // prefer to avoid doing a full parse.
    QString suffix = finfo.suffix().toLower();
    return (suffix == QStringLiteral("json") || suffix == QStringLiteral("gltf") || suffix == QStringLiteral("qgltf"));
}

void GLTFParser::renameFromJson(const QJsonObject &json, QObject * const object)
{
    if ( json.contains(KEY_NAME) )
        object->setObjectName( json.value(KEY_NAME).toString() );
}

QString GLTFParser::standardUniformNamefromSemantic(const QString &semantic)
{
    //Standard Uniforms
    //if (semantic == QStringLiteral("LOCAL"));
    if (semantic == QStringLiteral("MODEL"))
        return QStringLiteral("modelMatrix");
    if (semantic == QStringLiteral("VIEW"))
        return QStringLiteral("viewMatrix");
    if (semantic == QStringLiteral("PROJECTION"))
        return QStringLiteral("projectionMatrix");
    if (semantic == QStringLiteral("MODELVIEW"))
        return QStringLiteral("modelView");
    if (semantic == QStringLiteral("MODELVIEWPROJECTION"))
        return QStringLiteral("modelViewProjection");
    if (semantic == QStringLiteral("MODELINVERSE"))
        return QStringLiteral("inverseModelMatrix");
    if (semantic == QStringLiteral("VIEWINVERSE"))
        return QStringLiteral("inverViewMatrix");
    if (semantic == QStringLiteral("PROJECTIONINVERSE"))
        return QStringLiteral("inverseProjectionMatrix");
    if (semantic == QStringLiteral("MODELVIEWPROJECTIONINVERSE"))
        return QStringLiteral("inverseModelViewProjection");
    if (semantic == QStringLiteral("MODELINVERSETRANSPOSE"))
        return QStringLiteral("modelNormalMatrix");
    if (semantic == QStringLiteral("MODELVIEWINVERSETRANSPOSE"))
        return QStringLiteral("modelViewNormal");
    if (semantic == QStringLiteral("VIEWPORT"))
        return QStringLiteral("viewportMatrix");

    return QString();
}

QString GLTFParser::standardAttributeNameFromSemantic(const QString &semantic)
{
    //Standard Attributes
    if (semantic.startsWith(QStringLiteral("POSITION")))
        return QAttribute::defaultPositionAttributeName();
    if (semantic.startsWith(QStringLiteral("NORMAL")))
        return QAttribute::defaultNormalAttributeName();
    if (semantic.startsWith(QStringLiteral("TEXCOORD")))
        return QAttribute::defaultTextureCoordinateAttributeName();
    if (semantic.startsWith(QStringLiteral("COLOR")))
        return QAttribute::defaultColorAttributeName();
    if (semantic.startsWith(QStringLiteral("TANGENT")))
        return QAttribute::defaultTangentAttributeName();

//    if (semantic.startsWith(QStringLiteral("JOINT")));
//    if (semantic.startsWith(QStringLiteral("JOINTMATRIX")));
//    if (semantic.startsWith(QStringLiteral("WEIGHT")));

    return QString();
}

QParameter *GLTFParser::parameterFromTechnique(QTechnique *technique, const QString &parameterName)
{
    Q_FOREACH (QParameter *parameter, technique->parameters()) {
        if (parameter->name() == parameterName) {
            return parameter;
        }
    }

    return Q_NULLPTR;
}

Qt3DCore::QEntity* GLTFParser::defaultScene()
{
    if (m_defaultScene.isEmpty()) {
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "no default scene";
        return NULL;
    }

    return scene(m_defaultScene);
}

QMaterial *GLTFParser::materialWithCustomShader(const QString &id, const QJsonObject &jsonObj)
{
    //Default ES2 Technique
    QString techniqueName = jsonObj.value(KEY_TECHNIQUE).toString();
    if (!m_techniques.contains(techniqueName)) {
        qCWarning(GLTFParserLog) << "unknown technique" << techniqueName
                                 << "for material" << id << "in GLTF file" << m_basePath;
        return NULL;
    }
    QTechnique *technique = m_techniques.value(techniqueName);
    technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    technique->graphicsApiFilter()->setMajorVersion(2);
    technique->graphicsApiFilter()->setMinorVersion(0);
    technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);


    //Optional Core technique
    QTechnique *coreTechnique = Q_NULLPTR;
    QTechnique *gl2Technique = Q_NULLPTR;
    QString coreTechniqueName = jsonObj.value(KEY_TECHNIQUE_CORE).toString();
    if (!coreTechniqueName.isNull()) {
        if (!m_techniques.contains(coreTechniqueName)) {
            qCWarning(GLTFParserLog) << "unknown technique" << coreTechniqueName
                                     << "for material" << id << "in GLTF file" << m_basePath;
        } else {
            coreTechnique = m_techniques.value(coreTechniqueName);
            coreTechnique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
            coreTechnique->graphicsApiFilter()->setMajorVersion(3);
            coreTechnique->graphicsApiFilter()->setMinorVersion(1);
            coreTechnique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);
        }
    }
    //Optional GL2 technique
    QString gl2TechniqueName = jsonObj.value(KEY_TECHNIQUE_GL2).toString();
    if (!gl2TechniqueName.isNull()) {
        if (!m_techniques.contains(gl2TechniqueName)) {
            qCWarning(GLTFParserLog) << "unknown technique" << gl2TechniqueName
                                     << "for material" << id << "in GLTF file" << m_basePath;
        } else {
            gl2Technique = m_techniques.value(gl2TechniqueName);
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
    if (coreTechnique != Q_NULLPTR)
        effect->addTechnique(coreTechnique);
    if (gl2Technique != Q_NULLPTR)
        effect->addTechnique(gl2Technique);

    QMaterial* mat = new QMaterial;
    mat->setEffect(effect);

    renameFromJson(jsonObj, mat);

    QJsonObject values = jsonObj.value(KEY_VALUES).toObject();
    Q_FOREACH (QString vName, values.keys()) {
        QParameter *param = parameterFromTechnique(technique, vName);

        if (param == Q_NULLPTR && coreTechnique != Q_NULLPTR) {
            param = parameterFromTechnique(coreTechnique, vName);
        }

        if (param == Q_NULLPTR && gl2Technique != Q_NULLPTR) {
            param = parameterFromTechnique(gl2Technique, vName);
        }

        if (param == Q_NULLPTR) {
            qCWarning(GLTFParserLog) << "unknown parameter:" << vName << "in technique" << techniqueName
                                     << "processing material" << id;
            continue;
        }

        ParameterData paramData = m_parameterDataDict.value(param);
        QVariant var = parameterValueFromJSON(paramData.type, values.value(vName));

        mat->addParameter(new QParameter(param->name(), var));
    } // of material technique-instance values iteration

    return mat;
}

static inline QVariant vec4ToRgb(const QVariant &vec4Var)
{
    const QVector4D v = vec4Var.value<QVector4D>();
    return QVariant(QColor::fromRgbF(v.x(), v.y(), v.z()));
}

QMaterial *GLTFParser::commonMaterial(const QJsonObject &jsonObj)
{
    QVariantHash params;
    bool hasDiffuseMap = false;
    bool hasSpecularMap = false;
    bool hasNormalMap = false;

    QJsonObject values = jsonObj.value(KEY_VALUES).toObject();
    Q_FOREACH (const QString &vName, values.keys()) {
        const QJsonValue val = values.value(vName);
        QVariant var;
        QString propertyName = vName;
        if (vName == QStringLiteral("ambient") && val.isArray()) {
            var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
        } else if (vName == QStringLiteral("diffuse")) {
            if (val.isString()) {
                var = parameterValueFromJSON(GL_SAMPLER_2D, val);
                hasDiffuseMap = true;
            } else if (val.isArray()) {
                var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
            }
        } else if (vName == QStringLiteral("specular")) {
            if (val.isString()) {
                var = parameterValueFromJSON(GL_SAMPLER_2D, val);
                hasSpecularMap = true;
            } else if (val.isArray()) {
                var = vec4ToRgb(parameterValueFromJSON(GL_FLOAT_VEC4, val));
            }
        } else if (vName == QStringLiteral("shininess") && val.isDouble()) {
            var = parameterValueFromJSON(GL_FLOAT, val);
        } else if (vName == QStringLiteral("normalmap") && val.isString()) {
            var = parameterValueFromJSON(GL_SAMPLER_2D, val);
            propertyName = QStringLiteral("normal");
            hasNormalMap = true;
        } else if (vName == QStringLiteral("transparency")) {
            qCWarning(GLTFParserLog) << "Semi-transparent common materials are not currently supported, ignoring alpha";
        }
        if (var.isValid())
            params[propertyName] = var;
    }

    QMaterial *mat = Q_NULLPTR;
    if (hasNormalMap) {
        if (hasSpecularMap) {
            mat = new QNormalDiffuseSpecularMapMaterial;
        } else {
            if (hasDiffuseMap)
                mat = new QNormalDiffuseMapMaterial;
            else
                qCWarning(GLTFParserLog) << "Common material with normal and specular maps needs a diffuse map as well";
        }
    } else {
        if (hasSpecularMap) {
            if (hasDiffuseMap)
                mat = new QDiffuseSpecularMapMaterial;
            else
                qCWarning(GLTFParserLog) << "Common material with specular map needs a diffuse map as well";
        } else if (hasDiffuseMap) {
            mat = new QDiffuseMapMaterial;
        } else {
            mat = new QPhongMaterial;
        }
    }

    if (mat) {
        for (QVariantHash::const_iterator it = params.constBegin(), itEnd = params.constEnd(); it != itEnd; ++it)
            mat->setProperty(it.key().toUtf8(), it.value());
    } else {
        qCWarning(GLTFParserLog) << "Could not find a suitable built-in material for KHR_materials_common";
    }

    return mat;
}

QMaterial* GLTFParser::material(const QString &id)
{
    if (m_materialCache.contains(id))
        return m_materialCache.value(id);

    QJsonObject mats = m_json.object().value(KEY_MATERIALS).toObject();
    if (!mats.contains(id)) {
        qCWarning(GLTFParserLog) << "unknown material" << id << "in GLTF file" << m_basePath;
        return NULL;
    }

    QJsonObject jsonObj = mats.value(id).toObject();

    QMaterial *mat = Q_NULLPTR;

    // Prefer common materials over custom shaders.
    if (jsonObj.contains(KEY_EXTENSIONS)) {
        QJsonObject extensions = jsonObj.value(KEY_EXTENSIONS).toObject();
        if (extensions.contains(KEY_COMMON_MAT))
            mat = commonMaterial(extensions.value(KEY_COMMON_MAT).toObject());
    }

    if (!mat)
        mat = materialWithCustomShader(id, jsonObj);

    m_materialCache[id] = mat;
    return mat;
}

QCameraLens* GLTFParser::camera(const QString &id) const
{
    QJsonObject cams = m_json.object().value(KEY_CAMERAS).toObject();
    if (!cams.contains(id)) {
        qCWarning(GLTFParserLog) << "unknown camera" << id << "in GLTF file" << m_basePath;
        return Q_NULLPTR;
    }

    QJsonObject jsonObj = cams.value(id).toObject();
    QString camTy = jsonObj.value(KEY_TYPE).toString();

    if (camTy == QStringLiteral("perspective")) {
        if (!jsonObj.contains(KEY_PERSPECTIVE)) {
            qCWarning(GLTFParserLog) << "camera:" << id << "missing 'perspective' object";
            return Q_NULLPTR;
        }

        QJsonObject pObj = jsonObj.value(KEY_PERSPECTIVE).toObject();
        double aspectRatio = pObj.value(KEY_ASPECT_RATIO).toDouble();
        double yfov = pObj.value(KEY_YFOV).toDouble();
        double frustumNear = pObj.value(KEY_ZNEAR).toDouble();
        double frustumFar = pObj.value(KEY_ZFAR).toDouble();

        QCameraLens* result = new QCameraLens;
        result->setPerspectiveProjection(yfov, aspectRatio, frustumNear, frustumFar);
        return result;
    } else if (camTy == QStringLiteral("orthographic")) {
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "implement me";

        return Q_NULLPTR;
    } else {
        qCWarning(GLTFParserLog) << "camera:" << id << "has unsupported type:" << camTy;
        return Q_NULLPTR;
    }
}


void GLTFParser::parse()
{
    if (m_parseDone)
        return;

    QJsonObject buffers = m_json.object().value(KEY_BUFFERS).toObject();
    Q_FOREACH (QString nm, buffers.keys()) {
        processJSONBuffer( nm, buffers.value(nm).toObject() );
    }

    QJsonObject views = m_json.object().value(KEY_BUFFER_VIEWS).toObject();
    loadBufferData();
    Q_FOREACH (QString nm, views.keys()) {
        processJSONBufferView( nm, views.value(nm).toObject() );
    }
    unloadBufferData();

    QJsonObject shaders = m_json.object().value(KEY_SHADERS).toObject();
    Q_FOREACH (QString nm, shaders.keys()) {
        processJSONShader( nm, shaders.value(nm).toObject() );
    }

    QJsonObject programs = m_json.object().value(KEY_PROGRAMS).toObject();
    Q_FOREACH (QString nm, programs.keys()) {
        processJSONProgram( nm, programs.value(nm).toObject() );
    }

    QJsonObject techniques = m_json.object().value(KEY_TECHNIQUES).toObject();
    Q_FOREACH (QString nm, techniques.keys()) {
        processJSONTechnique( nm, techniques.value(nm).toObject() );
    }

    QJsonObject attrs = m_json.object().value(KEY_ACCESSORS).toObject();
    Q_FOREACH (QString nm, attrs.keys()) {
        processJSONAccessor( nm, attrs.value(nm).toObject() );
    }

    QJsonObject meshes = m_json.object().value(KEY_MESHES).toObject();
    Q_FOREACH (QString nm, meshes.keys()) {
        processJSONMesh( nm, meshes.value(nm).toObject() );
    }

    QJsonObject images = m_json.object().value(KEY_IMAGES).toObject();
    Q_FOREACH (QString nm, images.keys()) {
        processJSONImage( nm, images.value(nm).toObject() );
    }

    QJsonObject textures = m_json.object().value(KEY_TEXTURES).toObject();
    Q_FOREACH (QString nm, textures.keys()) {
        processJSONTexture(nm, textures.value(nm).toObject() );
    }

    m_defaultScene = m_json.object().value(KEY_SCENE).toString();
    m_parseDone = true;
}

void GLTFParser::cleanup()
{
    m_meshDict.clear();
    m_meshMaterialDict.clear();
    m_accessorDict.clear();
    //Check for Materials with no parent
    Q_FOREACH (QMaterial *material, m_materialCache.values()) {
        if (material->parent() == Q_NULLPTR)
            delete material;
    }
    m_materialCache.clear();
    m_bufferDatas.clear();
    m_buffers.clear();
    m_shaderPaths.clear();
    //Check for ShaderPrograms with no parent
    Q_FOREACH (QShaderProgram *program, m_programs.values()) {
        if (program->parent() == Q_NULLPTR)
            delete program;
    }
    m_programs.clear();
    //Check for Techniques with no parent
    Q_FOREACH (QTechnique *technique, m_techniques.values()) {
        if (technique->parent() == Q_NULLPTR)
            delete technique;
    }
    m_techniques.clear();
    //Check for Textures with no parent
    Q_FOREACH (QAbstractTextureProvider *texture, m_textures.values()) {
        if (texture->parent() == Q_NULLPTR)
            delete texture;
    }
    m_textures.clear();
    m_imagePaths.clear();
    m_defaultScene.clear();
    m_parameterDataDict.clear();
}

void GLTFParser::processJSONBuffer(const QString &id, const QJsonObject& json)
{
    // simply cache buffers for lookup by buffer-views
    m_bufferDatas[id] = BufferData(json);
}

void GLTFParser::processJSONBufferView(const QString &id, const QJsonObject& json)
{
    QString bufName = json.value(KEY_BUFFER).toString();
    if (!m_bufferDatas.contains(bufName)) {
        qCWarning(GLTFParserLog) << "unknown buffer:" << bufName << "processing view:" << id;
        return;
    }

    int target = json.value(KEY_TARGET).toInt();
    QBuffer::BufferType ty(QBuffer::VertexBuffer);

    switch (target) {
    case GL_ARRAY_BUFFER:           ty = QBuffer::VertexBuffer; break;
    case GL_ELEMENT_ARRAY_BUFFER:   ty = QBuffer::IndexBuffer; break;
    default:
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "buffer" << id << "unsupported target:" << target;
        return;
    }

    quint64 offset = 0;
    if (json.contains(KEY_BYTE_OFFSET)) {
        offset = json.value(KEY_BYTE_OFFSET).toInt();
        qCDebug(GLTFParserLog) << "bv:" << id << "has offset:" << offset;
    }

    quint64 len = json.value(KEY_BYTE_LENGTH).toInt();

    QByteArray bytes(m_bufferDatas[bufName].data->mid(offset, len));
    if (bytes.count() != (int) len) {
        qCWarning(GLTFParserLog) << "failed to read sufficient bytes from:" << m_bufferDatas[bufName].path
                                 << "for view" << id;
    }

    QBuffer *b(new QBuffer(ty));
    b->setData(bytes);
    m_buffers[id] = b;
}

void GLTFParser::processJSONShader(const QString &id, const QJsonObject &jsonObject)
{
    // shaders are trivial for the moment, defer the real work
    // to the program section
    QString path = jsonObject.value(KEY_URI).toString();

    QFileInfo info(m_basePath, path);
    if (!info.exists()) {
        qCWarning(GLTFParserLog) << "can't find shader" << id << "from path" << path;
        return;
    }

    m_shaderPaths[id] = info.absoluteFilePath();
}

void GLTFParser::processJSONProgram(const QString &id, const QJsonObject &jsonObject)
{
    QShaderProgram* prog = new QShaderProgram;
    prog->setObjectName(id);

    QString fragName = jsonObject.value(KEY_FRAGMENT_SHADER).toString(),
            vertName = jsonObject.value(KEY_VERTEX_SHADER).toString();
    if (!m_shaderPaths.contains(fragName) || !m_shaderPaths.contains(vertName)) {
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "program:" << id << "missing shader:"
                                 << fragName << vertName;
        return;
    }

    prog->setFragmentShaderCode(QShaderProgram::loadSource(QUrl::fromLocalFile(m_shaderPaths[fragName])));
    prog->setVertexShaderCode(QShaderProgram::loadSource(QUrl::fromLocalFile(m_shaderPaths[vertName])));
    m_programs[id] = prog;
}

void GLTFParser::processJSONTechnique(const QString &id, const QJsonObject &jsonObject )
{
    QTechnique *t = new QTechnique;
    t->setObjectName(id);

    // Parameters
    QHash<QString, QParameter*> paramDict;
    QJsonObject params = jsonObject.value(KEY_PARAMETERS).toObject();
    Q_FOREACH (QString pname, params.keys()) {
        QJsonObject po = params.value(pname).toObject();

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
    QString programName = jsonObject.value(KEY_PROGRAM).toString();
    if (!m_programs.contains(programName)) {
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "technique" << id
                                 << ": missing program" << programName;
    }

    QRenderPass* pass = new QRenderPass;
    pass->setShaderProgram(m_programs[programName]);

    // Attributes
    QJsonObject attrs = jsonObject.value(KEY_ATTRIBUTES).toObject();
    Q_FOREACH ( QString shaderAttributeName, attrs.keys() ) {
        QString pname = attrs.value(shaderAttributeName).toString();
        QParameter *parameter = paramDict.value(pname, Q_NULLPTR);
        QString attributeName = pname;
        if (parameter == Q_NULLPTR) {
            qCWarning(GLTFParserLog) << Q_FUNC_INFO << "attribute " << pname
                                     << "defined in instanceProgram but not as parameter";
            continue;
        }
        //Check if the parameter has a standard attribute semantic
        QString standardAttributeName = standardAttributeNameFromSemantic(m_parameterDataDict[parameter].semantic);
        if (!standardAttributeName.isNull()) {
            attributeName = standardAttributeName;
            t->removeParameter(parameter);
            m_parameterDataDict.remove(parameter);
            delete parameter;
        }

        pass->addBinding(new QParameterMapping(attributeName, shaderAttributeName, QParameterMapping::Attribute));
    } // of program-instance attributes

    // Uniforms
    QJsonObject uniforms = jsonObject.value(KEY_UNIFORMS).toObject();
    Q_FOREACH (QString shaderUniformName, uniforms.keys()) {
        QString pname = uniforms.value(shaderUniformName).toString();
        QParameter *parameter = paramDict.value(pname, Q_NULLPTR);
        if (parameter == Q_NULLPTR) {
            qCWarning(GLTFParserLog) << Q_FUNC_INFO << "uniform " << pname
                                     << "defined in instanceProgram but not as parameter";
            continue;
        }
        //Check if the parameter has a standard uniform semantic
        QString standardUniformName = standardUniformNamefromSemantic(m_parameterDataDict[parameter].semantic);
        if (standardUniformName.isNull()) {
            pass->addBinding(new QParameterMapping(pname, shaderUniformName, QParameterMapping::Uniform));
        } else {
            pass->addBinding(new QParameterMapping(standardUniformName, shaderUniformName, QParameterMapping::StandardUniform));
            t->removeParameter(parameter);
            m_parameterDataDict.remove(parameter);
            delete parameter;
        }
    } // of program-instance uniforms


    // States
    QJsonObject states = jsonObject.value(KEY_STATES).toObject();

    //Process states to enable
    QJsonArray enableStatesArray = states.value(KEY_ENABLE).toArray();
    QVector<int> enableStates;
    Q_FOREACH (QJsonValue enableValue, enableStatesArray) {
        enableStates.append(enableValue.toInt());
    }

    //Process the list of state functions
    QJsonObject functions = states.value(KEY_FUNCTIONS).toObject();
    Q_FOREACH (QString functionName, functions.keys()) {
        int enableStateType = 0;
        QRenderState *renderState = buildState(functionName, functions.value(functionName), enableStateType);
        if (renderState != Q_NULLPTR) {
            //Remove the need to set a default state values for enableStateType
            enableStates.removeOne(enableStateType);
            pass->addRenderState(renderState);
        }
    }

    //Create render states with default values for any remaining enable states
    Q_FOREACH (int enableState, enableStates) {
        QRenderState *renderState = buildStateEnable(enableState);
        if (renderState != Q_NULLPTR)
            pass->addRenderState(renderState);
    }


    t->addPass(pass);

    m_techniques[id] = t;
}

void GLTFParser::processJSONAccessor( const QString &id, const QJsonObject& json )
{
    m_accessorDict[id] = AccessorData(json);
}

void GLTFParser::processJSONMesh(const QString &id, const QJsonObject &json)
{
    QJsonArray primitivesArray = json.value(KEY_PRIMITIVES).toArray();
    Q_FOREACH (QJsonValue primitiveValue, primitivesArray) {
        QJsonObject primitiveObject = primitiveValue.toObject();
        int type = primitiveObject.value(KEY_MODE).toInt();
        QString material = primitiveObject.value(KEY_MATERIAL).toString();

        if ( material.isEmpty()) {
            qCWarning(GLTFParserLog) << "malformed primitive on " << id << ", missing material value"
                                     << material;
            continue;
        }

        QGeometryRenderer *geometryRenderer = new QGeometryRenderer;
        QGeometry *meshGeometry = new QGeometry(geometryRenderer);

        //Set Primitive Type
        geometryRenderer->setPrimitiveType(static_cast<QGeometryRenderer::PrimitiveType>(type));

        //Save Material for mesh
        m_meshMaterialDict[geometryRenderer] = material;

        QJsonObject attrs = primitiveObject.value(KEY_ATTRIBUTES).toObject();
        Q_FOREACH (QString attrName, attrs.keys()) {
            QString k = attrs.value(attrName).toString();
            if (!m_accessorDict.contains(k)) {
                qCWarning(GLTFParserLog) << "unknown attribute accessor:" << k << "on mesh" << id;
                continue;
            }

            QString attributeName = standardAttributeNameFromSemantic(attrName);
            if (attributeName.isEmpty())
                attributeName = attrName;

            //Get buffer handle for accessor
            QBuffer *buffer = m_buffers.value(m_accessorDict[k].bufferViewName, Q_NULLPTR);
            if (buffer == Q_NULLPTR) {
                qCWarning(GLTFParserLog) << "unknown buffer-view:" << m_accessorDict[k].bufferViewName << "processing accessor:" << id;
                continue;
            }

            QAttribute *attribute = new QAttribute(buffer,
                                                   attributeName,
                                                   m_accessorDict[k].type,
                                                   m_accessorDict[k].dataSize,
                                                   m_accessorDict[k].count,
                                                   m_accessorDict[k].offset,
                                                   m_accessorDict[k].stride);
            attribute->setAttributeType(QAttribute::VertexAttribute);
            meshGeometry->addAttribute(attribute);
        }

        if ( primitiveObject.contains(KEY_INDICES)) {
            QString k = primitiveObject.value(KEY_INDICES).toString();
            if (!m_accessorDict.contains(k)) {
                qCWarning(GLTFParserLog) << "unknown index accessor:" << k << "on mesh" << id;
            } else {
                //Get buffer handle for accessor
                QBuffer *buffer = m_buffers.value(m_accessorDict[k].bufferViewName, Q_NULLPTR);
                if (buffer == Q_NULLPTR) {
                    qCWarning(GLTFParserLog) << "unknown buffer-view:" << m_accessorDict[k].bufferViewName << "processing accessor:" << id;
                    continue;
                }

                QAttribute *attribute = new QAttribute(buffer,
                                                       m_accessorDict[k].type,
                                                       m_accessorDict[k].dataSize,
                                                       m_accessorDict[k].count,
                                                       m_accessorDict[k].offset,
                                                       m_accessorDict[k].stride);
                attribute->setAttributeType(QAttribute::IndexAttribute);
                meshGeometry->addAttribute(attribute);
            }
        } // of has indices

        geometryRenderer->setGeometry(meshGeometry);

        m_meshDict.insert( id, geometryRenderer);
    } // of primitives iteration
}

void GLTFParser::processJSONImage(const QString &id, const QJsonObject &jsonObject)
{
    QString path = jsonObject.value(KEY_URI).toString();
    QFileInfo info(m_basePath, path);
    if (!info.exists()) {
        qCWarning(GLTFParserLog) << "can't find image" << id << "from path" << path;
        return;
    }

    m_imagePaths[id] = info.absoluteFilePath();
}

void GLTFParser::processJSONTexture(const QString &id, const QJsonObject &jsonObject)
{
    int target = jsonObject.value(KEY_TARGET).toInt(GL_TEXTURE_2D);
    //TODO: support other targets that GL_TEXTURE_2D (though the spec doesn't support anything else)
    if (target != GL_TEXTURE_2D) {
        qCWarning(GLTFParserLog) << "unsupported texture target: " << target;
        return;
    }

    QTexture2D* tex = new QTexture2D;

    // TODO: Choose suitable internal format - may vary on OpenGL context type
    //int pixelFormat = jsonObj.value(KEY_FORMAT).toInt(GL_RGBA);
    int internalFormat = jsonObject.value(KEY_INTERNAL_FORMAT).toInt(GL_RGBA);

    tex->setFormat(static_cast<QAbstractTextureProvider::TextureFormat>(internalFormat));

    QString samplerId = jsonObject.value(KEY_SAMPLER).toString();
    QString source = jsonObject.value(KEY_SOURCE).toString();
    if (!m_imagePaths.contains(source)) {
        qCWarning(GLTFParserLog) << "texture" << id << "references missing image" << source;
        return;
    }

    QTextureImage *texImage = new QTextureImage(tex);
    texImage->setSource(QUrl::fromLocalFile(m_imagePaths[source]));
    tex->addTextureImage(texImage);

    QJsonObject samplersDict(m_json.object().value(KEY_SAMPLERS).toObject());
    if (!samplersDict.contains(samplerId)) {
        qCWarning(GLTFParserLog) << "texture" << id << "references unknown sampler" << samplerId;
        return;
    }

    QJsonObject sampler = samplersDict.value(samplerId).toObject();

    tex->setWrapMode(QTextureWrapMode(static_cast<QTextureWrapMode::WrapMode>(sampler.value(KEY_WRAP_S).toInt())));
    tex->setMinificationFilter(static_cast<QAbstractTextureProvider::Filter>(sampler.value(KEY_MIN_FILTER).toInt()));
    if (tex->minificationFilter() == QAbstractTextureProvider::NearestMipMapLinear ||
        tex->minificationFilter() == QAbstractTextureProvider::LinearMipMapNearest ||
        tex->minificationFilter() == QAbstractTextureProvider::NearestMipMapNearest ||
        tex->minificationFilter() == QAbstractTextureProvider::LinearMipMapLinear) {

        tex->setGenerateMipMaps(true);
    }
    tex->setMagnificationFilter(static_cast<QAbstractTextureProvider::Filter>(sampler.value(KEY_MAG_FILTER).toInt()));

    m_textures[id] = tex;
}

void GLTFParser::loadBufferData()
{
    Q_FOREACH (QString bufferName, m_bufferDatas.keys()) {
        if (m_bufferDatas[bufferName].data == Q_NULLPTR) {
            QFile* bufferFile = resolveLocalData(m_bufferDatas[bufferName].path);
            QByteArray *data = new QByteArray(bufferFile->readAll());
            m_bufferDatas[bufferName].data = data;
            delete bufferFile;
        }
    }
}

void GLTFParser::unloadBufferData()
{
    Q_FOREACH (QString bufferName, m_bufferDatas.keys()) {
        QByteArray *data = m_bufferDatas[bufferName].data;
        delete data;
    }
}

QFile *GLTFParser::resolveLocalData(QString path) const
{
    QDir d(m_basePath);
    Q_ASSERT(d.exists());

    QString absPath = d.absoluteFilePath(path);
    QFile* f = new QFile(absPath);
    f->open(QIODevice::ReadOnly);
    return f;
}

QVariant GLTFParser::parameterValueFromJSON(int type, const QJsonValue &value) const
{
    if (value.isBool()) {
        if (type == GL_BOOL)
            return QVariant(static_cast<GLboolean>(value.toBool()));
    } else if (value.isString()) {
        if (type == GL_SAMPLER_2D) {
            //Textures are special because we need to do a lookup to return the
            //QAbstractTextureProvider
            QString textureId = value.toString();
            if (!m_textures.contains(textureId)) {
                qCWarning(GLTFParserLog) << "unknown texture" << textureId;
                return QVariant();
            } else {
                return QVariant::fromValue(m_textures.value(textureId));
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

        QJsonArray valueArray = value.toArray();

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

QAttribute::DataType GLTFParser::accessorTypeFromJSON(int componentType)
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
    qCWarning(GLTFParserLog) << "unsupported accessor type" << componentType;
    return QAttribute::Float;
}

uint GLTFParser::accessorDataSizeFromJson(const QString &type)
{
    QString typeName = type.toUpper();
    if (typeName == "SCALAR")
        return 1;
    if (typeName == "VEC2")
        return 2;
    if (typeName == "VEC3")
        return 3;
    if (typeName == "VEC4")
        return 4;
    if (typeName == "MAT2")
        return 4;
    if (typeName == "MAT3")
        return 9;
    if (typeName == "MAT4")
        return 16;

    return 0;
}

QRenderState *GLTFParser::buildStateEnable(int state)
{
    int type = 0;
    //By calling buildState with QJsonValue(), a Render State with
    //default values is created.

    if (state == GL_BLEND) {
        //It doesn't make sense to handle this state alone
        return Q_NULLPTR;
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

    qCWarning(GLTFParserLog) << Q_FUNC_INFO << "unsupported render state:" << state;

    return Q_NULLPTR;
}

QRenderState* GLTFParser::buildState(const QString& functionName, const QJsonValue &value, int &type)
{
    type = -1;
    QJsonArray values = value.toArray();

    if (functionName == QStringLiteral("blendColor")) {
        type = GL_BLEND;
        //TODO: support render state blendColor
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "unsupported render state:" << functionName;
        return Q_NULLPTR;
    }

    if (functionName == QStringLiteral("blendEquationSeparate")) {
        type = GL_BLEND;
        //TODO: support settings blendEquation alpha
        QBlendEquation *blendEquation = new QBlendEquation;
        blendEquation->setMode((QBlendEquation::BlendMode)values.at(0).toInt(GL_FUNC_ADD));
        return blendEquation;
    }

    if (functionName == QStringLiteral("blendFuncSeparate")) {
        type = GL_BLEND;
        QBlendStateSeparate *blendState = new QBlendStateSeparate;
        blendState->setSrcRGB((QBlendState::Blending)values.at(0).toInt(GL_ONE));
        blendState->setSrcAlpha((QBlendState::Blending)values.at(1).toInt(GL_ONE));
        blendState->setDstRGB((QBlendState::Blending)values.at(2).toInt(GL_ZERO));
        blendState->setDstAlpha((QBlendState::Blending)values.at(3).toInt(GL_ZERO));
        return blendState;
    }

    if (functionName == QStringLiteral("colorMask")) {
        QColorMask *colorMask = new QColorMask;
        colorMask->setRed(values.at(0).toBool(true));
        colorMask->setGreen(values.at(1).toBool(true));
        colorMask->setBlue(values.at(2).toBool(true));
        colorMask->setAlpha(values.at(3).toBool(true));
        return colorMask;
    }

    if (functionName == QStringLiteral("cullFace")) {
        type = GL_CULL_FACE;
        QCullFace *cullFace = new QCullFace;
        cullFace->setMode((QCullFace::CullingMode)values.at(0).toInt(GL_BACK));
        return cullFace;
    }

    if (functionName == QStringLiteral("depthFunc")) {
        type = GL_DEPTH_TEST;
        QDepthTest *depthTest = new QDepthTest;
        depthTest->setFunc((QDepthTest::DepthFunc)values.at(0).toInt(GL_LESS));
        return depthTest;
    }

    if (functionName == QStringLiteral("depthMask")) {
        QDepthMask *depthMask = new QDepthMask;
        depthMask->setMask(values.at(0).toBool(true));
    }

    if (functionName == QStringLiteral("depthRange")) {
        //TODO: support render state depthRange
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "unsupported render state:" << functionName;
        return Q_NULLPTR;
    }

    if (functionName == QStringLiteral("frontFace")) {
        QFrontFace *frontFace = new QFrontFace;
        frontFace->setDirection((QFrontFace::FaceDir)values.at(0).toInt(GL_CCW));
        return frontFace;
    }

    if (functionName == QStringLiteral("lineWidth")) {
        //TODO: support render state lineWidth
        qCWarning(GLTFParserLog) << Q_FUNC_INFO << "unsupported render state:" << functionName;
        return Q_NULLPTR;
    }

    if (functionName == QStringLiteral("polygonOffset")) {
        type = GL_POLYGON_OFFSET_FILL;
        QPolygonOffset *polygonOffset = new QPolygonOffset;
        polygonOffset->setFactor((float)values.at(0).toDouble(0.0f));
        polygonOffset->setUnits((float)values.at(1).toDouble(0.0f));
        return polygonOffset;
    }

    if (functionName == QStringLiteral("scissor")) {
        type = GL_SCISSOR_TEST;
        QScissorTest *scissorTest = new QScissorTest;
        scissorTest->setLeft(values.at(0).toDouble(0.0f));
        scissorTest->setBottom(values.at(1).toDouble(0.0f));
        scissorTest->setWidth(values.at(2).toDouble(0.0f));
        scissorTest->setHeight(values.at(3).toDouble(0.0f));
        return scissorTest;
    }

    qCWarning(GLTFParserLog) << Q_FUNC_INFO << "unsupported render state:" << functionName;
    return Q_NULLPTR;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
