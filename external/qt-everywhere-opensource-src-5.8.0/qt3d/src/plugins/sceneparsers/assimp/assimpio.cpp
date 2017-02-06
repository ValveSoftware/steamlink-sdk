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

#include "assimpio.h"

#include <Qt3DCore/private/qabstractnodefactory_p.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtextureimagedatagenerator.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <QFileInfo>
#include <QColor>
#include <qmath.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/qurlhelper_p.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qgeometry.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;
using namespace Qt3DExtras;

namespace Qt3DRender {

/*!
    \class Qt3DRender::AssimpIO
    \inmodule Qt3DRender
    \since 5.5

    \brief Provides a generic way of loading various 3D assets
    format into a Qt3D scene.

    It should be noted that Assimp aiString is explicitly defined to be UTF-8.
*/

Q_LOGGING_CATEGORY(AssimpIOLog, "Qt3D.AssimpIO")

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
static inline QString aiStringToQString(const aiString &str)
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

} // anonymous

QStringList AssimpIO::assimpSupportedFormatsList = AssimpIO::assimpSupportedFormats();

/*!
 * Returns a QStringlist with the suffixes of the various supported asset formats.
 */
QStringList AssimpIO::assimpSupportedFormats()
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
    formats.append(QStringLiteral("b3d"));
    formats.append(QStringLiteral("blend"));
    formats.append(QStringLiteral("bvh"));
    formats.append(QStringLiteral("cob"));
    formats.append(QStringLiteral("csm"));
    formats.append(QStringLiteral("dae"));
    formats.append(QStringLiteral("dxf"));
    formats.append(QStringLiteral("enff"));
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
    formats.append(QStringLiteral("pk3"));
    formats.append(QStringLiteral("ply"));
    formats.append(QStringLiteral("prj"));
    formats.append(QStringLiteral("q3o"));
    formats.append(QStringLiteral("q3s"));
    formats.append(QStringLiteral("raw"));
    formats.append(QStringLiteral("scn"));
    formats.append(QStringLiteral("smd"));
    formats.append(QStringLiteral("stl"));
    formats.append(QStringLiteral("ter"));
    formats.append(QStringLiteral("uc"));
    formats.append(QStringLiteral("vta"));
    formats.append(QStringLiteral("x"));
    formats.append(QStringLiteral("xml"));
    formats.append(QStringLiteral("fbx"));

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
 *  Constructor. Initializes a new instance of AssimpIO.
 */
AssimpIO::AssimpIO() : QSceneIOHandler(),
    m_sceneParsed(false),
    m_scene(nullptr)
{
}

/*!
 * Destructor. Cleans the parser properly before destroying it.
 */
AssimpIO::~AssimpIO()
{
    cleanup();
}

/*!
 *  Returns \c true if the provided \a path has a suffix supported
 *  by the Assimp Assets importer.
 */
bool AssimpIO::isAssimpPath(const QString &path)
{
    QFileInfo fileInfo(path);

    if (!fileInfo.exists() ||
            !AssimpIO::assimpSupportedFormatsList.contains(fileInfo.suffix().toLower()))
        return false;
    return true;
}

/*!
 * Sets the \a source used by the parser to load the asset file.
 * If the file is valid, this will trigger parsing of the file.
 */
void AssimpIO::setSource(const QUrl &source)
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    QFileInfo file(path);
    m_sceneDir = file.absoluteDir();
    if (!file.exists()) {
        qCWarning(AssimpIOLog) << "File missing " << path;
        return ;
    }
    readSceneFile(path);
}

/*!
 * Returns \c true if the extension of \a source is supported by
 * the assimp parser.
 */
bool AssimpIO::isFileTypeSupported(const QUrl &source) const
{
    const QString path = QUrlHelper::urlToLocalFileOrQrc(source);
    return AssimpIO::isAssimpPath(path);
}

/*!
 * Returns a Entity node which is the root node of the scene
 * node specified by \a id. If \a id is empty, the scene is assumed to be
 * the root node of the scene.
 *
 * Returns \c nullptr if \a id was specified but no node matching it was found.
 */
Qt3DCore::QEntity *AssimpIO::scene(const QString &id)
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
        qCDebug(AssimpIOLog) << Q_FUNC_INFO << " Couldn't find requested scene node";
        return nullptr;
    }

    // Builds the Qt3D scene using the Assimp aiScene
    // and the various dicts filled previously by parse
    return node(rootNode);
}

/*!
 *  Returns a Node from the scene identified by \a id.
 *  Returns \c nullptr if the node was not found.
 */
Qt3DCore::QEntity *AssimpIO::node(const QString &id)
{
    if (m_scene == nullptr || m_scene->m_aiScene == nullptr)
        return nullptr;
    parse();
    aiNode *n = m_scene->m_aiScene->mRootNode->FindNode(id.toUtf8().constData());
    return node(n);
}

/*!
 * Returns a Node from an Assimp aiNode \a node.
 */
Qt3DCore::QEntity *AssimpIO::node(aiNode *node)
{
    if (node == nullptr)
        return nullptr;
    QEntity *entityNode = QAbstractNodeFactory::createNode<Qt3DCore::QEntity>("QEntity");
    entityNode->setObjectName(aiStringToQString(node->mName));

    // Add Meshes to the node
    for (uint i = 0; i < node->mNumMeshes; i++) {
        uint meshIdx = node->mMeshes[i];
        QGeometryRenderer *mesh = m_scene->m_meshes[meshIdx];
        // mesh material
        uint materialIndex = m_scene->m_aiScene->mMeshes[meshIdx]->mMaterialIndex;
        if (m_scene->m_materials.contains(materialIndex))
            entityNode->addComponent(m_scene->m_materials[materialIndex]);
        // mesh
        entityNode->addComponent(mesh);
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
void AssimpIO::readSceneFile(const QString &path)
{
    cleanup();

    m_scene = new SceneImporter();

    // SET THIS TO REMOVE POINTS AND LINES -> HAVE ONLY TRIANGLES
    m_scene->m_importer->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE|aiPrimitiveType_POINT);
    // SET CUSTOM FILE HANDLER TO HANDLE FILE READING THROUGH QT (RESOURCES, SOCKET ...)
    m_scene->m_importer->SetIOHandler(new AssimpHelper::AssimpIOSystem());

    // type and aiProcess_Triangulate discompose polygons with more than 3 points in triangles
    // aiProcess_SortByPType makes sur that meshes data are triangles
    m_scene->m_aiScene = m_scene->m_importer->ReadFile(path.toUtf8().constData(),
                                                       aiProcess_SortByPType|
                                                       aiProcess_Triangulate|
                                                       aiProcess_JoinIdenticalVertices|
                                                       aiProcess_GenSmoothNormals|
                                                       aiProcess_FlipUVs);
    if (m_scene->m_aiScene == nullptr) {
        qCWarning(AssimpIOLog) << "Assimp scene import failed";
        return ;
    }
    parse();
}

/*!
 * Cleans the various dictionaries holding the scene's information.
 */
void AssimpIO::cleanup()
{
    m_sceneParsed = false;
    delete m_scene;
    m_scene = nullptr;
}

/*!
 * Parses the aiScene provided py Assimp and converts Assimp
 * values to Qt3D values.
 */
void AssimpIO::parse()
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
void AssimpIO::loadMaterial(uint materialIndex)
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
void AssimpIO::loadMesh(uint meshIndex)
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

    qCDebug(AssimpIOLog) << Q_FUNC_INFO << " Mesh " << aiStringToQString(mesh->mName)
                             << " Vertices " << mesh->mNumVertices << " Faces " << mesh->mNumFaces << " Indices " << indices;
}

/*!
 * Converts the provided Assimp aiTexture at \a textureIndex to a Texture and
 * adds it to a dictionary of textures.
 * \sa Texture
 */
void AssimpIO::loadEmbeddedTexture(uint textureIndex)
{
    aiTexture *assimpTexture = m_scene->m_aiScene->mTextures[textureIndex];
    QAbstractTexture *texture = QAbstractNodeFactory::createNode<QTexture2D>("QTexture2D");
    AssimpRawTextureImage *imageData = new AssimpRawTextureImage();

    bool isCompressed = assimpTexture->mHeight == 0;
    uint textureSize = assimpTexture->mWidth *
            (isCompressed ? assimpTexture->mHeight : 1);
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
void AssimpIO::loadLight(uint lightIndex)
{
    aiLight *light = m_scene->m_aiScene->mLights[lightIndex];
    // TODO: Implement me!
    Q_UNUSED(light);
}

/*!
 * Parses the camera at cameraIndex and saves it to a dictionary of cameras.
 */
void AssimpIO::loadCamera(uint cameraIndex)
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

// OPTIONAL
void AssimpIO::loadAnimation(uint animationIndex)
{
    Q_UNUSED(animationIndex);
}

/*!
 *  Sets the object name of \a material to the name of \a assimpMaterial.
 */
void AssimpIO::copyMaterialName(QMaterial *material, aiMaterial *assimpMaterial)
{
    aiString name;
    if (assimpMaterial->Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) {
        // May not be necessary
        // Kept for debug purposes at the moment
        material->setObjectName(aiStringToQString(name));
        qCDebug(AssimpIOLog) << Q_FUNC_INFO << "Assimp Material " << material->objectName();
    }
}

/*!
 *  Fills \a material color properties with \a assimpMaterial color properties.
 */
void AssimpIO::copyMaterialColorProperties(QMaterial *material, aiMaterial *assimpMaterial)
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
void AssimpIO::copyMaterialBoolProperties(QMaterial *material, aiMaterial *assimpMaterial)
{
    int value;
    if (assimpMaterial->Get(AI_MATKEY_TWOSIDED, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_IS_TWOSIDED, material, (value == 0) ? false : true);
    if (assimpMaterial->Get(AI_MATKEY_ENABLE_WIREFRAME, value) == aiReturn_SUCCESS)
        setParameterValue(ASSIMP_MATERIAL_IS_WIREFRAME, material, (value == 0) ? false : true);
}

void AssimpIO::copyMaterialShadingModel(QMaterial *material, aiMaterial *assimpMaterial)
{
    Q_UNUSED(material);
    Q_UNUSED(assimpMaterial);
    // TODO
    // Match each shading function with a default shader

    //    AssimpIO::assimpMaterialAttributesMap[AI_MATKEY_SHADING_MODEL] = &AssimpIO::getMaterialShadingModel;
    //    AssimpIO::assimpMaterialAttributesMap[AI_MATKEY_BLEND_FUNC] = &AssimpIO::getMaterialBlendingFunction;
}

void AssimpIO::copyMaterialBlendingFunction(QMaterial *material, aiMaterial *assimpMaterial)
{
    Q_UNUSED(material);
    Q_UNUSED(assimpMaterial);
    // TO DO
}

/*!
 *
 */
void AssimpIO::copyMaterialTextures(QMaterial *material, aiMaterial *assimpMaterial)
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
            QString fullPath = m_sceneDir.absoluteFilePath(texturePath(path));
            // Load texture if not already loaded
            if (!m_scene->m_materialTextures.contains(fullPath)) {
                QAbstractTexture *tex = QAbstractNodeFactory::createNode<QTexture2D>("QTexture2D");
                QTextureImage *texImage = QAbstractNodeFactory::createNode<QTextureImage>("QTextureImage");
                texImage->setSource(QUrl::fromLocalFile(fullPath));
                tex->addTextureImage(texImage);
                m_scene->m_materialTextures.insert(fullPath, tex);
                qCDebug(AssimpIOLog) << Q_FUNC_INFO << " Loaded Texture " << fullPath;
            }
            setParameterValue(m_scene->m_textureToParameterName[textureType[i]],
                    material, QVariant::fromValue(m_scene->m_materialTextures[fullPath]));
        }
    }
}

/*!
 * Retrieves a \a material float property.
 */
void AssimpIO::copyMaterialFloatProperties(QMaterial *material, aiMaterial *assimpMaterial)
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
    QTextureImageDataPtr dataPtr;
    dataPtr->setData(m_data, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    return dataPtr;
}

bool AssimpRawTextureImage::AssimpRawTextureImageFunctor::operator ==(const QTextureImageDataGenerator &other) const
{
    const AssimpRawTextureImageFunctor *otherFunctor = functor_cast<AssimpRawTextureImageFunctor>(&other);
    return (otherFunctor != nullptr && otherFunctor->m_data == m_data);
}

AssimpIO::SceneImporter::SceneImporter()
    : m_importer(new Assimp::Importer())
    , m_aiScene(nullptr)
{
    // The Assimp::Importer manages the lifetime of the aiScene object
}

AssimpIO::SceneImporter::~SceneImporter()
{
    delete m_importer;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "assimpio.moc"
