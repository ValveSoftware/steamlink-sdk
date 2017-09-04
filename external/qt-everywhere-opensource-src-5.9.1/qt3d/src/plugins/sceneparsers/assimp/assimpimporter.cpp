/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "assimpimporter.h"

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtextureimagedatagenerator.h>
#include <Qt3DExtras/qmorphphongmaterial.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <Qt3DAnimation/qkeyframeanimation.h>
#include <Qt3DAnimation/qmorphinganimation.h>
#include <QtCore/QFileInfo>
#include <QtGui/QColor>

#include <qmath.h>

#include <Qt3DCore/private/qabstractnodefactory_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/qurlhelper_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;
using namespace Qt3DExtras;

namespace Qt3DRender {

/*!
    \class Qt3DRender::AssimpImporter
    \inmodule Qt3DRender
    \since 5.5

    \brief Provides a generic way of loading various 3D assets
    format into a Qt3D scene.

    It should be noted that Assimp aiString is explicitly defined to be UTF-8.
*/

Q_LOGGING_CATEGORY(AssimpImporterLog, "Qt3D.AssimpImporter", QtWarningMsg)

namespace {

const QString ASSIMP_MATERIAL_DIFFUSE_COLOR = QLatin1String("kd");
const QString ASSIMP_MATERIAL_SPECULAR_COLOR = QLatin1String("ks");
const QString ASSIMP_MATERIAL_AMBIENT_COLOR = QLatin1String("ka");
const QString ASSIMP_MATERIAL_EMISSIVE_COLOR = QLatin1String("emissive");
const QString ASSIMP_MATERIAL_TRANSPARENT_COLOR = QLatin1String("transparent");
const QString ASSIMP_MATERIAL_REFLECTIVE_COLOR = QLatin1String("reflective");

const QString ASSIMP_MATERIAL_DIFFUSE_TEXTURE = QLatin1String("diffuseTexture");
const QString ASSIMP_MATERIAL_AMBIENT_TEXTURE = QLatin1String("ambientTex");
const QString ASSIMP_MATERIAL_SPECULAR_TEXTURE = QLatin1String("specularTexture");
const QString ASSIMP_MATERIAL_EMISSIVE_TEXTURE = QLatin1String("emissiveTex");
const QString ASSIMP_MATERIAL_NORMALS_TEXTURE = QLatin1String("normalsTex");
const QString ASSIMP_MATERIAL_OPACITY_TEXTURE = QLatin1String("opacityTex");
const QString ASSIMP_MATERIAL_REFLECTION_TEXTURE = QLatin1String("reflectionTex");
const QString ASSIMP_MATERIAL_HEIGHT_TEXTURE = QLatin1String("heightTex");
const QString ASSIMP_MATERIAL_LIGHTMAP_TEXTURE = QLatin1String("opacityTex");
const QString ASSIMP_MATERIAL_DISPLACEMENT_TEXTURE = QLatin1String("displacementTex");
const QString ASSIMP_MATERIAL_SHININESS_TEXTURE = QLatin1String("shininessTex");

const QString ASSIMP_MATERIAL_IS_TWOSIDED = QLatin1String("twosided");
const QString ASSIMP_MATERIAL_IS_WIREFRAME = QLatin1String("wireframe");

const QString ASSIMP_MATERIAL_OPACITY = QLatin1String("opacity");
const QString ASSIMP_MATERIAL_SHININESS = QLatin1String("shininess");
const QString ASSIMP_MATERIAL_SHININESS_STRENGTH = QLatin1String("shininess_strength");
const QString ASSIMP_MATERIAL_REFRACTI = QLatin1String("refracti");
const QString ASSIMP_MATERIAL_REFLECTIVITY = QLatin1String("reflectivity");

const QString ASSIMP_MATERIAL_NAME = QLatin1String("name");

const QString VERTICES_ATTRIBUTE_NAME = QAttribute::defaultPositionAttributeName();
const QString NORMAL_ATTRIBUTE_NAME =  QAttribute::defaultNormalAttributeName();
const QString TANGENT_ATTRIBUTE_NAME = QAttribute::defaultTangentAttributeName();
const QString TEXTCOORD_ATTRIBUTE_NAME = QAttribute::defaultTextureCoordinateAttributeName();
const QString COLOR_ATTRIBUTE_NAME = QAttribute::defaultColorAttributeName();

/*!
 * Returns a QMatrix4x4 from \a matrix;
 */
QMatrix4x4 aiMatrix4x4ToQMatrix4x4(const aiMatrix4x4 &matrix) Q_DECL_NOTHROW
{
    return QMatrix4x4(matrix.a1, matrix.a2, matrix.a3, matrix.a4,
                      matrix.b1, matrix.b2, matrix.b3, matrix.b4,
                      matrix.c1, matrix.c2, matrix.c3, matrix.c4,
                      matrix.d1, matrix.d2, matrix.d3, matrix.d4);
}

/*!
 * Returns a QString from \a str;
 */
inline QString aiStringToQString(const aiString &str)
{
    return QString::fromUtf8(str.data, int(str.length));
}

QMaterial *createBestApproachingMaterial(const aiMaterial *assimpMaterial)
{
    aiString path; // unused but necessary
    const bool hasDiffuseTexture = (assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS);
    const bool hasSpecularTexture = (assimpMaterial->GetTexture(aiTextureType_SPECULAR, 0, &path) == AI_SUCCESS);

    if (hasDiffuseTexture && hasSpecularTexture)
        return QAbstractNodeFactory::createNode<QDiffuseSpecularMapMaterial>("QDiffuseSpecularMapMaterial");
    if (hasDiffuseTexture)
        return QAbstractNodeFactory::createNode<QDiffuseMapMaterial>("QDiffuseMapMaterial");
    return QAbstractNodeFactory::createNode<QPhongMaterial>("QPhongMaterial");
}

QString texturePath(const aiString &path)
{
    QString p = aiStringToQString(path);
    if (p.startsWith('/'))
        p.remove(0, 1);
    return p;
}

/*!
 * Returns the Qt3DRender::QParameter with named \a name if contained by the material
 * \a material. If the material doesn't contain the named parameter, a new
 * Qt3DRender::QParameter is created and inserted into the material.
 */
QParameter *findNamedParameter(const QString &name, QMaterial *material)
{
    // Does the material contain the parameter ?
    const auto params = material->parameters();
    for (QParameter *p : params) {
        if (p->name() == name)
            return p;
    }

    // Does the material's effect contain the parameter ?
    if (material->effect()) {
        const QEffect *e = material->effect();
        const auto params = e->parameters();
        for (QParameter *p : params) {
            if (p->name() == name)
                return p;
        }
    }

    // Create and add parameter to material
    QParameter *p = QAbstractNodeFactory::createNode<QParameter>("QParameter");
    p->setParent(material);
    p->setName(name);
    material->addParameter(p);
    return p;
}

void setParameterValue(const QString &name, QMaterial *material, const QVariant &value)
{
    QParameter *p = findNamedParameter(name, material);
    p->setValue(value);
}

QAttribute *createAttribute(QBuffer *buffer,
                            const QString &name,
                            QAttribute::VertexBaseType vertexBaseType,
                            uint vertexSize,
                            uint count,
                            uint byteOffset = 0,
                            uint byteStride = 0,
                            QNode *parent = nullptr)
{
    QAttribute *attribute = QAbstractNodeFactory::createNode<QAttribute>("QAttribute");
    attribute->setBuffer(buffer);
    attribute->setName(name);
    attribute->setVertexBaseType(vertexBaseType);
    attribute->setVertexSize(vertexSize);
    attribute->setCount(count);
    attribute->setByteOffset(byteOffset);
    attribute->setByteStride(byteStride);
    attribute->setParent(parent);
    return attribute;
}

QAttribute *createAttribute(QBuffer *buffer,
                            QAttribute::VertexBaseType vertexBaseType,
                            uint vertexSize,
                            uint count,
                            uint byteOffset = 0,
                            uint byteStride = 0,
                            QNode *parent = nullptr)
{
    QAttribute *attribute = QAbstractNodeFactory::createNode<QAttribute>("QAttribute");
    attribute->setBuffer(buffer);
    attribute->setVertexBaseType(vertexBaseType);
    attribute->setVertexSize(vertexSize);
    attribute->setCount(count);
    attribute->setByteOffset(byteOffset);
    attribute->setByteStride(byteStride);
    attribute->setParent(parent);
    return attribute;
}

QTextureWrapMode::WrapMode wrapModeFromaiTextureMapMode(int mode)
{
    switch (mode) {
    case aiTextureMapMode_Wrap:
        return QTextureWrapMode::Repeat;
    case aiTextureMapMode_Mirror:
        return QTextureWrapMode::MirroredRepeat;
    case aiTextureMapMode_Decal:
        return QTextureWrapMode::ClampToBorder;
    case aiTextureMapMode_Clamp:
    default:
        return QTextureWrapMode::ClampToEdge;
    }
}

} // anonymous

QStringList AssimpImporter::assimpSupportedFormatsList = AssimpImporter::assimpSupportedFormats();

/*!
 * Returns a QStringlist with the suffixes of the various supported asset formats.
 */
QStringList AssimpImporter::assimpSupportedFormats()
{
    QStringList formats;

    formats.reserve(60);
    formats.append(QStringLiteral("3d"));
    formats.append(QStringLiteral("3ds"));
    formats.append(QStringLiteral("ac"));
    formats.append(QStringLiteral("ac3d"));
    formats.append(QStringLiteral("acc"));
    formats.append(QStringLiteral("ase"));
    formats.append(QStringLiteral("ask"));
    formats.append(QStringLiteral("assbin"));
    formats.append(QStringLiteral("b3d"));
    formats.append(QStringLiteral("blend"));
    formats.append(QStringLiteral("bvh"));
    formats.append(QStringLiteral("cob"));
    formats.append(QStringLiteral("csm"));
    formats.append(QStringLiteral("dae"));
    formats.append(QStringLiteral("dxf"));
    formats.append(QStringLiteral("enff"));
    formats.append(QStringLiteral("fbx"));
    formats.append(QStringLiteral("hmp"));
    formats.append(QStringLiteral("irr"));
    formats.append(QStringLiteral("irrmesh"));
    formats.append(QStringLiteral("lwo"));
    formats.append(QStringLiteral("lws"));
    formats.append(QStringLiteral("lxo"));
    formats.append(QStringLiteral("md2"));
    formats.append(QStringLiteral("md3"));
    formats.append(QStringLiteral("md5anim"));
    formats.append(QStringLiteral("md5camera"));
    formats.append(QStringLiteral("md5mesh"));
    formats.append(QStringLiteral("mdc"));
    formats.append(QStringLiteral("mdl"));
    formats.append(QStringLiteral("mesh.xml"));
    formats.append(QStringLiteral("mot"));
    formats.append(QStringLiteral("ms3d"));
    formats.append(QStringLiteral("ndo"));
    formats.append(QStringLiteral("nff"));
    formats.append(QStringLiteral("obj"));
    formats.append(QStringLiteral("off"));
    formats.append(QStringLiteral("ogex"));
    formats.append(QStringLiteral("pk3"));
    formats.append(QStringLiteral("ply"));
    formats.append(QStringLiteral("prj"));
    formats.append(QStringLiteral("q3o"));
    formats.append(QStringLiteral("q3s"));
    formats.append(QStringLiteral("raw"));
    formats.append(QStringLiteral("scn"));
    formats.append(QStringLiteral("sib"));
    formats.append(QStringLiteral("smd"));
    formats.append(QStringLiteral("stl"));
    formats.append(QStringLiteral("ter"));
    formats.append(QStringLiteral("uc"));
    formats.append(QStringLiteral("vta"));
    formats.append(QStringLiteral("x"));
    formats.append(QStringLiteral("xml"));

    return formats;
}

class AssimpRawTextureImage : public QAbstractTextureImage
{
    Q_OBJECT
public:
    explicit AssimpRawTextureImage(QNode *parent = 0);

    QTextureImageDataGeneratorPtr dataGenerator() const Q_DECL_FINAL;

    void setData(const QByteArray &data);

private:
    QByteArray m_data;

    class AssimpRawTextureImageFunctor : public QTextureImageDataGenerator
    {
    public:
        explicit AssimpRawTextureImageFunctor(const QByteArray &data);

        QTextureImageDataPtr operator()() Q_DECL_FINAL;
        bool operator ==(const QTextureImageDataGenerator &other) const Q_DECL_FINAL;

        QT3D_FUNCTOR(AssimpRawTextureImageFunctor)
    private:
        QByteArray m_data;
    };
};

/*!
 *  Constructor. Initializes a new instance of AssimpImporter.
 */
AssimpImporter::AssimpImporter() : QSceneImporter(),
    m_sceneParsed(false),
    m_scene(nullptr)
{
}

/*!
 * Destructor. Cleans the parser properly before destroying it.
 */
AssimpImporter::~AssimpImporter()
{
    cleanup();
}

/*!
 *  Returns \c true if the provided \a path has a suffix supported
 *  by the Assimp Assets importer.
 */
bool AssimpImporter::isAssimpPath(const QString &path)
{
    QFileInfo fileInfo(path);

    if (!fileInfo.exists() ||
            !AssimpImporter::assimpSupportedFormatsList.contains(fileInfo.suffix().toLower()))
        return false;
    return true;
}

/*!
 * Sets the \a source used by the parser to load the asset file.
 * If the file is valid, this will trigger parsing of the file.
 */
void AssimpImporter::setSource(const QUrl &source)
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    QFileInfo file(path);
    m_sceneDir = file.absoluteDir();
    if (!file.exists()) {
        qCWarning(AssimpImporterLog) << "File missing " << path;
        return ;
    }
    readSceneFile(path);
}

/*!
 * Returns \c true if the extension of \a source is supported by
 * the assimp parser.
 */
bool AssimpImporter::isFileTypeSupported(const QUrl &source) const
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    return AssimpImporter::isAssimpPath(path);
}

/*!
 * Returns a Entity node which is the root node of the scene
 * node specified by \a id. If \a id is empty, the scene is assumed to be
 * the root node of the scene.
 *
 * Returns \c nullptr if \a id was specified but no node matching it was found.
 */
Qt3DCore::QEntity *AssimpImporter::scene(const QString &id)
{
    // m_aiScene shouldn't be null.
    // If it is either, the file failed to be imported or
    // setFilePath was not called
    if (m_scene == nullptr || m_scene->m_aiScene == nullptr)
        return nullptr;

    aiNode *rootNode = m_scene->m_aiScene->mRootNode;
    // if id specified, tries to find node
    if (!id.isEmpty() &&
            !(rootNode = rootNode->FindNode(id.toUtf8().constData()))) {
        qCDebug(AssimpImporterLog) << Q_FUNC_INFO << " Couldn't find requested scene node";
        return nullptr;
    }

    // Builds the Qt3D scene using the Assimp aiScene
    // and the various dicts filled previously by parse
    Qt3DCore::QEntity *n = node(rootNode);
    if (m_scene->m_animations.size() > 0) {
        qWarning() << "No target found for " << m_scene->m_animations.size() << " animations!";

        for (Qt3DAnimation::QKeyframeAnimation *anim : qAsConst(m_scene->m_animations))
            delete anim;
        m_scene->m_animations.clear();
    }
    return n;
}

/*!
 *  Returns a Node from the scene identified by \a id.
 *  Returns \c nullptr if the node was not found.
 */
Qt3DCore::QEntity *AssimpImporter::node(const QString &id)
{
    if (m_scene == nullptr || m_scene->m_aiScene == nullptr)
        return nullptr;
    parse();
    aiNode *n = m_scene->m_aiScene->mRootNode->FindNode(id.toUtf8().constData());
    return node(n);
}

template <typename T>
void findAnimationsForNode(QVector<T *> &animations, QVector<T *> &result, const QString &name)
{
    for (T *anim : animations) {
        if (anim->targetName() == name) {
            result.push_back(anim);
            animations.removeAll(anim);
        }
    }
}

/*!
 * Returns a Node from an Assimp aiNode \a node.
 */
Qt3DCore::QEntity *AssimpImporter::node(aiNode *node)
{
    if (node == nullptr)
        return nullptr;
    QEntity *entityNode = QAbstractNodeFactory::createNode<Qt3DCore::QEntity>("QEntity");
    entityNode->setObjectName(aiStringToQString(node->mName));

    // Add Meshes to the node
    for (uint i = 0; i < node->mNumMeshes; i++) {
        uint meshIdx = node->mMeshes[i];
        QMaterial *material = nullptr;
        QGeometryRenderer *mesh = m_scene->m_meshes[meshIdx];
        // mesh material
        uint materialIndex = m_scene->m_aiScene->mMeshes[meshIdx]->mMaterialIndex;

        if (m_scene->m_materials.contains(materialIndex))
            material = m_scene->m_materials[materialIndex];

        QList<Qt3DAnimation::QMorphingAnimation *> morphingAnimations
                = mesh->findChildren<Qt3DAnimation::QMorphingAnimation *>();
        if (morphingAnimations.size() > 0) {
            material = new Qt3DExtras::QMorphPhongMaterial(entityNode);

            QVector<Qt3DAnimation::QMorphingAnimation *> animations;
            findAnimationsForNode<Qt3DAnimation::QMorphingAnimation>(m_scene->m_morphAnimations,
                                                                  animations,
                                                                  aiStringToQString(node->mName));
            const auto morphTargetList = morphingAnimations.at(0)->morphTargetList();
            for (Qt3DAnimation::QMorphingAnimation *anim : qAsConst(animations)) {
                anim->setParent(entityNode);
                anim->setTarget(mesh);
                anim->setMorphTargets(morphTargetList);
            }

            for (int j = 0; j < animations.size(); ++j) {
                QObject::connect(animations[j], &Qt3DAnimation::QMorphingAnimation::interpolatorChanged,
                                (Qt3DExtras::QMorphPhongMaterial *)material,
                                 &Qt3DExtras::QMorphPhongMaterial::setInterpolator);
            }
            morphingAnimations[0]->deleteLater();
        }

        if (node->mNumMeshes == 1) {
            if (material)
                entityNode->addComponent(material);
            // mesh
            entityNode->addComponent(mesh);
        } else {
            QEntity *childEntity = QAbstractNodeFactory::createNode<Qt3DCore::QEntity>("QEntity");
            if (material)
                childEntity->addComponent(material);
            childEntity->addComponent(mesh);
            childEntity->setParent(entityNode);

            Qt3DCore::QTransform *transform
                    = QAbstractNodeFactory::createNode<Qt3DCore::QTransform>("QTransform");
            childEntity->addComponent(transform);
        }
    }

    // Add Children to Node
    for (uint i = 0; i < node->mNumChildren; i++) {
        // this-> is necessary here otherwise
        // it conflicts with the variable node
        QEntity *child = this->node(node->mChildren[i]);
        // Are we sure each child are unique ???
        if (child != nullptr)
            child->setParent(entityNode);
    }

    // Add Transformations
    const QMatrix4x4 qTransformMatrix = aiMatrix4x4ToQMatrix4x4(node->mTransformation);
    Qt3DCore::QTransform *transform = QAbstractNodeFactory::createNode<Qt3DCore::QTransform>("QTransform");
    transform->setMatrix(qTransformMatrix);
    entityNode->addComponent(transform);

    QVector<Qt3DAnimation::QKeyframeAnimation *> animations;
    findAnimationsForNode<Qt3DAnimation::QKeyframeAnimation>(m_scene->m_animations,
                                                          animations,
                                                          aiStringToQString(node->mName));

    for (Qt3DAnimation::QKeyframeAnimation *anim : qAsConst(animations)) {
        anim->setTarget(transform);
        anim->setParent(entityNode);
    }

    // Add Camera
    if (m_scene->m_cameras.contains(node))
        m_scene->m_cameras[node]->setParent(entityNode);

    // TO DO : Add lights ....

    return entityNode;
}

/*!
 * Reads the scene file pointed by \a path and launches the parsing of
 * the scene using Assimp, after having cleaned up previously saved values
 * from eventual previous parsings.
 */
void AssimpImporter::readSceneFile(const QString &path)
{
    cleanup();

    m_scene = new SceneImporter();

    // SET THIS TO REMOVE POINTS AND LINES -> HAVE ONLY TRIANGLES
    m_scene->m_importer->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE|aiPrimitiveType_POINT);
    // SET CUSTOM FILE HANDLER TO HANDLE FILE READING THROUGH QT (RESOURCES, SOCKET ...)
    m_scene->m_importer->SetIOHandler(new AssimpHelper::AssimpIOSystem());

    // type and aiProcess_Triangulate discompose polygons with more than 3 points in triangles
    // aiProcess_SortByPType makes sure that meshes data are triangles
    m_scene->m_aiScene = m_scene->m_importer->ReadFile(path.toUtf8().constData(),
                                                       aiProcess_SortByPType|
                                                       aiProcess_Triangulate|
                                                       aiProcess_GenSmoothNormals|
                                                       aiProcess_FlipUVs);
    if (m_scene->m_aiScene == nullptr) {
        qCWarning(AssimpImporterLog) << "Assimp scene import failed";
        return ;
    }
    parse();
}

/*!
 * Cleans the various dictionaries holding the scene's information.
 */
void AssimpImporter::cleanup()
{
    m_sceneParsed = false;
    delete m_scene;
    m_scene = nullptr;
}

/*!
 * Parses the aiScene provided py Assimp and converts Assimp
 * values to Qt3D values.
 */
void AssimpImporter::parse()
{
    if (!m_sceneParsed) {
        // Set parsed flags
        m_sceneParsed = !m_sceneParsed;

        for (uint i = 0; i < m_scene->m_aiScene->mNumTextures; i++)
            loadEmbeddedTexture(i);
        for (uint i = 0; i < m_scene->m_aiScene->mNumMaterials; i++)
            loadMaterial(i);
        for (uint i = 0; i < m_scene->m_aiScene->mNumMeshes; i++)
            loadMesh(i);
        for (uint i = 0; i < m_scene->m_aiScene->mNumCameras; i++)
            loadCamera(i);
        for (uint i = 0; i < m_scene->m_aiScene->mNumLights; i++)
            loadLight(i);
        for (uint i = 0; i < m_scene->m_aiScene->mNumAnimations; i++)
            loadAnimation(i);
    }
}

/*!
 * Converts the provided Assimp aiMaterial identified by \a materialIndex to a
 * Qt3D material and adds it to a dictionary of materials.
 * \sa Material
 */
void AssimpImporter::loadMaterial(uint materialIndex)
{
    // Generates default material based on what the assimp material contains
    aiMaterial *assimpMaterial = m_scene->m_aiScene->mMaterials[materialIndex];
    QMaterial *material = createBestApproachingMaterial(assimpMaterial);
    // Material Name
    copyMaterialName(material, assimpMaterial);
    copyMaterialColorProperties(material, assimpMaterial);
    copyMaterialBoolProperties(material, assimpMaterial);
    copyMaterialFloatProperties(material, assimpMaterial);

    // Add textures to materials dict
    copyMaterialTextures(material, assimpMaterial);

    m_scene->m_materials.insert(materialIndex, material);
}

/*!
 * Converts the Assimp aiMesh mesh identified by \a meshIndex to a QGeometryRenderer
 * and adds it to a dictionary of meshes.
 * \sa QGeometryRenderer
 */
void AssimpImporter::loadMesh(uint meshIndex)
{
    aiMesh *mesh = m_scene->m_aiScene->mMeshes[meshIndex];

    QGeometryRenderer *geometryRenderer = QAbstractNodeFactory::createNode<QGeometryRenderer>("QGeometryRenderer");
    QGeometry *meshGeometry = QAbstractNodeFactory::createNode<QGeometry>("QGeometry");
    meshGeometry->setParent(geometryRenderer);
    Qt3DRender::QBuffer *vertexBuffer = QAbstractNodeFactory::createNode<Qt3DRender::QBuffer>("QBuffer");
    vertexBuffer->setParent(meshGeometry);
    vertexBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);
    Qt3DRender::QBuffer *indexBuffer = QAbstractNodeFactory::createNode<Qt3DRender::QBuffer>("QBuffer");
    indexBuffer->setParent(meshGeometry);
    indexBuffer->setType(Qt3DRender::QBuffer::IndexBuffer);

    geometryRenderer->setGeometry(meshGeometry);

    // Primitive are always triangles with the current Assimp's configuration

    // Vertices and Normals always present with the current Assimp's configuration
    aiVector3D *vertices = mesh->mVertices;
    aiVector3D *normals = mesh->mNormals;
    aiColor4D *colors = mesh->mColors[0];
    // Tangents and TextureCoord not always present
    bool hasTangent = mesh->HasTangentsAndBitangents();
    bool hasTexture = mesh->HasTextureCoords(0);
    bool hasColor = (colors != NULL); // NULL defined by Assimp
    aiVector3D *tangents = hasTangent ? mesh->mTangents : nullptr;
    aiVector3D *textureCoord = hasTexture ? mesh->mTextureCoords[0] : nullptr;

    // Add values in raw float array
    ushort chunkSize = 6 + (hasTangent ? 3 : 0) + (hasTexture ? 2 : 0) + (hasColor ? 4 : 0);
    QByteArray bufferArray;
    bufferArray.resize(chunkSize * mesh->mNumVertices * sizeof(float));
    float *vbufferContent = reinterpret_cast<float*>(bufferArray.data());
    for (uint i = 0; i < mesh->mNumVertices; i++) {
        uint idx = i * chunkSize;
        // position
        vbufferContent[idx] = vertices[i].x;
        vbufferContent[idx + 1] = vertices[i].y;
        vbufferContent[idx + 2] = vertices[i].z;
        // normals
        vbufferContent[idx + 3] = normals[i].x;
        vbufferContent[idx + 4] = normals[i].y;
        vbufferContent[idx + 5] = normals[i].z;

        if (hasTangent) {
            vbufferContent[idx + 6] = tangents[i].x;
            vbufferContent[idx + 7] = tangents[i].y;
            vbufferContent[idx + 8] = tangents[i].z;
        }
        if (hasTexture) {
            char offset =  (hasTangent ? 9 : 6);
            vbufferContent[idx + offset] = textureCoord[i].x;
            vbufferContent[idx + offset + 1] = textureCoord[i].y;
        }
        if (hasColor) {
            char offset = 6 + (hasTangent ? 3 : 0) + (hasTexture ? 2 : 0);
            vbufferContent[idx + offset] = colors[i].r;
            vbufferContent[idx + offset + 1] = colors[i].g;
            vbufferContent[idx + offset + 2] = colors[i].b;
            vbufferContent[idx + offset + 3] = colors[i].a;
        }
    }

    vertexBuffer->setData(bufferArray);

    // Add vertex attributes to the mesh with the right array
    QAttribute *positionAttribute = createAttribute(vertexBuffer, VERTICES_ATTRIBUTE_NAME,
                                          QAttribute::Float, 3,
                                          mesh->mNumVertices,
                                          0,
                                          chunkSize * sizeof(float));

    QAttribute *normalAttribute = createAttribute(vertexBuffer, NORMAL_ATTRIBUTE_NAME,
                                          QAttribute::Float, 3,
                                          mesh->mNumVertices,
                                          3 * sizeof(float),
                                          chunkSize * sizeof(float));

    meshGeometry->addAttribute(positionAttribute);
    meshGeometry->addAttribute(normalAttribute);

    if (hasTangent) {
        QAttribute *tangentsAttribute = createAttribute(vertexBuffer, TANGENT_ATTRIBUTE_NAME,
                                              QAttribute::Float, 3,
                                              mesh->mNumVertices,
                                              6 * sizeof(float),
                                              chunkSize * sizeof(float));
        meshGeometry->addAttribute(tangentsAttribute);
    }

    if (hasTexture) {
        QAttribute *textureCoordAttribute = createAttribute(vertexBuffer, TEXTCOORD_ATTRIBUTE_NAME,
                                              QAttribute::Float, 2,
                                              mesh->mNumVertices,
                                              (hasTangent ? 9 : 6) * sizeof(float),
                                              chunkSize * sizeof(float));
        meshGeometry->addAttribute(textureCoordAttribute);
    }

    if (hasColor) {
        QAttribute *colorAttribute = createAttribute(vertexBuffer, COLOR_ATTRIBUTE_NAME,
                                              QAttribute::Float, 4,
                                              mesh->mNumVertices,
                                              (6 + (hasTangent ? 3 : 0) + (hasTexture ? 2 : 0)) * sizeof(float),
                                              chunkSize * sizeof(float));
        meshGeometry->addAttribute(colorAttribute);
    }

    QAttribute::VertexBaseType indiceType;
    QByteArray ibufferContent;
    uint indices = mesh->mNumFaces * 3;
    // If there are less than 65535 indices, indices can then fit in ushort
    // which saves video memory
    if (indices >= USHRT_MAX) {
        indiceType = QAttribute::UnsignedInt;
        ibufferContent.resize(indices * sizeof(quint32));
        for (uint i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            Q_ASSERT(face.mNumIndices == 3);
            memcpy(&reinterpret_cast<quint32*>(ibufferContent.data())[i * 3], face.mIndices, 3 * sizeof(uint));
        }
    }
    else {
        indiceType = QAttribute::UnsignedShort;
        ibufferContent.resize(indices * sizeof(quint16));
        for (uint i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            Q_ASSERT(face.mNumIndices == 3);
            for (ushort j = 0; j < face.mNumIndices; j++)
                reinterpret_cast<quint16*>(ibufferContent.data())[i * 3 + j] = face.mIndices[j];
        }
    }

    indexBuffer->setData(ibufferContent);

    // Add indices attributes
    QAttribute *indexAttribute = createAttribute(indexBuffer, indiceType, 1, indices);
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);

    meshGeometry->addAttribute(indexAttribute);

    m_scene->m_meshes[meshIndex] = geometryRenderer;

    if (mesh->mNumAnimMeshes > 0) {

        aiAnimMesh *animesh = mesh->mAnimMeshes[0];

        if (animesh->mNumVertices != mesh->mNumVertices)
            return;

        Qt3DAnimation::QMorphingAnimation *morphingAnimation
                = new Qt3DAnimation::QMorphingAnimation(geometryRenderer);
        QVector<QString> names;
        QVector<Qt3DAnimation::QMorphTarget *> targets;
        uint voff = 0;
        uint noff = 0;
        uint tanoff = 0;
        uint texoff = 0;
        uint coloff = 0;
        uint offset = 0;
        if (animesh->mVertices) {
            names.push_back(VERTICES_ATTRIBUTE_NAME);
            offset += 3;
        }
        if (animesh->mNormals) {
            names.push_back(NORMAL_ATTRIBUTE_NAME);
            noff = offset;
            offset += 3;
        }
        if (animesh->mTangents) {
            names.push_back(TANGENT_ATTRIBUTE_NAME);
            tanoff = offset;
            offset += 3;
        }
        if (animesh->mTextureCoords[0]) {
            names.push_back(TEXTCOORD_ATTRIBUTE_NAME);
            texoff = offset;
            offset += 2;
        }
        if (animesh->mColors[0]) {
            names.push_back(COLOR_ATTRIBUTE_NAME);
            coloff = offset;
        }

        ushort clumpSize = (animesh->mVertices ? 3 : 0)
                            + (animesh->mNormals ? 3 : 0)
                            + (animesh->mTangents ? 3 : 0)
                            + (animesh->mColors[0] ? 4 : 0)
                            + (animesh->mTextureCoords[0] ? 2 : 0);


        for (uint i = 0; i < mesh->mNumAnimMeshes; i++) {
            aiAnimMesh *animesh = mesh->mAnimMeshes[i];
            Qt3DAnimation::QMorphTarget *target = new Qt3DAnimation::QMorphTarget(geometryRenderer);
            targets.push_back(target);
            QVector<QAttribute *> attributes;
            QByteArray targetBufferArray;
            targetBufferArray.resize(clumpSize * mesh->mNumVertices * sizeof(float));
            float *dst = reinterpret_cast<float *>(targetBufferArray.data());

            for (uint j = 0; j < mesh->mNumVertices; j++) {
                if (animesh->mVertices) {
                    *dst++ = animesh->mVertices[j].x;
                    *dst++ = animesh->mVertices[j].y;
                    *dst++ = animesh->mVertices[j].z;
                }
                if (animesh->mNormals) {
                    *dst++ = animesh->mNormals[j].x;
                    *dst++ = animesh->mNormals[j].y;
                    *dst++ = animesh->mNormals[j].z;
                }
                if (animesh->mTangents) {
                    *dst++ = animesh->mTangents[j].x;
                    *dst++ = animesh->mTangents[j].y;
                    *dst++ = animesh->mTangents[j].z;
                }
                if (animesh->mTextureCoords[0]) {
                    *dst++ = animesh->mTextureCoords[0][j].x;
                    *dst++ = animesh->mTextureCoords[0][j].y;
                }
                if (animesh->mColors[0]) {
                    *dst++ = animesh->mColors[0][j].r;
                    *dst++ = animesh->mColors[0][j].g;
                    *dst++ = animesh->mColors[0][j].b;
                    *dst++ = animesh->mColors[0][j].a;
                }
            }

            Qt3DRender::QBuffer *targetBuffer
                    = QAbstractNodeFactory::createNode<Qt3DRender::QBuffer>("QBuffer");
            targetBuffer->setData(targetBufferArray);
            targetBuffer->setParent(meshGeometry);

            if (animesh->mVertices) {
                attributes.push_back(createAttribute(targetBuffer, VERTICES_ATTRIBUTE_NAME,
                                                     QAttribute::Float, 3,
                                                     animesh->mNumVertices, voff * sizeof(float),
                                                     clumpSize * sizeof(float), meshGeometry));
            }
            if (animesh->mNormals) {
                attributes.push_back(createAttribute(targetBuffer, NORMAL_ATTRIBUTE_NAME,
                                                     QAttribute::Float, 3,
                                                     animesh->mNumVertices, noff * sizeof(float),
                                                     clumpSize * sizeof(float), meshGeometry));
            }
            if (animesh->mTangents) {
                attributes.push_back(createAttribute(targetBuffer, TANGENT_ATTRIBUTE_NAME,
                                                     QAttribute::Float, 3,
                                                     animesh->mNumVertices, tanoff * sizeof(float),
                                                     clumpSize * sizeof(float), meshGeometry));
            }
            if (animesh->mTextureCoords[0]) {
                attributes.push_back(createAttribute(targetBuffer, TEXTCOORD_ATTRIBUTE_NAME,
                                                     QAttribute::Float, 2,
                                                     animesh->mNumVertices, texoff * sizeof(float),
                                                     clumpSize * sizeof(float), meshGeometry));
            }
            if (animesh->mColors[0]) {
                attributes.push_back(createAttribute(targetBuffer, COLOR_ATTRIBUTE_NAME,
                                                     QAttribute::Float, 4,
                                                     animesh->mNumVertices, coloff * sizeof(float),
                                                     clumpSize * sizeof(float), meshGeometry));
            }
            target->setAttributes(attributes);
        }
        morphingAnimation->setMorphTargets(targets);
        morphingAnimation->setTargetName(aiStringToQString(mesh->mName));
        morphingAnimation->setTarget(geometryRenderer);
    }

    qCDebug(AssimpImporterLog) << Q_FUNC_INFO << " Mesh " << aiStringToQString(mesh->mName)
                               << " Vertices " << mesh->mNumVertices << " Faces "
                               << mesh->mNumFaces << " Indices " << indices;
}

/*!
 * Converts the provided Assimp aiTexture at \a textureIndex to a Texture and
 * adds it to a dictionary of textures.
 * \sa Texture
 */
void AssimpImporter::loadEmbeddedTexture(uint textureIndex)
{
    aiTexture *assimpTexture = m_scene->m_aiScene->mTextures[textureIndex];
    QAbstractTexture *texture = QAbstractNodeFactory::createNode<QTexture2D>("QTexture2D");
    AssimpRawTextureImage *imageData = new AssimpRawTextureImage();

    bool isCompressed = assimpTexture->mHeight == 0;
    uint textureSize = assimpTexture->mWidth *
            (isCompressed ? 1 : assimpTexture->mHeight);
    // Set texture to RGBA8888
    QByteArray textureContent;
    textureContent.reserve(textureSize * 4);
    for (uint i = 0; i < textureSize; i++) {
        uint idx = i * 4;
        aiTexel texel = assimpTexture->pcData[i];
        textureContent[idx] = texel.r;
        textureContent[idx + 1] = texel.g;
        textureContent[idx + 2] = texel.b;
        textureContent[idx + 3] = texel.a;
    }
    imageData->setData(textureContent);
    texture->addTextureImage(imageData);
    m_scene->m_embeddedTextures[textureIndex] = texture;
}

/*!
 * Loads the light in the current scene located at \a lightIndex.
 */
void AssimpImporter::loadLight(uint lightIndex)
{
    aiLight *light = m_scene->m_aiScene->mLights[lightIndex];
    // TODO: Implement me!
    Q_UNUSED(light);
}

/*!
 * Parses the camera at cameraIndex and saves it to a dictionary of cameras.
 */
void AssimpImporter::loadCamera(uint cameraIndex)
{
    aiCamera *assimpCamera = m_scene->m_aiScene->mCameras[cameraIndex];
    aiNode *cameraNode = m_scene->m_aiScene->mRootNode->FindNode(assimpCamera->mName);

    // If no node is associated to the camera in the scene, camera not saved
    if (cameraNode == nullptr)
        return ;

    QEntity *camera = QAbstractNodeFactory::createNode<Qt3DCore::QEntity>("QEntity");
    QCameraLens *lens = QAbstractNodeFactory::createNode<QCameraLens>("QCameraLens");

    lens->setObjectName(aiStringToQString(assimpCamera->mName));
    lens->setPerspectiveProjection(qRadiansToDegrees(assimpCamera->mHorizontalFOV),
                                   qMax(assimpCamera->mAspect, 1.0f),
                                   assimpCamera->mClipPlaneNear,
                                   assimpCamera->mClipPlaneFar);
    camera->addComponent(lens);

    QMatrix4x4 m;
    m.lookAt(QVector3D(assimpCamera->mPosition.x, assimpCamera->mPosition.y, assimpCamera->mPosition.z),
             QVector3D(assimpCamera->mLookAt.x, assimpCamera->mLookAt.y, assimpCamera->mLookAt.z),
             QVector3D(assimpCamera->mUp.x, assimpCamera->mUp.y, assimpCamera->mUp.z));
    Qt3DCore::QTransform *transform = QAbstractNodeFactory::createNode<Qt3DCore::QTransform>("QTransform");
    transform->setMatrix(m);
    camera->addComponent(transform);

    m_scene->m_cameras[cameraNode] = camera;
}

int findTimeIndex(const QVector<float> &times, float time) {
    for (int i = 0; i < times.size(); i++) {
        if (qFuzzyCompare(times[i], time))
            return i;
    }
    return -1;
}

void insertAtTime(QVector<float> &positions, QVector<Qt3DCore::QTransform *> &tranforms,
                  Qt3DCore::QTransform *t, float time)
{
    if (positions.size() == 0) {
        positions.push_back(time);
        tranforms.push_back(t);
    } else if (time < positions.first()) {
        positions.push_front(time);
        tranforms.push_front(t);
    } else if (time > positions.last()) {
        positions.push_back(time);
        tranforms.push_back(t);
    } else {
        qWarning() << "Insert new key in the middle of the keyframe not implemented.";
    }
}

// OPTIONAL
void AssimpImporter::loadAnimation(uint animationIndex)
{
    aiAnimation *assimpAnim = m_scene->m_aiScene->mAnimations[animationIndex];
    qCDebug(AssimpImporterLog) << "load Animation: "<< aiStringToQString(assimpAnim->mName);
    double tickScale = 1.0;
    if (!qFuzzyIsNull(assimpAnim->mTicksPerSecond))
        tickScale = 1.0 / assimpAnim->mTicksPerSecond;

    /* keyframe animations */
    for (uint i = 0; i < assimpAnim->mNumChannels; ++i) {
        aiNodeAnim *nodeAnim = assimpAnim->mChannels[i];
        aiNode *targetNode = m_scene->m_aiScene->mRootNode->FindNode(nodeAnim->mNodeName);

        Qt3DAnimation::QKeyframeAnimation *kfa = new Qt3DAnimation::QKeyframeAnimation();
        QVector<float> positions;
        QVector<Qt3DCore::QTransform*> transforms;
        if ((nodeAnim->mNumPositionKeys > 1)
                || !(nodeAnim->mNumPositionKeys == 1 && nodeAnim->mPositionKeys[0].mValue.x == 0
                    && nodeAnim->mPositionKeys[0].mValue.y == 0
                     && nodeAnim->mPositionKeys[0].mValue.z == 0)) {
            for (uint j = 0; j < nodeAnim->mNumPositionKeys; j++) {
                positions.push_back(nodeAnim->mPositionKeys[j].mTime);
                Qt3DCore::QTransform *t = new Qt3DCore::QTransform();
                t->setTranslation(QVector3D(nodeAnim->mPositionKeys[j].mValue.x,
                                            nodeAnim->mPositionKeys[j].mValue.y,
                                            nodeAnim->mPositionKeys[j].mValue.z));
                transforms.push_back(t);
            }
        }
        if ((nodeAnim->mNumRotationKeys > 1) ||
                !(nodeAnim->mNumRotationKeys == 1 && nodeAnim->mRotationKeys[0].mValue.x == 0
                  && nodeAnim->mRotationKeys[0].mValue.y == 0
                  && nodeAnim->mRotationKeys[0].mValue.z == 0
                  && nodeAnim->mRotationKeys[0].mValue.w == 1)) {
            for (uint j = 0; j < nodeAnim->mNumRotationKeys; j++) {
                int index = findTimeIndex(positions, nodeAnim->mRotationKeys[j].mTime);
                if (index >= 0) {
                    Qt3DCore::QTransform *t = transforms[index];
                    t->setRotation(QQuaternion(nodeAnim->mRotationKeys[j].mValue.w,
                                               nodeAnim->mRotationKeys[j].mValue.x,
                                               nodeAnim->mRotationKeys[j].mValue.y,
                                               nodeAnim->mRotationKeys[j].mValue.z));
                } else {
                    Qt3DCore::QTransform *t = new Qt3DCore::QTransform();
                    t->setRotation(QQuaternion(nodeAnim->mRotationKeys[j].mValue.w,
                                               nodeAnim->mRotationKeys[j].mValue.x,
                                               nodeAnim->mRotationKeys[j].mValue.y,
                                               nodeAnim->mRotationKeys[j].mValue.z));
                    insertAtTime(positions, transforms, t, nodeAnim->mRotationKeys[j].mTime);
                }
            }
        }
        if ((nodeAnim->mNumScalingKeys > 1)
                || !(nodeAnim->mNumScalingKeys == 1 && nodeAnim->mScalingKeys[0].mValue.x == 1
                    && nodeAnim->mScalingKeys[0].mValue.y == 1
                     && nodeAnim->mScalingKeys[0].mValue.z == 1)) {
            for (uint j = 0; j < nodeAnim->mNumScalingKeys; j++) {
                int index = findTimeIndex(positions, nodeAnim->mScalingKeys[j].mTime);
                if (index >= 0) {
                    Qt3DCore::QTransform *t = transforms[index];
                    t->setScale3D(QVector3D(nodeAnim->mScalingKeys[j].mValue.x,
                                            nodeAnim->mScalingKeys[j].mValue.y,
                                            nodeAnim->mScalingKeys[j].mValue.z));
                } else {
                    Qt3DCore::QTransform *t = new Qt3DCore::QTransform();
                    t->setScale3D(QVector3D(nodeAnim->mScalingKeys[j].mValue.x,
                                            nodeAnim->mScalingKeys[j].mValue.y,
                                            nodeAnim->mScalingKeys[j].mValue.z));
                    insertAtTime(positions, transforms, t, nodeAnim->mScalingKeys[j].mTime);
                }
            }
        }
        for (int j = 0; j < positions.size(); ++j)
            positions[j] = positions[j] * tickScale;
        kfa->setFramePositions(positions);
        kfa->setKeyframes(transforms);
        kfa->setAnimationName(QString(assimpAnim->mName.C_Str()));
        kfa->setTargetName(QString(targetNode->mName.C_Str()));
        m_scene->m_animations.push_back(kfa);
    }
    /* mesh morph animations */
    for (uint i = 0; i < assimpAnim->mNumMorphMeshChannels; ++i) {
        aiMeshMorphAnim *morphAnim = assimpAnim->mMorphMeshChannels[i];
        aiNode *targetNode = m_scene->m_aiScene->mRootNode->FindNode(morphAnim->mName);
        aiMesh *mesh = m_scene->m_aiScene->mMeshes[targetNode->mMeshes[0]];

        Qt3DAnimation::QMorphingAnimation *morphingAnimation = new Qt3DAnimation::QMorphingAnimation;
        QVector<float> positions;
        positions.resize(morphAnim->mNumKeys);
        // set so that weights array is allocated to correct size in morphingAnimation
        morphingAnimation->setTargetPositions(positions);
        for (unsigned int j = 0; j < morphAnim->mNumKeys; ++j) {
            aiMeshMorphKey &key = morphAnim->mKeys[j];
            positions[j] = key.mTime * tickScale;

            QVector<float> weights;
            weights.resize(key.mNumValuesAndWeights);
            for (int k = 0; k < weights.size(); k++) {
                const unsigned int value = key.mValues[k];
                if (value < key.mNumValuesAndWeights)
                    weights[value] = key.mWeights[k];
            }
            morphingAnimation->setWeights(j, weights);
        }

        morphingAnimation->setTargetPositions(positions);
        morphingAnimation->setAnimationName(QString(assimpAnim->mName.C_Str()));
        morphingAnimation->setTargetName(QString(targetNode->mName.C_Str()));
        morphingAnimation->setMethod((mesh->mMethod == aiMorphingMethod_MORPH_NORMALIZED)
                                     ? Qt3DAnimation::QMorphingAnimation::Normalized
                                     : Qt3DAnimation::QMorphingAnimation::Relative);
        m_scene->m_morphAnimations.push_back(morphingAnimation);
    }
}

/*!
 *  Sets the object name of \a material to the name of \a assimpMaterial.
 */
void AssimpImporter::copyMaterialName(QMaterial *material, aiMaterial *assimpMaterial)
{
    aiString name;
    if (assimpMaterial->Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) {
        // May not be necessary
        // Kept for debug purposes at the moment
        material->setObjectName(aiStringToQString(name));
        qCDebug(AssimpImporterLog) << Q_FUNC_INFO << "Assimp Material " << material->objectName();
    }
}

/*!
 *  Fills \a material color properties with \a assimpMaterial color properties.
 */
void AssimpImporter::copyMaterialColorProperties(QMaterial *material, aiMaterial *assimpMaterial)
{
    aiColor3D color;
    if (assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_DIFFUSE_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
    if (assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_SPECULAR_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
    if (assimpMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_AMBIENT_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
    if (assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_EMISSIVE_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
    if (assimpMaterial->Get(AI_MATKEY_COLOR_TRANSPARENT, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_TRANSPARENT_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
    if (assimpMaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, color) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_REFLECTIVE_COLOR, material, QColor::fromRgbF(color.r, color.g, color.b));
}

/*!
 * Retrieves a \a material bool property.
 */
void AssimpImporter::copyMaterialBoolProperties(QMaterial *material, aiMaterial *assimpMaterial)
{
    int value;
    if (assimpMaterial->Get(AI_MATKEY_TWOSIDED, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_IS_TWOSIDED, material, (value == 0) ? false : true);
    if (assimpMaterial->Get(AI_MATKEY_ENABLE_WIREFRAME, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_IS_WIREFRAME, material, (value == 0) ? false : true);
}

void AssimpImporter::copyMaterialShadingModel(QMaterial *material, aiMaterial *assimpMaterial)
{
    Q_UNUSED(material);
    Q_UNUSED(assimpMaterial);
    // TODO
    // Match each shading function with a default shader

    //    AssimpIO::assimpMaterialAttributesMap[AI_MATKEY_SHADING_MODEL] = &AssimpIO::getMaterialShadingModel;
    //    AssimpIO::assimpMaterialAttributesMap[AI_MATKEY_BLEND_FUNC] = &AssimpIO::getMaterialBlendingFunction;
}

void AssimpImporter::copyMaterialBlendingFunction(QMaterial *material, aiMaterial *assimpMaterial)
{
    Q_UNUSED(material);
    Q_UNUSED(assimpMaterial);
    // TO DO
}

/*!
 *
 */
void AssimpImporter::copyMaterialTextures(QMaterial *material, aiMaterial *assimpMaterial)
{
    static const aiTextureType textureType[] = {aiTextureType_AMBIENT,
                                                aiTextureType_DIFFUSE,
                                                aiTextureType_DISPLACEMENT,
                                                aiTextureType_EMISSIVE,
                                                aiTextureType_HEIGHT,
                                                aiTextureType_LIGHTMAP,
                                                aiTextureType_NORMALS,
                                                aiTextureType_OPACITY,
                                                aiTextureType_REFLECTION,
                                                aiTextureType_SHININESS,
                                                aiTextureType_SPECULAR};

    if (m_scene->m_textureToParameterName.isEmpty()) {
        m_scene->m_textureToParameterName.insert(aiTextureType_AMBIENT, ASSIMP_MATERIAL_AMBIENT_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_DIFFUSE, ASSIMP_MATERIAL_DIFFUSE_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_DISPLACEMENT, ASSIMP_MATERIAL_DISPLACEMENT_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_EMISSIVE, ASSIMP_MATERIAL_EMISSIVE_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_HEIGHT, ASSIMP_MATERIAL_HEIGHT_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_LIGHTMAP, ASSIMP_MATERIAL_LIGHTMAP_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_NORMALS, ASSIMP_MATERIAL_NORMALS_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_OPACITY, ASSIMP_MATERIAL_OPACITY_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_REFLECTION, ASSIMP_MATERIAL_REFLECTION_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_SHININESS, ASSIMP_MATERIAL_SHININESS_TEXTURE);
        m_scene->m_textureToParameterName.insert(aiTextureType_SPECULAR, ASSIMP_MATERIAL_SPECULAR_TEXTURE);
    }

    for (unsigned int i = 0; i < sizeof(textureType)/sizeof(textureType[0]); i++) {
        aiString path;
        if (assimpMaterial->GetTexture(textureType[i], 0, &path) == AI_SUCCESS) {
            const QString fullPath = m_sceneDir.absoluteFilePath(texturePath(path));
            // Load texture if not already loaded
            QAbstractTexture *tex = QAbstractNodeFactory::createNode<QTexture2D>("QTexture2D");
            QTextureImage *texImage = QAbstractNodeFactory::createNode<QTextureImage>("QTextureImage");
            texImage->setSource(QUrl::fromLocalFile(fullPath));
            tex->addTextureImage(texImage);

            // Set proper wrapping mode
            QTextureWrapMode wrapMode(QTextureWrapMode::Repeat);
            int xMode = 0;
            int yMode = 0;

            if (assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_U(textureType[i], 0), xMode) == aiReturn_SUCCESS)
                wrapMode.setX(wrapModeFromaiTextureMapMode(xMode));
            if (assimpMaterial->Get(AI_MATKEY_MAPPINGMODE_V(textureType[i], 0), yMode) == aiReturn_SUCCESS)
                wrapMode.setY(wrapModeFromaiTextureMapMode(yMode));

            tex->setWrapMode(wrapMode);

            qCDebug(AssimpImporterLog) << Q_FUNC_INFO << " Loaded Texture " << fullPath;
            setParameterValue(m_scene->m_textureToParameterName[textureType[i]],
                    material, QVariant::fromValue(tex));
        }
    }
}

/*!
 * Retrieves a \a material float property.
 */
void AssimpImporter::copyMaterialFloatProperties(QMaterial *material, aiMaterial *assimpMaterial)
{
    float value = 0;
    if (assimpMaterial->Get(AI_MATKEY_OPACITY, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_OPACITY, material, value);
    if (assimpMaterial->Get(AI_MATKEY_SHININESS, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_SHININESS, material, value);
    if (assimpMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_SHININESS_STRENGTH, material, value);
    if (assimpMaterial->Get(AI_MATKEY_REFRACTI, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_REFRACTI, material, value);
    if (assimpMaterial->Get(AI_MATKEY_REFLECTIVITY, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_REFLECTIVITY, material, value);
}

AssimpRawTextureImage::AssimpRawTextureImage(QNode *parent)
    : QAbstractTextureImage(parent)
{
}

QTextureImageDataGeneratorPtr AssimpRawTextureImage::dataGenerator() const
{
    return QTextureImageDataGeneratorPtr(new AssimpRawTextureImageFunctor(m_data));
}

void AssimpRawTextureImage::setData(const QByteArray &data)
{
    if (data != m_data) {
        m_data = data;
        notifyDataGeneratorChanged();
    }
}

AssimpRawTextureImage::AssimpRawTextureImageFunctor::AssimpRawTextureImageFunctor(const QByteArray &data)
    : QTextureImageDataGenerator()
    , m_data(data)
{
}

QTextureImageDataPtr AssimpRawTextureImage::AssimpRawTextureImageFunctor::operator()()
{
    QTextureImageDataPtr dataPtr = QTextureImageDataPtr::create();
    // Note: we assume 4 components per pixel and not compressed for now
    dataPtr->setData(m_data, 4, false);
    return dataPtr;
}

bool AssimpRawTextureImage::AssimpRawTextureImageFunctor::operator ==(const QTextureImageDataGenerator &other) const
{
    const AssimpRawTextureImageFunctor *otherFunctor = functor_cast<AssimpRawTextureImageFunctor>(&other);
    return (otherFunctor != nullptr && otherFunctor->m_data == m_data);
}

AssimpImporter::SceneImporter::SceneImporter()
    : m_importer(new Assimp::Importer())
    , m_aiScene(nullptr)
{
    // The Assimp::Importer manages the lifetime of the aiScene object
}

AssimpImporter::SceneImporter::~SceneImporter()
{
    delete m_importer;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "assimpimporter.moc"
