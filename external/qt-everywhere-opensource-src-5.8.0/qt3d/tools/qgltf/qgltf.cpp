/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <qiodevice.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qhash.h>
#include <qdebug.h>
#include <qcoreapplication.h>
#include <qcommandlineparser.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <qmath.h>

#define GLT_UNSIGNED_SHORT 0x1403
#define GLT_UNSIGNED_INT 0x1405
#define GLT_FLOAT 0x1406

#define GLT_FLOAT_VEC2 0x8B50
#define GLT_FLOAT_VEC3 0x8B51
#define GLT_FLOAT_VEC4 0x8B52
#define GLT_FLOAT_MAT3 0x8B5B
#define GLT_FLOAT_MAT4 0x8B5C
#define GLT_SAMPLER_2D 0x8B5E

#define GLT_ARRAY_BUFFER 0x8892
#define GLT_ELEMENT_ARRAY_BUFFER 0x8893

#define GLT_DEPTH_TEST 0x0B71
#define GLT_CULL_FACE 0x0B44
#define GLT_BLEND 0x0BE2

class AssimpIOStream : public Assimp::IOStream
{
public:
    AssimpIOStream(QIODevice *device);
    ~AssimpIOStream();

    size_t Read(void *pvBuffer, size_t pSize, size_t pCount) Q_DECL_OVERRIDE;
    size_t Write(const void *pvBuffer, size_t pSize, size_t pCount) Q_DECL_OVERRIDE;
    aiReturn Seek(size_t pOffset, aiOrigin pOrigin) Q_DECL_OVERRIDE;
    size_t Tell() const Q_DECL_OVERRIDE;
    size_t FileSize() const Q_DECL_OVERRIDE;
    void Flush() Q_DECL_OVERRIDE;

private:
    QIODevice *m_device;
};

class AssimpIOSystem : public Assimp::IOSystem
{
public:
    AssimpIOSystem();
    bool Exists(const char *pFile) const Q_DECL_OVERRIDE;
    char getOsSeparator() const Q_DECL_OVERRIDE;
    Assimp::IOStream *Open(const char *pFile, const char *pMode) Q_DECL_OVERRIDE;
    void Close(Assimp::IOStream *pFile) Q_DECL_OVERRIDE;

private:
    QHash<QByteArray, QIODevice::OpenMode> m_openModeMap;
};

AssimpIOStream::AssimpIOStream(QIODevice *device) :
    m_device(device)
{
    Q_ASSERT(m_device);
}

AssimpIOStream::~AssimpIOStream()
{
    delete m_device;
}

size_t AssimpIOStream::Read(void *pvBuffer, size_t pSize, size_t pCount)
{
    qint64 readBytes = m_device->read((char *)pvBuffer, pSize * pCount);
    if (readBytes < 0)
        qWarning() << Q_FUNC_INFO << " read failed";
    return readBytes;
}

size_t AssimpIOStream::Write(const void *pvBuffer, size_t pSize, size_t pCount)
{
    qint64 writtenBytes = m_device->write((char *)pvBuffer, pSize * pCount);
    if (writtenBytes < 0)
        qWarning() << Q_FUNC_INFO << " write failed";
    return writtenBytes;
}

aiReturn AssimpIOStream::Seek(size_t pOffset, aiOrigin pOrigin)
{
    qint64 seekPos = pOffset;

    if (pOrigin == aiOrigin_CUR)
        seekPos += m_device->pos();
    else if (pOrigin == aiOrigin_END)
        seekPos += m_device->size();

    if (!m_device->seek(seekPos)) {
        qWarning() << Q_FUNC_INFO << " seek failed";
        return aiReturn_FAILURE;
    }
    return aiReturn_SUCCESS;
}

size_t AssimpIOStream::Tell() const
{
    return m_device->pos();
}

size_t AssimpIOStream::FileSize() const
{
    return m_device->size();
}

void AssimpIOStream::Flush()
{
    // we don't write via assimp
}

AssimpIOSystem::AssimpIOSystem()
{
    m_openModeMap[QByteArrayLiteral("r")] = QIODevice::ReadOnly;
    m_openModeMap[QByteArrayLiteral("r+")] = QIODevice::ReadWrite;
    m_openModeMap[QByteArrayLiteral("w")] = QIODevice::WriteOnly | QIODevice::Truncate;
    m_openModeMap[QByteArrayLiteral("w+")] = QIODevice::ReadWrite | QIODevice::Truncate;
    m_openModeMap[QByteArrayLiteral("a")] = QIODevice::WriteOnly | QIODevice::Append;
    m_openModeMap[QByteArrayLiteral("a+")] = QIODevice::ReadWrite | QIODevice::Append;
    m_openModeMap[QByteArrayLiteral("wb")] = QIODevice::WriteOnly;
    m_openModeMap[QByteArrayLiteral("wt")] = QIODevice::WriteOnly | QIODevice::Text;
    m_openModeMap[QByteArrayLiteral("rb")] = QIODevice::ReadOnly;
    m_openModeMap[QByteArrayLiteral("rt")] = QIODevice::ReadOnly | QIODevice::Text;
}

bool AssimpIOSystem::Exists(const char *pFile) const
{
    return QFileInfo(QString::fromUtf8(pFile)).exists();
}

char AssimpIOSystem::getOsSeparator() const
{
    return QDir::separator().toLatin1();
}

Assimp::IOStream *AssimpIOSystem::Open(const char *pFile, const char *pMode)
{
    const QString fileName(QString::fromUtf8(pFile));
    const QByteArray cleanedMode(QByteArray(pMode).trimmed());

    const QIODevice::OpenMode openMode = m_openModeMap.value(cleanedMode, QIODevice::NotOpen);

    QScopedPointer<QFile> file(new QFile(fileName));
    if (file->open(openMode))
        return new AssimpIOStream(file.take());

    return nullptr;
}

void AssimpIOSystem::Close(Assimp::IOStream *pFile)
{
    delete pFile;
}

static inline QString ai2qt(const aiString &str)
{
    return QString::fromUtf8(str.data, int(str.length));
}

static inline QVector<float> ai2qt(const aiMatrix4x4 &matrix)
{
    return QVector<float>() << matrix.a1 << matrix.b1 << matrix.c1 << matrix.d1
                            << matrix.a2 << matrix.b2 << matrix.c2 << matrix.d2
                            << matrix.a3 << matrix.b3 << matrix.c3 << matrix.d3
                            << matrix.a4 << matrix.b4 << matrix.c4 << matrix.d4;
}

struct Options {
    QString outDir;
    bool genBin;
    bool compact;
    bool compress;
    bool genTangents;
    bool interleave;
    float scale;
    bool genCore;
    enum TextureCompression {
        NoTextureCompression,
        ETC1
    };
    TextureCompression texComp;
    bool commonMat;
    bool shaders;
    bool showLog;
} opts;

class Importer
{
public:
    Importer();
    virtual ~Importer();

    virtual bool load(const QString &filename) = 0;

    struct BufferInfo {
        QString name;
        QByteArray data;
    };
    QVector<BufferInfo> buffers() const;

    struct MeshInfo {
        struct BufferView {
            BufferView() : bufIndex(0), offset(0), length(0), componentType(0), target(0) { }
            QString name;
            uint bufIndex;
            uint offset;
            uint length;
            uint componentType;
            uint target;
        };
        QVector<BufferView> views;
        struct Accessor {
            Accessor() : offset(0), stride(0), count(0), componentType(0) { }
            QString name;
            QString usage;
            QString bufferView;
            uint offset;
            uint stride;
            uint count;
            uint componentType;
            QString type;
            QVector<float> minVal;
            QVector<float> maxVal;
        };
        QVector<Accessor> accessors;
        QString name; // generated
        QString originalName; // may be empty
        uint materialIndex;
    };

    QVector<MeshInfo::BufferView> bufferViews() const;
    QVector<MeshInfo::Accessor> accessors() const;
    uint meshCount() const;
    MeshInfo meshInfo(uint meshIndex) const;

    struct MaterialInfo {
        QString name;
        QString originalName;
        QHash<QByteArray, QVector<float> > m_colors;
        QHash<QByteArray, float> m_values;
        QHash<QByteArray, QString> m_textures;
    };
    uint materialCount() const;
    MaterialInfo materialInfo(uint materialIndex) const;

    QSet<QString> externalTextures() const;

    struct CameraInfo {
        QString name; // suffixed
        float aspectRatio;
        float yfov;
        float zfar;
        float znear;
    };
    QHash<QString, CameraInfo> cameraInfo() const;

    struct EmbeddedTextureInfo {
        EmbeddedTextureInfo() { }
        QString name;
#ifdef HAS_QIMAGE
        EmbeddedTextureInfo(const QString &name, const QImage &image) : name(name), image(image) { }
        QImage image;
#endif
    };
    QHash<QString, EmbeddedTextureInfo> embeddedTextures() const;

    struct Node {
        QString name;
        QString uniqueName; // generated
        QVector<float> transformation;
        QVector<Node *> children;
        QVector<uint> meshes;
    };
    const Node *rootNode() const;

    struct KeyFrame {
        KeyFrame() : t(0), transValid(false), rotValid(false), scaleValid(false) { }
        float t;
        bool transValid;
        QVector<float> trans;
        bool rotValid;
        QVector<float> rot;
        bool scaleValid;
        QVector<float> scale;
    };
    struct AnimationInfo {
        AnimationInfo() : hasTranslation(false), hasRotation(false), hasScale(false) { }
        QString name;
        QString targetNode;
        bool hasTranslation;
        bool hasRotation;
        bool hasScale;
        QVector<KeyFrame> keyFrames;
    };
    QVector<AnimationInfo> animations() const;

    bool allMeshesForMaterialHaveTangents(uint materialIndex) const;

    const Node *findNode(const Node *root, const QString &originalName) const;

protected:
    void delNode(Importer::Node *n);

    QByteArray m_buffer;
    QHash<uint, MeshInfo> m_meshInfo;
    QHash<uint, MaterialInfo> m_materialInfo;
    QHash<QString, EmbeddedTextureInfo> m_embeddedTextures;
    QSet<QString> m_externalTextures;
    QHash<QString, CameraInfo> m_cameraInfo;
    Node *m_rootNode;
    QVector<AnimationInfo> m_animations;
};
QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(Importer::BufferInfo,           Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::MeshInfo::BufferView, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::MeshInfo::Accessor,   Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::MaterialInfo,         Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::CameraInfo,           Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::EmbeddedTextureInfo,  Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::Node,                 Q_COMPLEX_TYPE); // uses address as identity
Q_DECLARE_TYPEINFO(Importer::KeyFrame,             Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Importer::AnimationInfo,        Q_MOVABLE_TYPE);
QT_END_NAMESPACE

Importer::Importer()
    : m_rootNode(nullptr)
{
}

void Importer::delNode(Importer::Node *n)
{
    if (!n)
        return;
    for (Importer::Node *c : qAsConst(n->children))
        delNode(c);
    delete n;
}

Importer::~Importer()
{
    delNode(m_rootNode);
}

QVector<Importer::BufferInfo> Importer::buffers() const
{
    BufferInfo b;
    b.name = QStringLiteral("buf");
    b.data = m_buffer;
    return QVector<BufferInfo>() << b;
}

const Importer::Node *Importer::rootNode() const
{
    return m_rootNode;
}

bool Importer::allMeshesForMaterialHaveTangents(uint materialIndex) const
{
    for (const MeshInfo &mi : m_meshInfo) {
        if (mi.materialIndex == materialIndex) {
            bool hasTangents = false;
            for (const MeshInfo::Accessor &acc : mi.accessors) {
                if (acc.usage == QStringLiteral("TANGENT")) {
                    hasTangents = true;
                    break;
                }
            }
            if (!hasTangents)
                return false;
        }
    }
    return true;
}

QVector<Importer::MeshInfo::BufferView> Importer::bufferViews() const
{
    QVector<Importer::MeshInfo::BufferView> bv;
    for (const MeshInfo &mi : m_meshInfo) {
        for (const MeshInfo::BufferView &v : mi.views)
            bv << v;
    }
    return bv;
}

QVector<Importer::MeshInfo::Accessor> Importer::accessors() const
{
    QVector<Importer::MeshInfo::Accessor> acc;
    for (const MeshInfo &mi : m_meshInfo) {
        for (const MeshInfo::Accessor &a : mi.accessors)
            acc << a;
    }
    return acc;
}

uint Importer::meshCount() const
{
    return m_meshInfo.count();
}

Importer::MeshInfo Importer::meshInfo(uint meshIndex) const
{
    return m_meshInfo[meshIndex];
}

uint Importer::materialCount() const
{
    return m_materialInfo.count();
}

Importer::MaterialInfo Importer::materialInfo(uint materialIndex) const
{
    return m_materialInfo[materialIndex];
}

QHash<QString, Importer::CameraInfo> Importer::cameraInfo() const
{
    return m_cameraInfo;
}

QSet<QString> Importer::externalTextures() const
{
    return m_externalTextures;
}

QHash<QString, Importer::EmbeddedTextureInfo> Importer::embeddedTextures() const
{
    return m_embeddedTextures;
}

QVector<Importer::AnimationInfo> Importer::animations() const
{
    return m_animations;
}

const Importer::Node *Importer::findNode(const Node *root, const QString &originalName) const
{
    for (const Node *c : root->children) {
        if (c->name == originalName)
            return c;
        const Node *cn = findNode(c, originalName);
        if (cn)
            return cn;
    }
    return nullptr;
}

class AssimpImporter : public Importer
{
public:
    AssimpImporter();

    bool load(const QString &filename) Q_DECL_OVERRIDE;

private:
    const aiScene *scene() const;
    void printNodes(const aiNode *node, int level = 1);
    void buildBuffer();
    void parseEmbeddedTextures();
    void parseMaterials();
    void parseCameras();
    void parseNode(Importer::Node *dst, const aiNode *src);
    void parseScene();
    void parseAnimations();
    void addKeyFrame(QVector<KeyFrame> &keyFrames, float t, aiVector3D *vt, aiQuaternion *vr, aiVector3D *vs);

    QScopedPointer<Assimp::Importer> m_importer;
};

AssimpImporter::AssimpImporter() :
    m_importer(new Assimp::Importer)
{
    m_importer->SetIOHandler(new AssimpIOSystem);
    m_importer->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
}

bool AssimpImporter::load(const QString &filename)
{
    uint flags = aiProcess_Triangulate | aiProcess_SortByPType
            | aiProcess_JoinIdenticalVertices
            | aiProcess_GenSmoothNormals
            | aiProcess_GenUVCoords
            | aiProcess_FlipUVs
            | aiProcess_FindDegenerates;

    if (opts.genTangents)
        flags |= aiProcess_CalcTangentSpace;

    const aiScene *scene = m_importer->ReadFile(filename.toUtf8().constData(), flags);
    if (!scene)
        return false;

    if (opts.showLog) {
        qDebug().noquote() << filename
                           << scene->mNumMeshes << "meshes,"
                           << scene->mNumMaterials << "materials,"
                           << scene->mNumTextures << "embedded textures,"
                           << scene->mNumCameras << "cameras,"
                           << scene->mNumLights << "lights,"
                           << scene->mNumAnimations << "animations";
        qDebug() << "Scene:";
        printNodes(scene->mRootNode);
    }

    buildBuffer();
    parseEmbeddedTextures();
    parseMaterials();
    parseCameras();
    parseScene();
    parseAnimations();

    return true;
}

void AssimpImporter::printNodes(const aiNode *node, int level)
{
    qDebug().noquote() << QString().fill('-', level * 4) << ai2qt(node->mName) << node->mNumMeshes << "mesh refs";
    for (uint i = 0; i < node->mNumChildren; ++i)
        printNodes(node->mChildren[i], level + 1);
}

template<class T> void copyIndexBuf(T *dst, const aiMesh *src)
{
    for (uint j = 0; j < src->mNumFaces; ++j) {
        const aiFace *f = &src->mFaces[j];
        if (f->mNumIndices != 3)
            qFatal("Face %d is not a triangle (index count %d instead of 3)", j, f->mNumIndices);
        *dst++ = f->mIndices[0];
        *dst++ = f->mIndices[1];
        *dst++ = f->mIndices[2];
    }
}

static QString newBufferViewName()
{
    static int cnt = 0;
    return QString(QStringLiteral("bufferView_%1")).arg(++cnt);
}

static QString newAccessorName()
{
    static int cnt = 0;
    return QString(QStringLiteral("accessor_%1")).arg(++cnt);
}

static QString newMeshName()
{
    static int cnt = 0;
    return QString(QStringLiteral("mesh_%1")).arg(++cnt);
}

static QString newMaterialName()
{
    static int cnt = 0;
    return QString(QStringLiteral("material_%1")).arg(++cnt);
}

static QString newTechniqueName()
{
    static int cnt = 0;
    return QString(QStringLiteral("technique_%1")).arg(++cnt);
}

static QString newTextureName()
{
    static int cnt = 0;
    return QString(QStringLiteral("texture_%1")).arg(++cnt);
}

static QString newImageName()
{
    static int cnt = 0;
    return QString(QStringLiteral("image_%1")).arg(++cnt);
}

static QString newShaderName()
{
    static int cnt = 0;
    return QString(QStringLiteral("shader_%1")).arg(++cnt);
}

static QString newProgramName()
{
    static int cnt = 0;
    return QString(QStringLiteral("program_%1")).arg(++cnt);
}

static QString newNodeName()
{
    static int cnt = 0;
    return QString(QStringLiteral("node_%1")).arg(++cnt);
}

static QString newAnimationName()
{
    static int cnt = 0;
    return QString(QStringLiteral("animation_%1")).arg(++cnt);
}

template<class T> void calcBB(QVector<float> &minVal, QVector<float> &maxVal, T *data, int vertexCount, int compCount)
{
    minVal.resize(compCount);
    maxVal.resize(compCount);
    for (int i = 0; i < vertexCount; ++i) {
        for (int j = 0; j < compCount; ++j) {
            if (i == 0) {
                minVal[j] = maxVal[j] = data[i][j];
            } else {
                if (data[i][j] < minVal[j])
                    minVal[j] = data[i][j];
                if (data[i][j] > maxVal[j])
                    maxVal[j] = data[i][j];
            }
        }
    }
}

// One buffer per importer (scene).
// Two buffer views (array, index) + three or more accessors per mesh.

void AssimpImporter::buildBuffer()
{
    m_buffer.clear();
    m_meshInfo.clear();

    if (opts.showLog)
        qDebug() << "Meshes:";

    const aiScene *sc = scene();
    for (uint i = 0; i < sc->mNumMeshes; ++i) {
        aiMesh *m = sc->mMeshes[i];
        MeshInfo meshInfo;
        meshInfo.originalName = ai2qt(m->mName);
        meshInfo.name = newMeshName();
        meshInfo.materialIndex = m->mMaterialIndex;

        aiVector3D *vertices = m->mVertices;
        aiVector3D *normals = m->mNormals;
        aiVector3D *textureCoords = m->mTextureCoords[0];
        aiColor4D *colors = m->mColors[0];
        aiVector3D *tangents = m->mTangents;

        if (opts.scale != 1) {
            for (uint j = 0; j < m->mNumVertices; ++j) {
                vertices[j].x *= opts.scale;
                vertices[j].y *= opts.scale;
                vertices[j].z *= opts.scale;
            }
        }

        // Vertex (3), Normal (3), Coord? (2), Color? (4), Tangent? (3)
        uint stride = 3 + 3 + (textureCoords ? 2 : 0) + (colors ? 4 : 0) + (tangents ? 3 : 0);
        QByteArray vertexBuf;
        vertexBuf.resize(stride * m->mNumVertices * sizeof(float));
        float *p = reinterpret_cast<float *>(vertexBuf.data());

        if (opts.interleave) {
            for (uint j = 0; j < m->mNumVertices; ++j) {
                // Vertex
                *p++ = vertices[j].x;
                *p++ = vertices[j].y;
                *p++ = vertices[j].z;

                // Normal
                *p++ = normals[j].x;
                *p++ = normals[j].y;
                *p++ = normals[j].z;

                // Coord
                if (textureCoords) {
                    *p++ = textureCoords[j].x;
                    *p++ = textureCoords[j].y;
                }

                // Color
                if (colors) {
                    *p++ = colors[j].r;
                    *p++ = colors[j].g;
                    *p++ = colors[j].b;
                    *p++ = colors[j].a;
                }

                // Tangent
                if (tangents) {
                    *p++ = tangents[j].x;
                    *p++ = tangents[j].y;
                    *p++ = tangents[j].z;
                }
            }
        } else {
            // Vertex
            for (uint j = 0; j < m->mNumVertices; ++j) {
                *p++ = vertices[j].x;
                *p++ = vertices[j].y;
                *p++ = vertices[j].z;
            }

            // Normal
            for (uint j = 0; j < m->mNumVertices; ++j) {
                *p++ = normals[j].x;
                *p++ = normals[j].y;
                *p++ = normals[j].z;
            }

            // Coord
            if (textureCoords) {
                for (uint j = 0; j < m->mNumVertices; ++j) {
                    *p++ = textureCoords[j].x;
                    *p++ = textureCoords[j].y;
                }
            }

            // Color
            if (colors) {
                for (uint j = 0; j < m->mNumVertices; ++j) {
                    *p++ = colors[j].r;
                    *p++ = colors[j].g;
                    *p++ = colors[j].b;
                    *p++ = colors[j].a;
                }
            }

            // Tangent
            if (tangents) {
                for (uint j = 0; j < m->mNumVertices; ++j) {
                    *p++ = tangents[j].x;
                    *p++ = tangents[j].y;
                    *p++ = tangents[j].z;
                }
            }
        }

        MeshInfo::BufferView vertexBufView;
        vertexBufView.name = newBufferViewName();
        vertexBufView.length = vertexBuf.size();
        vertexBufView.offset = m_buffer.size();
        vertexBufView.componentType = GLT_FLOAT;
        vertexBufView.target = GLT_ARRAY_BUFFER;
        meshInfo.views.append(vertexBufView);

        QByteArray indexBuf;
        uint indexCount = m->mNumFaces * 3;
        if (indexCount >= USHRT_MAX) {
            indexBuf.resize(indexCount * sizeof(quint32));
            quint32 *p = reinterpret_cast<quint32 *>(indexBuf.data());
            copyIndexBuf(p, m);
        } else {
            indexBuf.resize(indexCount * sizeof(quint16));
            quint16 *p = reinterpret_cast<quint16 *>(indexBuf.data());
            copyIndexBuf(p, m);
        }

        MeshInfo::BufferView indexBufView;
        indexBufView.name = newBufferViewName();
        indexBufView.length = indexBuf.size();
        indexBufView.offset = vertexBufView.offset + vertexBufView.length;
        indexBufView.componentType = indexCount >= USHRT_MAX ? GLT_UNSIGNED_INT : GLT_UNSIGNED_SHORT;
        indexBufView.target = GLT_ELEMENT_ARRAY_BUFFER;
        meshInfo.views.append(indexBufView);

        MeshInfo::Accessor acc;
        uint startOffset = 0;
        // Vertex
        acc.name = newAccessorName();
        acc.usage = QStringLiteral("POSITION");
        acc.bufferView = vertexBufView.name;
        acc.offset = 0;
        acc.stride = opts.interleave ? stride * sizeof(float) : 3 * sizeof(float);
        acc.count = m->mNumVertices;
        acc.componentType = vertexBufView.componentType;
        acc.type = QStringLiteral("VEC3");
        calcBB(acc.minVal, acc.maxVal, vertices, m->mNumVertices, 3);
        meshInfo.accessors.append(acc);
        startOffset += opts.interleave ? 3 : 3 * m->mNumVertices;
        // Normal
        acc.name = newAccessorName();
        acc.usage = QStringLiteral("NORMAL");
        acc.offset = startOffset * sizeof(float);
        if (!opts.interleave)
            acc.stride = 3 * sizeof(float);
        calcBB(acc.minVal, acc.maxVal, normals, m->mNumVertices, 3);
        meshInfo.accessors.append(acc);
        startOffset += opts.interleave ? 3 : 3 * m->mNumVertices;
        // Coord
        if (textureCoords) {
            acc.name = newAccessorName();
            acc.usage = QStringLiteral("TEXCOORD_0");
            acc.offset = startOffset * sizeof(float);
            if (!opts.interleave)
                acc.stride = 2 * sizeof(float);
            acc.type = QStringLiteral("VEC2");
            calcBB(acc.minVal, acc.maxVal, textureCoords, m->mNumVertices, 2);
            meshInfo.accessors.append(acc);
            startOffset += opts.interleave ? 2 : 2 * m->mNumVertices;
        }
        // Color
        if (colors) {
            acc.name = newAccessorName();
            acc.usage = QStringLiteral("COLOR");
            acc.offset = startOffset * sizeof(float);
            if (!opts.interleave)
                acc.stride = 4 * sizeof(float);
            acc.type = QStringLiteral("VEC4");
            calcBB(acc.minVal, acc.maxVal, colors, m->mNumVertices, 4);
            meshInfo.accessors.append(acc);
            startOffset += opts.interleave ? 4 : 4 * m->mNumVertices;
        }
        // Tangent
        if (tangents) {
            acc.name = newAccessorName();
            acc.usage = QStringLiteral("TANGENT");
            acc.offset = startOffset * sizeof(float);
            if (!opts.interleave)
                acc.stride = 3 * sizeof(float);
            acc.type = QStringLiteral("VEC3");
            calcBB(acc.minVal, acc.maxVal, tangents, m->mNumVertices, 3);
            meshInfo.accessors.append(acc);
            startOffset += opts.interleave ? 3 : 3 * m->mNumVertices;
        }

        // Index
        acc.name = newAccessorName();
        acc.usage = QStringLiteral("INDEX");
        acc.bufferView = indexBufView.name;
        acc.offset = 0;
        acc.stride = 0;
        acc.count = indexCount;
        acc.componentType = indexBufView.componentType;
        acc.type = QStringLiteral("SCALAR");
        acc.minVal = acc.maxVal = QVector<float>();
        meshInfo.accessors.append(acc);

        if (opts.showLog) {
            qDebug().noquote() << "#" << i << "(" << meshInfo.name << "/" << meshInfo.originalName << ")"
                               << m->mNumVertices << "vertices,"
                               << m->mNumFaces << "faces," << stride << "bytes per vertex,"
                               << vertexBuf.size() << "vertex bytes," << indexBuf.size() << "index bytes";
            if (opts.scale != 1)
                qDebug() << "  scaled by" << opts.scale;
            if (!opts.interleave)
                qDebug() << "  non-interleaved layout";
            QStringList sl;
            for (const MeshInfo::BufferView &bv : qAsConst(meshInfo.views)) sl << bv.name;
            qDebug() << "  buffer views:" << sl;
            sl.clear();
            for (const MeshInfo::Accessor &acc : qAsConst(meshInfo.accessors)) sl << acc.name;
            qDebug() << "  accessors:" << sl;
            qDebug() << "  material: #" << meshInfo.materialIndex;
        }

        m_buffer.append(vertexBuf);
        m_buffer.append(indexBuf);

        m_meshInfo.insert(i, meshInfo);
    }

    if (opts.showLog)
        qDebug().noquote() << "Total buffer size" << m_buffer.size();
}

void AssimpImporter::parseEmbeddedTextures()
{
#ifdef HAS_QIMAGE
    m_embeddedTextures.clear();

    const aiScene *sc = scene();
    if (opts.showLog && sc->mNumTextures)
        qDebug() << "Embedded textures:";

    for (uint i = 0; i < sc->mNumTextures; ++i) {
        aiTexture *t = sc->mTextures[i];
        QImage img;
        if (t->mHeight == 0) {
            img = QImage::fromData(reinterpret_cast<uchar *>(t->pcData), t->mWidth);
        } else {
            uint sz = t->mWidth * t->mHeight;
            QByteArray data;
            data.resize(sz * 4);
            uchar *p = reinterpret_cast<uchar *>(data.data());
            for (uint j = 0; j < sz; ++j) {
                *p++ = t->pcData[j].r;
                *p++ = t->pcData[j].g;
                *p++ = t->pcData[j].b;
                *p++ = t->pcData[j].a;
            }
            img = QImage(reinterpret_cast<const uchar *>(data.constData()), t->mWidth, t->mHeight, QImage::Format_RGBA8888);
            img.detach();
        }
        QString name;
        static int imgCnt = 0;
        name = QString(QStringLiteral("texture_%1.png")).arg(++imgCnt);
        QString embeddedTextureRef = QStringLiteral("*") + QString::number(i); // see AI_MAKE_EMBEDDED_TEXNAME
        m_embeddedTextures.insert(embeddedTextureRef, EmbeddedTextureInfo(name, img));
        if (opts.showLog)
            qDebug().noquote() << "#" << i << name << img;
    }
#else
    if (scene()->mNumTextures)
        qWarning() << "WARNING: No image support, ignoring" << scene()->mNumTextures << "embedded textures";
#endif
}

void AssimpImporter::parseMaterials()
{
    m_materialInfo.clear();
    m_externalTextures.clear();

    if (opts.showLog)
        qDebug() << "Materials:";

    const aiScene *sc = scene();
    for (uint i = 0; i < sc->mNumMaterials; ++i) {
        const aiMaterial *mat = sc->mMaterials[i];
        MaterialInfo matInfo;
        matInfo.name = newMaterialName();

        aiString s;
        if (mat->Get(AI_MATKEY_NAME, s) == aiReturn_SUCCESS)
            matInfo.originalName = ai2qt(s);

        aiColor4D color;
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
            matInfo.m_colors.insert("diffuse", QVector<float>() << color.r << color.g << color.b << color.a);
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS)
            matInfo.m_colors.insert("specular", QVector<float>() << color.r << color.g << color.b);
        if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS)
            matInfo.m_colors.insert("ambient", QVector<float>() << color.r << color.g << color.b);

        float f;
        if (mat->Get(AI_MATKEY_SHININESS, f) == aiReturn_SUCCESS)
            matInfo.m_values.insert("shininess", f);

        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &s) == aiReturn_SUCCESS)
            matInfo.m_textures.insert("diffuse", ai2qt(s));
        if (mat->GetTexture(aiTextureType_SPECULAR, 0, &s) == aiReturn_SUCCESS)
            matInfo.m_textures.insert("specular", ai2qt(s));
        if (mat->GetTexture(aiTextureType_NORMALS, 0, &s) == aiReturn_SUCCESS)
            matInfo.m_textures.insert("normal", ai2qt(s));

        QHash<QByteArray, QString>::iterator texIt = matInfo.m_textures.begin();
        while (texIt != matInfo.m_textures.end()) {
            // Map embedded texture references to real files.
            if (texIt->startsWith('*'))
                *texIt = m_embeddedTextures[*texIt].name;
            else
                m_externalTextures.insert(*texIt);
            ++texIt;
        }

        m_materialInfo.insert(i, matInfo);

        if (opts.showLog) {
            qDebug().noquote() << "#" << i << "(" << matInfo.name << "/" << matInfo.originalName << ")";
            qDebug() << "  colors:" << matInfo.m_colors;
            qDebug() << "  values:" << matInfo.m_values;
            qDebug() << "  textures:" << matInfo.m_textures;
        }
    }
}

void AssimpImporter::parseCameras()
{
    m_cameraInfo.clear();

    if (opts.showLog)
        qDebug() << "Cameras:";

    const aiScene *sc = scene();
    for (uint i = 0; i < sc->mNumCameras; ++i) {
        const aiCamera *cam = sc->mCameras[i];
        QString name = ai2qt(cam->mName);
        CameraInfo c;

        c.name = name + QStringLiteral("_cam");
        c.aspectRatio = qFuzzyIsNull(cam->mAspect) ? 1.5f : cam->mAspect;
        c.yfov = qRadiansToDegrees(cam->mHorizontalFOV);
        if (c.yfov < 10) // this can't be right
            c.yfov = 45;
        c.znear = cam->mClipPlaneNear;
        c.zfar = cam->mClipPlaneFar;

        // Collada / glTF cameras point in -Z by default, the rest is in the
        // node matrix, no separate look-at params given here.

        m_cameraInfo.insert(name, c);

        if (opts.showLog)
            qDebug().noquote() << "#" << i << "(" << name << ")" << c.aspectRatio << c.yfov << c.znear << c.zfar;
    }
}

void AssimpImporter::parseNode(Importer::Node *dst, const aiNode *src)
{
    dst->name = ai2qt(src->mName);
    dst->uniqueName = newNodeName();
    for (uint j = 0; j < src->mNumChildren; ++j) {
        Node *c = new Node;
        parseNode(c, src->mChildren[j]);
        dst->children << c;
    }
    dst->transformation = ai2qt(src->mTransformation);
    for (uint j = 0; j < src->mNumMeshes; ++j)
        dst->meshes << src->mMeshes[j];
}

void AssimpImporter::parseScene()
{
    delNode(m_rootNode);
    const aiScene *sc = scene();
    m_rootNode = new Node;
    parseNode(m_rootNode, sc->mRootNode);
}

void AssimpImporter::addKeyFrame(QVector<KeyFrame> &keyFrames, float t, aiVector3D *vt, aiQuaternion *vr, aiVector3D *vs)
{
    KeyFrame kf;
    int idx = -1;
    for (int i = 0; i < keyFrames.count(); ++i) {
        if (qFuzzyCompare(keyFrames[i].t, t)) {
            kf = keyFrames[i];
            idx = i;
            break;
        }
    }

    kf.t = t;
    if (vt) {
        kf.transValid = true;
        kf.trans = QVector<float>() << vt->x << vt->y << vt->z;
    }
    if (vr) {
        kf.rotValid = true;
        kf.rot = QVector<float>() << vr->w << vr->x << vr->y << vr->z;
    }
    if (vs) {
        kf.scaleValid = true;
        kf.scale = QVector<float>() << vs->x << vs->y << vs->z;
    }

    if (idx >= 0)
        keyFrames[idx] = kf;
    else
        keyFrames.append(kf);
}

void AssimpImporter::parseAnimations()
{
    const aiScene *sc = scene();
    if (opts.showLog && sc->mNumAnimations)
        qDebug() << "Animations:";

    for (uint i = 0; i < sc->mNumAnimations; ++i) {
        const aiAnimation *anim = sc->mAnimations[i];

        // Only care about node animations.
        for (uint j = 0; j < anim->mNumChannels; ++j) {
            const aiNodeAnim *a = anim->mChannels[j];
            AnimationInfo animInfo;
            QVector<KeyFrame> keyFrames;

            if (opts.showLog)
                qDebug().noquote() << ai2qt(anim->mName) << "->" << ai2qt(a->mNodeName);

            // Target values in the keyframes are local absolute (relative to parent, like node.matrix).
            for (uint kf = 0; kf < a->mNumPositionKeys; ++kf) {
                float t = float(a->mPositionKeys[kf].mTime);
                aiVector3D v = a->mPositionKeys[kf].mValue;
                animInfo.hasTranslation = true;
                addKeyFrame(keyFrames, t, &v, nullptr, nullptr);
            }
            for (uint kf = 0; kf < a->mNumRotationKeys; ++kf) {
                float t = float(a->mRotationKeys[kf].mTime);
                aiQuaternion v = a->mRotationKeys[kf].mValue;
                animInfo.hasRotation = true;
                addKeyFrame(keyFrames, t, nullptr, &v, nullptr);
            }
            for (uint kf = 0; kf < a->mNumScalingKeys; ++kf) {
                float t = float(a->mScalingKeys[kf].mTime);
                aiVector3D v = a->mScalingKeys[kf].mValue;
                animInfo.hasScale = true;
                addKeyFrame(keyFrames, t, nullptr, nullptr, &v);
            }

            // Here we should ideally get rid of non-animated properties (that
            // just set the t-r-s value from node.matrix in every frame) but
            // let's leave that as a future exercise.

            if (!keyFrames.isEmpty()) {
                animInfo.name = ai2qt(anim->mName);
                QString nodeName = ai2qt(a->mNodeName); // have to map to our generated, unique node names
                const Node *targetNode = findNode(m_rootNode, nodeName);
                if (targetNode)
                    animInfo.targetNode = targetNode->uniqueName;
                else
                    qWarning().noquote() << "ERROR: Cannot find target node" << nodeName << "for animation" << animInfo.name;
                animInfo.keyFrames = keyFrames;
                m_animations << animInfo;

                if (opts.showLog) {
                    for (const KeyFrame &kf : qAsConst(keyFrames)) {
                        QString msg;
                        QTextStream s(&msg);
                        s << "  @ " << kf.t;
                        if (kf.transValid)
                            s << " T=(" << kf.trans[0] << ", " << kf.trans[1] << ", " << kf.trans[2] << ")";
                        if (kf.rotValid)
                            s << " R=(w=" << kf.rot[0] << ", " << kf.rot[1] << ", " << kf.rot[2] << ", " << kf.rot[3] << ")";
                        if (kf.scaleValid)
                            s << " S=(" << kf.scale[0] << ", " << kf.scale[1] << ", " << kf.scale[2] << ")";
                        qDebug().noquote() << msg;
                    }
                }
            }
        }
    }
}

const aiScene *AssimpImporter::scene() const
{
    return m_importer->GetScene();
}

class Exporter
{
public:
    Exporter(Importer *importer) : m_importer(importer) { }
    virtual ~Exporter() { }

    virtual void save(const QString &inputFilename) = 0;

protected:
    bool nodeIsUseful(const Importer::Node *n) const;
    void copyExternalTextures(const QString &inputFilename);
    void exportEmbeddedTextures();
    void compressTextures();

    Importer *m_importer;
    QSet<QString> m_files;
    QHash<QString, QString> m_compressedTextures;
};

bool Exporter::nodeIsUseful(const Importer::Node *n) const
{
    if (!n->meshes.isEmpty() || m_importer->cameraInfo().contains(n->name))
        return true;

    for (const Importer::Node *c : n->children) {
        if (nodeIsUseful(c))
            return true;
    }

    return false;
}

void Exporter::copyExternalTextures(const QString &inputFilename)
{
    const auto textureFilenames = m_importer->externalTextures();
    for (const QString &textureFilename : textureFilenames) {
        const QString dst = opts.outDir + textureFilename;
        m_files.insert(QFileInfo(dst).fileName());
        // External textures need copying only when output dir was specified.
        if (!opts.outDir.isEmpty()) {
            const QString src = QFileInfo(inputFilename).path() + QStringLiteral("/") + textureFilename;
            if (QFileInfo(src).absolutePath() != QFileInfo(dst).absolutePath()) {
                if (opts.showLog)
                    qDebug().noquote() << "Copying" << src << "to" << dst;
                QFile(src).copy(dst);
            }
        }
    }
}

void Exporter::exportEmbeddedTextures()
{
#ifdef HAS_QIMAGE
    const auto embeddedTextures = m_importer->embeddedTextures();
    for (const Importer::EmbeddedTextureInfo &embTex : embeddedTextures) {
        QString fn = opts.outDir + embTex.name;
        m_files.insert(QFileInfo(fn).fileName());
        if (opts.showLog)
            qDebug().noquote() << "Writing" << fn;
        embTex.image.save(fn);
    }
#endif
}

void Exporter::compressTextures()
{
    if (opts.texComp != Options::ETC1)
        return;

    const auto textureFilenames = m_importer->externalTextures();
    const auto embeddedTextures = m_importer->embeddedTextures();
    QStringList imageList;
    imageList.reserve(textureFilenames.size() + embeddedTextures.size());
    for (const QString &textureFilename : textureFilenames)
        imageList << opts.outDir + textureFilename;
    for (const Importer::EmbeddedTextureInfo &embTex : embeddedTextures)
        imageList << opts.outDir + embTex.name;

    for (const QString &filename : qAsConst(imageList)) {
        if (QFileInfo(filename).suffix().toLower() != QStringLiteral("png"))
            continue;
        QByteArray cmd = QByteArrayLiteral("etc1tool ");
        cmd += filename.toUtf8();
        qDebug().noquote() << "Invoking" << cmd;
        // No QProcess in bootstrap
        if (system(cmd.constData()) == -1) {
            qWarning() << "ERROR: Failed to launch etc1tool";
        } else {
            QString src = QFileInfo(filename).fileName();
            QString dst = QFileInfo(src).baseName() + QStringLiteral(".pkm");
            m_compressedTextures.insert(src, dst);
            m_files.remove(src);
            m_files.insert(dst);
        }
    }
}

class GltfExporter : public Exporter
{
public:
    GltfExporter(Importer *importer);
    void save(const QString &inputFilename) Q_DECL_OVERRIDE;

private:
    struct ProgramInfo {
        struct Param {
            Param() : type(0) { }
            Param(QString name, QString nameInShader, QString semantic, uint type)
                : name(name), nameInShader(nameInShader), semantic(semantic), type(type) { }
            QString name;
            QString nameInShader;
            QString semantic;
            uint type;
        };
        QString commonTechniqueName;
        QString vertShader;
        QString fragShader;
        QVector<Param> attributes;
        QVector<Param> uniforms;
    };
    friend class QTypeInfo<ProgramInfo>;
    friend class QTypeInfo<ProgramInfo::Param>;

    void writeShader(const QString &src, const QString &dst, const QVector<QPair<QByteArray, QByteArray> > &substTab);
    QString exportNode(const Importer::Node *n, QJsonObject &nodes);
    void exportMaterials(QJsonObject &materials, QHash<QString, QString> *textureNameMap);
    void exportParameter(QJsonObject &dst, const QVector<ProgramInfo::Param> &params);
    void exportTechniques(QJsonObject &obj, const QString &basename);
    void exportAnimations(QJsonObject &obj, QVector<Importer::BufferInfo> &bufList,
                          QVector<Importer::MeshInfo::BufferView> &bvList,
                          QVector<Importer::MeshInfo::Accessor> &accList);
    void initShaderInfo();
    ProgramInfo *chooseProgram(uint materialIndex);

    QJsonObject m_obj;
    QJsonDocument m_doc;
    QVector<ProgramInfo> m_progs;

    struct TechniqueInfo {
        TechniqueInfo() : opaque(true), prog(nullptr) { }
        TechniqueInfo(const QString &name, bool opaque, ProgramInfo *prog)
            : name(name)
            , opaque(opaque)
            , prog(prog)
        {
            coreName = name + QStringLiteral("_core");
            gl2Name = name + QStringLiteral("_gl2");
        }
        QString name;
        QString coreName;
        QString gl2Name;
        bool opaque;
        ProgramInfo *prog;
    };
    friend class QTypeInfo<TechniqueInfo>;
    QVector<TechniqueInfo> m_techniques;
    QSet<ProgramInfo *> m_usedPrograms;

    QVector<QPair<QByteArray, QByteArray> > m_subst_es2;
    QVector<QPair<QByteArray, QByteArray> > m_subst_core;

    QHash<QString, bool> m_imageHasAlpha;
};
QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(GltfExporter::ProgramInfo,        Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(GltfExporter::ProgramInfo::Param, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(GltfExporter::TechniqueInfo,      Q_MOVABLE_TYPE);
QT_END_NAMESPACE

GltfExporter::GltfExporter(Importer *importer)
    : Exporter(importer)
{
    initShaderInfo();
}

struct Shader {
    const char *name;
    const char *text;
} shaders[] = {
    {
        "color.vert",
"$VERSION\n"
"$ATTRIBUTE vec3 vertexPosition;\n"
"$ATTRIBUTE vec3 vertexNormal;\n"
"$VVARYING vec3 vPosition;\n"
"$VVARYING vec3 vNormal;\n"
"uniform mat4 projection;\n"
"uniform mat4 modelView;\n"
"uniform mat3 modelViewNormal;\n"
"void main()\n"
"{\n"
"    vNormal = normalize( modelViewNormal * vertexNormal );\n"
"    vPosition = vec3( modelView * vec4( vertexPosition, 1.0 ) );\n"
"    gl_Position = projection * modelView * vec4( vertexPosition, 1.0 );\n"
"}\n"
    },
    {
        "color.frag",
"$VERSION\n"
"uniform $HIGHP vec4 lightPosition;\n"
"uniform $HIGHP vec3 lightIntensity;\n"
"uniform $HIGHP vec3 ka;\n"
"uniform $HIGHP vec4 kd;\n"
"uniform $HIGHP vec3 ks;\n"
"uniform $HIGHP float shininess;\n"
"$FVARYING $HIGHP vec3 vPosition;\n"
"$FVARYING $HIGHP vec3 vNormal;\n"
"$DECL_FRAGCOLOR\n"
"$HIGHP vec3 adsModel( const $HIGHP vec3 pos, const $HIGHP vec3 n )\n"
"{\n"
"    $HIGHP vec3 s = normalize( vec3( lightPosition ) - pos );\n"
"    $HIGHP vec3 v = normalize( -pos );\n"
"    $HIGHP vec3 r = reflect( -s, n );\n"
"    $HIGHP float diffuse = max( dot( s, n ), 0.0 );\n"
"    $HIGHP float specular = 0.0;\n"
"    if ( dot( s, n ) > 0.0 )\n"
"        specular = pow( max( dot( r, v ), 0.0 ), shininess );\n"
"    return lightIntensity * ( ka + kd.rgb * diffuse + ks * specular );\n"
"}\n"
"void main()\n"
"{\n"
"    $FRAGCOLOR = vec4( adsModel( vPosition, normalize( vNormal ) ) * kd.a, kd.a );\n"
"}\n"
    },
    {
        "diffusemap.vert",
"$VERSION\n"
"$ATTRIBUTE vec3 vertexPosition;\n"
"$ATTRIBUTE vec3 vertexNormal;\n"
"$ATTRIBUTE vec2 vertexTexCoord;\n"
"$VVARYING vec3 vPosition;\n"
"$VVARYING vec3 vNormal;\n"
"$VVARYING vec2 vTexCoord;\n"
"uniform mat4 projection;\n"
"uniform mat4 modelView;\n"
"uniform mat3 modelViewNormal;\n"
"void main()\n"
"{\n"
"    vTexCoord = vertexTexCoord;\n"
"    vNormal = normalize( modelViewNormal * vertexNormal );\n"
"    vPosition = vec3( modelView * vec4( vertexPosition, 1.0 ) );\n"
"    gl_Position = projection * modelView * vec4( vertexPosition, 1.0 );\n"
"}\n"
    },
    {
        "diffusemap.frag",
"$VERSION\n"
"uniform $HIGHP vec4 lightPosition;\n"
"uniform $HIGHP vec3 lightIntensity;\n"
"uniform $HIGHP vec3 ka;\n"
"uniform $HIGHP vec3 ks;\n"
"uniform $HIGHP float shininess;\n"
"uniform sampler2D diffuseTexture;\n"
"$FVARYING $HIGHP vec3 vPosition;\n"
"$FVARYING $HIGHP vec3 vNormal;\n"
"$FVARYING $HIGHP vec2 vTexCoord;\n"
"$DECL_FRAGCOLOR\n"
"$HIGHP vec4 adsModel( const $HIGHP vec3 pos, const $HIGHP vec3 n )\n"
"{\n"
"    $HIGHP vec3 s = normalize( vec3( lightPosition ) - pos );\n"
"    $HIGHP vec3 v = normalize( -pos );\n"
"    $HIGHP vec3 r = reflect( -s, n );\n"
"    $HIGHP float diffuse = max( dot( s, n ), 0.0 );\n"
"    $HIGHP float specular = 0.0;\n"
"    if ( dot( s, n ) > 0.0 )\n"
"        specular = pow( max( dot( r, v ), 0.0 ), shininess );\n"
"    $HIGHP vec4 kd = $TEXTURE2D( diffuseTexture, vTexCoord );\n"
"    return vec4( lightIntensity * ( ka + kd.rgb * diffuse + ks * specular ) * kd.a, kd.a );\n"
"}\n"
"void main()\n"
"{\n"
"    $FRAGCOLOR = adsModel( vPosition, normalize( vNormal ) );\n"
"}\n"
    },
    {
        "diffusespecularmap.frag",
"$VERSION\n"
"uniform $HIGHP vec4 lightPosition;\n"
"uniform $HIGHP vec3 lightIntensity;\n"
"uniform $HIGHP vec3 ka;\n"
"uniform $HIGHP float shininess;\n"
"uniform sampler2D diffuseTexture;\n"
"uniform sampler2D specularTexture;\n"
"$FVARYING $HIGHP vec3 vPosition;\n"
"$FVARYING $HIGHP vec3 vNormal;\n"
"$FVARYING $HIGHP vec2 vTexCoord;\n"
"$DECL_FRAGCOLOR\n"
"$HIGHP vec4 adsModel( const in $HIGHP vec3 pos, const in $HIGHP vec3 n )\n"
"{\n"
"    $HIGHP vec3 s = normalize( vec3( lightPosition ) - pos );\n"
"    $HIGHP vec3 v = normalize( -pos );\n"
"    $HIGHP vec3 r = reflect( -s, n );\n"
"    $HIGHP float diffuse = max( dot( s, n ), 0.0 );\n"
"    $HIGHP float specular = 0.0;\n"
"    if ( dot( s, n ) > 0.0 )\n"
"        specular = ( shininess / ( 8.0 * 3.14 ) ) * pow( max( dot( r, v ), 0.0 ), shininess );\n"
"    $HIGHP vec4 kd = $TEXTURE2D( diffuseTexture, vTexCoord );\n"
"    $HIGHP vec3 ks = $TEXTURE2D( specularTexture, vTexCoord );\n"
"    return vec4( lightIntensity * ( ka + kd.rgb * diffuse + ks * specular ) * kd.a, kd.a );\n"
"}\n"
"void main()\n"
"{\n"
"    $FRAGCOLOR = vec4( adsModel( vPosition, normalize( vNormal ) ), 1.0 );\n"
"}\n"
    },
    {
        "normaldiffusemap.vert",
"$VERSION\n"
"$ATTRIBUTE vec3 vertexPosition;\n"
"$ATTRIBUTE vec3 vertexNormal;\n"
"$ATTRIBUTE vec2 vertexTexCoord;\n"
"$ATTRIBUTE vec4 vertexTangent;\n"
"$VVARYING vec3 lightDir;\n"
"$VVARYING vec3 viewDir;\n"
"$VVARYING vec2 texCoord;\n"
"uniform mat4 projection;\n"
"uniform mat4 modelView;\n"
"uniform mat3 modelViewNormal;\n"
"uniform vec4 lightPosition;\n"
"void main()\n"
"{\n"
"    texCoord = vertexTexCoord;\n"
"    vec3 normal = normalize( modelViewNormal * vertexNormal );\n"
"    vec3 tangent = normalize( modelViewNormal * vertexTangent.xyz );\n"
"    vec3 position = vec3( modelView * vec4( vertexPosition, 1.0 ) );\n"
"    vec3 binormal = normalize( cross( normal, tangent ) );\n"
"    mat3 tangentMatrix = mat3 (\n"
"        tangent.x, binormal.x, normal.x,\n"
"        tangent.y, binormal.y, normal.y,\n"
"        tangent.z, binormal.z, normal.z );\n"
"    vec3 s = vec3( lightPosition ) - position;\n"
"    lightDir = normalize( tangentMatrix * s );\n"
"    vec3 v = -position;\n"
"    viewDir = normalize( tangentMatrix * v );\n"
"    gl_Position = projection * modelView * vec4( vertexPosition, 1.0 );\n"
"}\n"
    },
    {
        "normaldiffusemap.frag",
"$VERSION\n"
"uniform $HIGHP vec3 lightIntensity;\n"
"uniform $HIGHP vec3 ka;\n"
"uniform $HIGHP vec3 ks;\n"
"uniform $HIGHP float shininess;\n"
"uniform sampler2D diffuseTexture;\n"
"uniform sampler2D normalTexture;\n"
"$FVARYING $HIGHP vec3 lightDir;\n"
"$FVARYING $HIGHP vec3 viewDir;\n"
"$FVARYING $HIGHP vec2 texCoord;\n"
"$DECL_FRAGCOLOR\n"
"$HIGHP vec3 adsModel( const $HIGHP vec3 norm, const $HIGHP vec3 diffuseReflect)\n"
"{\n"
"    $HIGHP vec3 r = reflect( -lightDir, norm );\n"
"    $HIGHP vec3 ambient = lightIntensity * ka;\n"
"    $HIGHP float sDotN = max( dot( lightDir, norm ), 0.0 );\n"
"    $HIGHP vec3 diffuse = lightIntensity * diffuseReflect * sDotN;\n"
"    $HIGHP vec3 ambientAndDiff = ambient + diffuse;\n"
"    $HIGHP vec3 spec = vec3( 0.0 );\n"
"    if ( sDotN > 0.0 )\n"
"        spec = lightIntensity * ks * pow( max( dot( r, viewDir ), 0.0 ), shininess );\n"
"    return ambientAndDiff + spec;\n"
"}\n"
"void main()\n"
"{\n"
"    $HIGHP vec4 kd = $TEXTURE2D( diffuseTexture, texCoord );\n"
"    $HIGHP vec4 normal = 2.0 * $TEXTURE2D( normalTexture, texCoord ) - vec4( 1.0 );\n"
"    $FRAGCOLOR = vec4( adsModel( normalize( normal.xyz ), kd.rgb) * kd.a, kd.a );\n"
"}\n"
    },
    {
        "normaldiffusespecularmap.frag",
"$VERSION\n"
"uniform $HIGHP vec3 lightIntensity;\n"
"uniform $HIGHP vec3 ka;\n"
"uniform $HIGHP float shininess;\n"
"uniform sampler2D diffuseTexture;\n"
"uniform sampler2D specularTexture;\n"
"uniform sampler2D normalTexture;\n"
"$FVARYING $HIGHP vec3 lightDir;\n"
"$FVARYING $HIGHP vec3 viewDir;\n"
"$FVARYING $HIGHP vec2 texCoord;\n"
"$DECL_FRAGCOLOR\n"
"$HIGHP vec3 adsModel( const $HIGHP vec3 norm, const $HIGHP vec3 diffuseReflect, const $HIGHP vec3 specular )\n"
"{\n"
"    $HIGHP vec3 r = reflect( -lightDir, norm );\n"
"    $HIGHP vec3 ambient = lightIntensity * ka;\n"
"    $HIGHP float sDotN = max( dot( lightDir, norm ), 0.0 );\n"
"    $HIGHP vec3 diffuse = lightIntensity * diffuseReflect * sDotN;\n"
"    $HIGHP vec3 ambientAndDiff = ambient + diffuse;\n"
"    $HIGHP vec3 spec = vec3( 0.0 );\n"
"    if ( sDotN > 0.0 )\n"
"        spec = lightIntensity * ( shininess / ( 8.0 * 3.14 ) ) * pow( max( dot( r, viewDir ), 0.0 ), shininess );\n"
"    return (ambientAndDiff + spec * specular.rgb);\n"
"}\n"
"void main()\n"
"{\n"
"    $HIGHP vec4 kd = $TEXTURE2D( diffuseTexture, texCoord );\n"
"    $HIGHP vec3 ks = $TEXTURE2D( specularTexture, texCoord );\n"
"    $HIGHP vec4 normal = 2.0 * $TEXTURE2D( normalTexture, texCoord ) - vec4( 1.0 );\n"
"    $FRAGCOLOR = vec4( adsModel( normalize( normal.xyz ), kd.rgb, ks ) * kd.a, kd.a );\n"
"}\n"
    }
};

void GltfExporter::initShaderInfo()
{
    ProgramInfo p;

    p = ProgramInfo();
    p.commonTechniqueName = "PHONG"; // diffuse RGBA, specular RGBA
    p.vertShader = "color.vert";
    p.fragShader = "color.frag";
    p.attributes << ProgramInfo::Param("position", "vertexPosition", "POSITION", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("normal", "vertexNormal", "NORMAL", GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("projection", "projection", "PROJECTION", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("modelView", "modelView", "MODELVIEW", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("normalMatrix", "modelViewNormal", "MODELVIEWINVERSETRANSPOSE", GLT_FLOAT_MAT3);
    p.uniforms << ProgramInfo::Param("lightPosition", "lightPosition", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("lightIntensity", "lightIntensity", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("ambient", "ka", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("diffuse", "kd", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("specular", "ks", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("shininess", "shininess", QString(), GLT_FLOAT);
    m_progs << p;

    p = ProgramInfo();
    p.commonTechniqueName = "PHONG"; //  diffuse texture, specular RGBA
    p.vertShader = "diffusemap.vert";
    p.fragShader = "diffusemap.frag";
    p.attributes << ProgramInfo::Param("position", "vertexPosition", "POSITION", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("normal", "vertexNormal", "NORMAL", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("texcoord0", "vertexTexCoord", "TEXCOORD_0", GLT_FLOAT_VEC2);
    p.uniforms << ProgramInfo::Param("projection", "projection", "PROJECTION", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("modelView", "modelView", "MODELVIEW", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("normalMatrix", "modelViewNormal", "MODELVIEWINVERSETRANSPOSE", GLT_FLOAT_MAT3);
    p.uniforms << ProgramInfo::Param("lightPosition", "lightPosition", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("lightIntensity", "lightIntensity", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("ambient", "ka", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("specular", "ks", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("shininess", "shininess", QString(), GLT_FLOAT);
    p.uniforms << ProgramInfo::Param("diffuse", "diffuseTexture", QString(), GLT_SAMPLER_2D);
    m_progs << p;

    p = ProgramInfo();
    p.commonTechniqueName = "PHONG"; // diffuse texture, specular texture
    p.vertShader = "diffusemap.vert";
    p.fragShader = "diffusespecularmap.frag";
    p.attributes << ProgramInfo::Param("position", "vertexPosition", "POSITION", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("normal", "vertexNormal", "NORMAL", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("texcoord0", "vertexTexCoord", "TEXCOORD_0", GLT_FLOAT_VEC2);
    p.uniforms << ProgramInfo::Param("projection", "projection", "PROJECTION", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("modelView", "modelView", "MODELVIEW", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("normalMatrix", "modelViewNormal", "MODELVIEWINVERSETRANSPOSE", GLT_FLOAT_MAT3);
    p.uniforms << ProgramInfo::Param("lightPosition", "lightPosition", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("lightIntensity", "lightIntensity", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("ambient", "ka", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("shininess", "shininess", QString(), GLT_FLOAT);
    p.uniforms << ProgramInfo::Param("diffuse", "diffuseTexture", QString(), GLT_SAMPLER_2D);
    p.uniforms << ProgramInfo::Param("specular", "specularTexture", QString(), GLT_SAMPLER_2D);
    m_progs << p;

    p = ProgramInfo();
    p.commonTechniqueName = "PHONG"; // diffuse texture, specular RGBA, normalmap texture
    p.vertShader = "normaldiffusemap.vert";
    p.fragShader = "normaldiffusemap.frag";
    p.attributes << ProgramInfo::Param("position", "vertexPosition", "POSITION", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("normal", "vertexNormal", "NORMAL", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("texcoord0", "vertexTexCoord", "TEXCOORD_0", GLT_FLOAT_VEC2);
    p.attributes << ProgramInfo::Param("tangent", "vertexTangent", "TANGENT", GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("projection", "projection", "PROJECTION", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("modelView", "modelView", "MODELVIEW", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("normalMatrix", "modelViewNormal", "MODELVIEWINVERSETRANSPOSE", GLT_FLOAT_MAT3);
    p.uniforms << ProgramInfo::Param("lightPosition", "lightPosition", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("lightIntensity", "lightIntensity", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("ambient", "ka", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("specular", "ks", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("shininess", "shininess", QString(), GLT_FLOAT);
    p.uniforms << ProgramInfo::Param("diffuse", "diffuseTexture", QString(), GLT_SAMPLER_2D);
    p.uniforms << ProgramInfo::Param("normalmap", "normalTexture", QString(), GLT_SAMPLER_2D);
    m_progs << p;

    p = ProgramInfo();
    p.commonTechniqueName = "PHONG"; // diffuse texture, specular texture, normalmap texture
    p.vertShader = "normaldiffusemap.vert";
    p.fragShader = "normaldiffusespecularmap.frag";
    p.attributes << ProgramInfo::Param("position", "vertexPosition", "POSITION", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("normal", "vertexNormal", "NORMAL", GLT_FLOAT_VEC3);
    p.attributes << ProgramInfo::Param("texcoord0", "vertexTexCoord", "TEXCOORD_0", GLT_FLOAT_VEC2);
    p.attributes << ProgramInfo::Param("tangent", "vertexTangent", "TANGENT", GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("projection", "projection", "PROJECTION", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("modelView", "modelView", "MODELVIEW", GLT_FLOAT_MAT4);
    p.uniforms << ProgramInfo::Param("normalMatrix", "modelViewNormal", "MODELVIEWINVERSETRANSPOSE", GLT_FLOAT_MAT3);
    p.uniforms << ProgramInfo::Param("lightPosition", "lightPosition", QString(), GLT_FLOAT_VEC4);
    p.uniforms << ProgramInfo::Param("lightIntensity", "lightIntensity", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("ambient", "ka", QString(), GLT_FLOAT_VEC3);
    p.uniforms << ProgramInfo::Param("shininess", "shininess", QString(), GLT_FLOAT);
    p.uniforms << ProgramInfo::Param("diffuse", "diffuseTexture", QString(), GLT_SAMPLER_2D);
    p.uniforms << ProgramInfo::Param("specular", "specularTexture", QString(), GLT_SAMPLER_2D);
    p.uniforms << ProgramInfo::Param("normalmap", "normalTexture", QString(), GLT_SAMPLER_2D);
    m_progs << p;

    m_subst_es2 << qMakePair(QByteArrayLiteral("$VERSION"), QByteArray());
    m_subst_es2 << qMakePair(QByteArrayLiteral("$ATTRIBUTE"), QByteArrayLiteral("attribute"));
    m_subst_es2 << qMakePair(QByteArrayLiteral("$VVARYING"), QByteArrayLiteral("varying"));
    m_subst_es2 << qMakePair(QByteArrayLiteral("$FVARYING"), QByteArrayLiteral("varying"));
    m_subst_es2 << qMakePair(QByteArrayLiteral("$TEXTURE2D"), QByteArrayLiteral("texture2D"));
    m_subst_es2 << qMakePair(QByteArrayLiteral("$DECL_FRAGCOLOR"), QByteArray());
    m_subst_es2 << qMakePair(QByteArrayLiteral("$FRAGCOLOR"), QByteArrayLiteral("gl_FragColor"));
    m_subst_es2 << qMakePair(QByteArrayLiteral("$HIGHP"), QByteArrayLiteral("highp"));

    m_subst_core << qMakePair(QByteArrayLiteral("$VERSION"), QByteArrayLiteral("#version 150 core"));
    m_subst_core << qMakePair(QByteArrayLiteral("$ATTRIBUTE"), QByteArrayLiteral("in"));
    m_subst_core << qMakePair(QByteArrayLiteral("$VVARYING"), QByteArrayLiteral("out"));
    m_subst_core << qMakePair(QByteArrayLiteral("$FVARYING"), QByteArrayLiteral("in"));
    m_subst_core << qMakePair(QByteArrayLiteral("$TEXTURE2D"), QByteArrayLiteral("texture"));
    m_subst_core << qMakePair(QByteArrayLiteral("$DECL_FRAGCOLOR"), QByteArrayLiteral("out vec4 fragColor;"));
    m_subst_core << qMakePair(QByteArrayLiteral("$FRAGCOLOR"), QByteArrayLiteral("fragColor"));
    m_subst_core << qMakePair(QByteArrayLiteral("$HIGHP "), QByteArray());
}

GltfExporter::ProgramInfo *GltfExporter::chooseProgram(uint materialIndex)
{
    Importer::MaterialInfo matInfo = m_importer->materialInfo(materialIndex);
    const bool hasNormalTexture = matInfo.m_textures.contains("normal");
    const bool hasSpecularTexture = matInfo.m_textures.contains("specular");
    const bool hasDiffuseTexture = matInfo.m_textures.contains("diffuse");

    if (hasNormalTexture && !m_importer->allMeshesForMaterialHaveTangents(materialIndex))
        qWarning() << "WARNING: Tangent vectors not exported while the material requires it. (hint: try -t)";

    if (hasNormalTexture && hasSpecularTexture && hasDiffuseTexture) {
        if (opts.showLog)
            qDebug() << "Using program taking diffuse, specular, normal textures";
        return &m_progs[4];
    }

    if (hasNormalTexture && hasDiffuseTexture) {
        if (opts.showLog)
            qDebug() << "Using program taking diffuse, normal textures";
        return &m_progs[3];
    }

    if (hasSpecularTexture && hasDiffuseTexture) {
        if (opts.showLog)
            qDebug() << "Using program taking diffuse, specular textures";
        return &m_progs[2];
    }

    if (hasDiffuseTexture) {
        if (opts.showLog)
            qDebug() << "Using program taking diffuse texture";
        return &m_progs[1];
    }

    if (opts.showLog)
        qDebug() << "Using program without textures";
    return &m_progs[0];
}

QString GltfExporter::exportNode(const Importer::Node *n, QJsonObject &nodes)
{
    QJsonObject node;
    node["name"] = n->name;
    QJsonArray children;
    for (const Importer::Node *c : n->children) {
        if (nodeIsUseful(c))
            children << exportNode(c, nodes);
    }
    node["children"] = children;
    QJsonArray matrix;
    const float *mtxp = n->transformation.constData();
    for (int j = 0; j < 16; ++j)
        matrix.append(*mtxp++);
    node["matrix"] = matrix;
    QJsonArray meshList;
    for (int j = 0; j < n->meshes.count(); ++j)
        meshList.append(m_importer->meshInfo(n->meshes[j]).name);
    if (!meshList.isEmpty()) {
        node["meshes"] = meshList;
    } else {
        QHash<QString, Importer::CameraInfo> cam = m_importer->cameraInfo();
        if (cam.contains(n->name))
            node["camera"] = cam[n->name].name;
    }

    nodes[n->uniqueName] = node;
    return n->uniqueName;
}

static inline QJsonArray col2jsvec(const QVector<float> &color, bool alpha = false)
{
    QJsonArray arr;
    arr << color[0] << color[1] << color[2];
    if (alpha)
        arr << color[3];
    return arr;
}

static inline QJsonArray vec2jsvec(const QVector<float> &v)
{
    QJsonArray arr;
    for (int i = 0; i < v.count(); ++i)
        arr << v[i];
    return arr;
}

static inline void promoteColorsToRGBA(QJsonObject *obj)
{
    QJsonObject::iterator it = obj->begin(), itEnd = obj->end();
    while (it != itEnd) {
        QJsonArray arr = it.value().toArray();
        if (arr.count() == 3) {
            const QString key = it.key();
            if (key == QStringLiteral("ambient")
                    || key == QStringLiteral("diffuse")
                    || key == QStringLiteral("specular")) {
                arr.append(1);
                *it = arr;
            }
        }
        ++it;
    }
}

void GltfExporter::exportMaterials(QJsonObject &materials, QHash<QString, QString> *textureNameMap)
{
    for (uint i = 0; i < m_importer->materialCount(); ++i) {
        Importer::MaterialInfo matInfo = m_importer->materialInfo(i);
        QJsonObject material;
        material["name"] = matInfo.originalName;

        bool opaque = true;
        QJsonObject vals;
        for (QHash<QByteArray, QString>::const_iterator it = matInfo.m_textures.constBegin(); it != matInfo.m_textures.constEnd(); ++it) {
            if (!textureNameMap->contains(it.value()))
                textureNameMap->insert(it.value(), newTextureName());
            QByteArray key = it.key();
            if (key == QByteArrayLiteral("normal")) // avoid clashing with the vertex normals
                key = QByteArrayLiteral("normalmap");
            // alpha is supported for diffuse textures, but have to check the image data to decide if blending is needed
            if (key == QByteArrayLiteral("diffuse")) {
                QString imgFn = opts.outDir + it.value();
                if (m_imageHasAlpha.contains(imgFn)) {
                    if (m_imageHasAlpha[imgFn])
                        opaque = false;
                } else {
#ifdef HAS_QIMAGE
                    QImage img(imgFn);
                    if (!img.isNull()) {
                        if (img.hasAlphaChannel()) {
                            for (int y = 0; opaque && y < img.height(); ++y)
                                for (int x = 0; opaque && x < img.width(); ++x)
                                    if (qAlpha(img.pixel(x, y)) < 255)
                                        opaque = false;
                        }
                        m_imageHasAlpha[imgFn] = !opaque;
                    } else {
                        qWarning() << "WARNING: Cannot determine presence of alpha for" << imgFn;
                    }
#else
                    qWarning() << "WARNING: No image support, assuming all textures are opaque";
#endif
                }
            }
            vals[key] = textureNameMap->value(it.value());
        }
        for (QHash<QByteArray, float>::const_iterator it = matInfo.m_values.constBegin();
             it != matInfo.m_values.constEnd(); ++it) {
            if (vals.contains(it.key()))
                continue;
            vals[it.key()] = it.value();
        }
        for (QHash<QByteArray, QVector<float> >::const_iterator it = matInfo.m_colors.constBegin();
             it != matInfo.m_colors.constEnd(); ++it) {
            if (vals.contains(it.key()))
                continue;
            // alpha is supported for the diffuse color. < 1 will enable blending.
            const bool alpha = it.key() == QStringLiteral("diffuse");
            if (alpha && it.value()[3] < 1.0f)
                opaque = false;
            vals[it.key()] = col2jsvec(it.value(), alpha);
        }
        if (opts.shaders)
            material["values"] = vals;

        ProgramInfo *prog = chooseProgram(i);
        TechniqueInfo techniqueInfo;
        bool needsNewTechnique = true;
        for (int j = 0; j < m_techniques.count(); ++j) {
            if (m_techniques[j].prog == prog) {
                techniqueInfo = m_techniques[j];
                needsNewTechnique = opaque != techniqueInfo.opaque;
            }
            if (!needsNewTechnique)
                break;
        }
        if (needsNewTechnique) {
            QString techniqueName = newTechniqueName();
            techniqueInfo = TechniqueInfo(techniqueName, opaque, prog);
            m_techniques.append(techniqueInfo);
            m_usedPrograms.insert(prog);
        }

        if (opts.shaders) {
            if (opts.showLog)
                qDebug().noquote() << "Material #" << i << "->" << techniqueInfo.name;

            material["technique"] = techniqueInfo.name;
            if (opts.genCore) {
                material["techniqueCore"] = techniqueInfo.coreName;
                material["techniqueGL2"] = techniqueInfo.gl2Name;
            }
        }

        if (opts.commonMat) {
            // The built-in shaders we output are of little use in practice.
            // Ideally we want Qt3D's own standard materials in order to have our
            // models participate in lighting for example. To achieve this, output
            // a KHR_materials_common block which Qt3D's loader will recognize and
            // prefer over the shader-based techniques.
            if (!prog->commonTechniqueName.isEmpty()) {
                QJsonObject commonMat;
                commonMat["technique"] = prog->commonTechniqueName;
                // Set the values as-is. "normalmap" is our own extension, not in the spec.
                // However, RGB colors have to be promoted to RGBA since the spec uses
                // vec4, and all types are pre-defined for common material values.
                promoteColorsToRGBA(&vals);
                commonMat["values"] = vals;
                if (!opaque)
                    commonMat["transparent"] = true;
                QJsonObject extensions;
                extensions["KHR_materials_common"] = commonMat;
                material["extensions"] = extensions;
            }
        }

        materials[matInfo.name] = material;
    }
}

void GltfExporter::writeShader(const QString &src, const QString &dst, const QVector<QPair<QByteArray, QByteArray> > &substTab)
{
    for (const Shader shader : shaders) {
        QByteArray name = src.toUtf8();
        if (!qstrcmp(shader.name, name.constData())) {
            QString outfn = opts.outDir + dst;
            QFile outf(outfn);
            if (outf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                m_files.insert(QFileInfo(outf.fileName()).fileName());
                if (opts.showLog)
                    qDebug() << "Writing" << outfn;
                const auto lines = QString::fromUtf8(shader.text).split('\n');
                for (QString line : lines) {
                    for (const auto &subst : substTab)
                        line.replace(subst.first, subst.second);
                    line += QStringLiteral("\n");
                    outf.write(line.toUtf8());
                }
            }
            return;
        }
    }
    qWarning() << "ERROR: No shader found for" << src;
}

void GltfExporter::exportParameter(QJsonObject &dst, const QVector<ProgramInfo::Param> &params)
{
    for (const ProgramInfo::Param &param : params) {
        QJsonObject parameter;
        parameter["type"] = int(param.type);
        if (!param.semantic.isEmpty())
            parameter["semantic"] = param.semantic;
        if (param.name == QStringLiteral("lightIntensity"))
            parameter["value"] = QJsonArray() << 1 << 1 << 1;
        if (param.name == QStringLiteral("lightPosition"))
            parameter["value"] = QJsonArray() << 0 << 0 << 0 << 1;
        dst[param.name] = parameter;
    }
}

namespace {
struct ProgramNames
{
    QString name;
    QString coreName;
};
}

void GltfExporter::exportTechniques(QJsonObject &obj, const QString &basename)
{
    if (!opts.shaders)
        return;

    QJsonObject shaders;
    QHash<QString, QString> shaderMap;
    for (ProgramInfo *prog : qAsConst(m_usedPrograms)) {
        QString newName;
        if (!shaderMap.contains(prog->vertShader)) {
            QJsonObject vertexShader;
            vertexShader["type"] = 35633;
            if (newName.isEmpty())
                newName = newShaderName();
            QString key = basename + QStringLiteral("_") + newName + QStringLiteral("_v");
            QString fn = QString(QStringLiteral("%1.%2")).arg(key).arg("vert");
            vertexShader["uri"] = fn;
            writeShader(prog->vertShader, fn, m_subst_es2);
            if (opts.genCore) {
                QJsonObject coreVertexShader;
                QString coreKey = QString(QStringLiteral("%1_core").arg(key));
                fn = QString(QStringLiteral("%1.%2")).arg(coreKey).arg("vert");
                coreVertexShader["type"] = 35633;
                coreVertexShader["uri"] = fn;
                writeShader(prog->vertShader, fn, m_subst_core);
                shaders[coreKey] = coreVertexShader;
                shaderMap.insert(QString(prog->vertShader + QStringLiteral("_core")), coreKey);
            }
            shaders[key] = vertexShader;
            shaderMap.insert(prog->vertShader, key);
        }
        if (!shaderMap.contains(prog->fragShader)) {
            QJsonObject fragmentShader;
            fragmentShader["type"] = 35632;
            if (newName.isEmpty())
                newName = newShaderName();
            QString key = basename + QStringLiteral("_") + newName + QStringLiteral("_f");
            QString fn = QString(QStringLiteral("%1.%2")).arg(key).arg("frag");
            fragmentShader["uri"] = fn;
            writeShader(prog->fragShader, fn, m_subst_es2);
            if (opts.genCore) {
                QJsonObject coreFragmentShader;
                QString coreKey = QString(QStringLiteral("%1_core").arg(key));
                fn = QString(QStringLiteral("%1.%2")).arg(coreKey).arg("frag");
                coreFragmentShader["type"] = 35632;
                coreFragmentShader["uri"] = fn;
                writeShader(prog->fragShader, fn, m_subst_core);
                shaders[coreKey] = coreFragmentShader;
                shaderMap.insert(QString(prog->fragShader + QStringLiteral("_core")), coreKey);
            }
            shaders[key] = fragmentShader;
            shaderMap.insert(prog->fragShader, key);
        }
    }
    obj["shaders"] = shaders;

    QJsonObject programs;
    QHash<const ProgramInfo *, ProgramNames> programMap;
    for (const ProgramInfo *prog : qAsConst(m_usedPrograms)) {
        QJsonObject program;
        program["vertexShader"] = shaderMap[prog->vertShader];
        program["fragmentShader"] = shaderMap[prog->fragShader];
        QJsonArray attrs;
        for (const ProgramInfo::Param &param : prog->attributes) {
            attrs << param.nameInShader;
        }
        program["attributes"] = attrs;
        QString programName = newProgramName();
        programMap[prog].name = programName;
        programs[programMap[prog].name] = program;
        if (opts.genCore) {
            program["vertexShader"] = shaderMap[QString(prog->vertShader + QLatin1String("_core"))];
            program["fragmentShader"] = shaderMap[QString(prog->fragShader + QLatin1String("_core"))];
            QJsonArray attrs;
            for (const ProgramInfo::Param &param : prog->attributes) {
                attrs << param.nameInShader;
            }
            program["attributes"] = attrs;
            programMap[prog].coreName = programName + QLatin1String("_core");
            programs[programMap[prog].coreName] = program;
        }
    }
    obj["programs"] = programs;

    QJsonObject techniques;
    for (const TechniqueInfo &techniqueInfo : qAsConst(m_techniques)) {
        QJsonObject technique;
        QJsonObject parameters;
        const ProgramInfo *prog = techniqueInfo.prog;
        exportParameter(parameters, prog->attributes);
        exportParameter(parameters, prog->uniforms);
        technique["parameters"] = parameters;
        technique["program"] = programMap[prog].name;
        QJsonObject progAttrs;
        for (const ProgramInfo::Param &param : prog->attributes) {
            progAttrs[param.nameInShader] = param.name;
        }
        technique["attributes"] = progAttrs;
        QJsonObject progUniforms;
        for (const ProgramInfo::Param &param : prog->uniforms) {
            progUniforms[param.nameInShader] = param.name;
        }
        technique["uniforms"] = progUniforms;
        QJsonObject states;
        QJsonArray enabledStates;
        enabledStates << GLT_DEPTH_TEST << GLT_CULL_FACE;
        if (!techniqueInfo.opaque) {
            enabledStates << GLT_BLEND;
            QJsonObject funcs;
            // GL_ONE, GL_ONE_MINUS_SRC_ALPHA
            funcs["blendFuncSeparate"] = QJsonArray() << 1 << 771 << 1 << 771;
            states["functions"] = funcs;
        }
        states["enable"] = enabledStates;
        technique["states"] = states;
        techniques[techniqueInfo.name] = technique;

        if (opts.genCore) {
            //GL2 (same as ES2)
            techniques[techniqueInfo.gl2Name] = technique;

            //Core
            technique["program"] = programMap[prog].coreName;
            techniques[techniqueInfo.coreName] = technique;
        }
    }
    obj["techniques"] = techniques;
}

void GltfExporter::exportAnimations(QJsonObject &obj,
                                    QVector<Importer::BufferInfo> &bufList,
                                    QVector<Importer::MeshInfo::BufferView> &bvList,
                                    QVector<Importer::MeshInfo::Accessor> &accList)
{
    const auto animationInfos = m_importer->animations();
    if (animationInfos.empty()) {
        obj["animations"] = QJsonObject();
        return;
    }

    QString bvName = newBufferViewName();
    QByteArray extraData;

    int sz = 0;
    for (const Importer::AnimationInfo &ai : animationInfos)
        sz += ai.keyFrames.count() * (1 + 3 + 4 + 3) * sizeof(float);
    extraData.resize(sz);

    float *base = reinterpret_cast<float *>(extraData.data());
    float *p = base;

    QJsonObject animations;
    for (const Importer::AnimationInfo &ai : animationInfos) {
        QJsonObject animation;
        animation["name"] = ai.name;
        animation["count"] = ai.keyFrames.count();
        QJsonObject samplers;
        QJsonArray channels;

        if (ai.hasTranslation) {
            QJsonObject sampler;
            sampler["input"] = QStringLiteral("TIME");
            sampler["interpolation"] = QStringLiteral("LINEAR");
            sampler["output"] = QStringLiteral("translation");
            samplers["sampler_translation"] = sampler;
            QJsonObject channel;
            channel["sampler"] = QStringLiteral("sampler_translation");
            QJsonObject target;
            target["id"] = ai.targetNode;
            target["path"] = QStringLiteral("translation");
            channel["target"] = target;
            channels << channel;
        }
        if (ai.hasRotation) {
            QJsonObject sampler;
            sampler["input"] = QStringLiteral("TIME");
            sampler["interpolation"] = QStringLiteral("LINEAR");
            sampler["output"] = QStringLiteral("rotation");
            samplers["sampler_rotation"] = sampler;
            QJsonObject channel;
            channel["sampler"] = QStringLiteral("sampler_rotation");
            QJsonObject target;
            target["id"] = ai.targetNode;
            target["path"] = QStringLiteral("rotation");
            channel["target"] = target;
            channels << channel;
        }
        if (ai.hasScale) {
            QJsonObject sampler;
            sampler["input"] = QStringLiteral("TIME");
            sampler["interpolation"] = QStringLiteral("LINEAR");
            sampler["output"] = QStringLiteral("scale");
            samplers["sampler_scale"] = sampler;
            QJsonObject channel;
            channel["sampler"] = QStringLiteral("sampler_scale");
            QJsonObject target;
            target["id"] = ai.targetNode;
            target["path"] = QStringLiteral("scale");
            channel["target"] = target;
            channels << channel;
        }

        animation["samplers"] = samplers;
        animation["channels"] = channels;
        QJsonObject parameters;

        // Multiple animations sharing the same data should ideally use the
        // same accessors. This we unfortunately cannot do due to assimp's/our
        // own data structures so everything will get its own accessor and data
        // for now.

        Importer::MeshInfo::Accessor acc;
        acc.name = newAccessorName();
        acc.bufferView = bvName;
        acc.count = ai.keyFrames.count();
        acc.componentType = GLT_FLOAT;
        acc.type = QStringLiteral("SCALAR");
        acc.offset = uint((p - base) * sizeof(float));
        for (const Importer::KeyFrame &kf : ai.keyFrames)
            *p++ = kf.t;
        parameters["TIME"] = acc.name;
        accList << acc;

        if (ai.hasTranslation) {
            acc.name = newAccessorName();
            acc.componentType = GLT_FLOAT;
            acc.type = QStringLiteral("VEC3");
            acc.offset = uint((p - base) * sizeof(float));
            QVector<float> lastV;
            for (const Importer::KeyFrame &kf : ai.keyFrames) {
                const QVector<float> *v = kf.transValid ? &kf.trans : &lastV;
                *p++ = v->at(0);
                *p++ = v->at(1);
                *p++ = v->at(2);
                if (kf.transValid)
                    lastV = *v;
            }
            parameters["translation"] = acc.name;
            accList << acc;
        }
        if (ai.hasRotation) {
            acc.name = newAccessorName();
            acc.componentType = GLT_FLOAT;
            acc.type = QStringLiteral("VEC4");
            acc.offset = uint((p - base) * sizeof(float));
            QVector<float> lastV;
            for (const Importer::KeyFrame &kf : ai.keyFrames) {
                const QVector<float> *v = kf.rotValid ? &kf.rot : &lastV;
                *p++ = v->at(1); // x
                *p++ = v->at(2); // y
                *p++ = v->at(3); // z
                *p++ = v->at(0); // w
                if (kf.rotValid)
                    lastV = *v;
            }
            parameters["rotation"] = acc.name;
            accList << acc;
        }
        if (ai.hasScale) {
            acc.name = newAccessorName();
            acc.componentType = GLT_FLOAT;
            acc.type = QStringLiteral("VEC3");
            acc.offset = uint((p - base) * sizeof(float));
            QVector<float> lastV;
            for (const Importer::KeyFrame &kf : ai.keyFrames) {
                const QVector<float> *v = kf.scaleValid ? &kf.scale : &lastV;
                *p++ = v->at(0);
                *p++ = v->at(1);
                *p++ = v->at(2);
                if (kf.scaleValid)
                    lastV = *v;
            }
            parameters["scale"] = acc.name;
            accList << acc;
        }
        animation["parameters"] = parameters;

        animations[newAnimationName()] = animation;
    }
    obj["animations"] = animations;

    // Now all the key frame data is in extraData. Append it to the first buffer
    // and create a single buffer view for it.
    if (!extraData.isEmpty()) {
        if (bufList.isEmpty()) {
            Importer::BufferInfo b;
            b.name = QStringLiteral("buf");
            bufList << b;
        }
        Importer::BufferInfo &buf(bufList[0]);
        Importer::MeshInfo::BufferView bv;
        bv.name = bvName;
        bv.offset = buf.data.size();
        bv.length = uint((p - base) * sizeof(float));
        bv.componentType = GLT_FLOAT;
        bvList << bv;
        extraData.resize(bv.length);
        buf.data += extraData;
        if (opts.showLog)
            qDebug().noquote() << "Animation data in buffer uses" << extraData.size() << "bytes";
    }
}

void GltfExporter::save(const QString &inputFilename)
{
    if (opts.showLog)
        qDebug() << "Exporting";

    m_files.clear();
    m_techniques.clear();
    m_usedPrograms.clear();

    QFile f;
    QString basename = QFileInfo(inputFilename).baseName();
    QString bufNameTempl = basename + QStringLiteral("_%1.bin");

    copyExternalTextures(inputFilename);
    exportEmbeddedTextures();
    compressTextures();

    m_obj = QJsonObject();

    QVector<Importer::BufferInfo> bufList = m_importer->buffers();
    QVector<Importer::MeshInfo::BufferView> bvList = m_importer->bufferViews();
    QVector<Importer::MeshInfo::Accessor> accList = m_importer->accessors();

    // Animations add data to the buffer so process them first.
    exportAnimations(m_obj, bufList, bvList, accList);

    QJsonObject asset;
    asset["generator"] = QString(QStringLiteral("qgltf %1")).arg(QCoreApplication::applicationVersion());
    asset["version"] = QStringLiteral("1.0");
    asset["premultipliedAlpha"] = true;
    m_obj["asset"] = asset;

    for (int i = 0; i < bufList.count(); ++i) {
        QString bufName = bufNameTempl.arg(i + 1);
        f.setFileName(opts.outDir + bufName);
        if (opts.showLog)
            qDebug().noquote() << (opts.compress ? "Writing (compressed)" : "Writing") << (opts.outDir + bufName);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_files.insert(QFileInfo(f.fileName()).fileName());
            QByteArray data = bufList[i].data;
            if (opts.compress)
                data = qCompress(data);
            f.write(data);
            f.close();
        }
    }

    QJsonObject buffers;
    for (int i = 0; i < bufList.count(); ++i) {
        QJsonObject buffer;
        buffer["byteLength"] = bufList[i].data.size();
        buffer["type"] = QStringLiteral("arraybuffer");
        buffer["uri"] = bufNameTempl.arg(i + 1);
        if (opts.compress)
            buffer["compression"] = QStringLiteral("Qt");
        buffers[bufList[i].name] = buffer;
    }
    m_obj["buffers"] = buffers;

    QJsonObject bufferViews;
    for (const Importer::MeshInfo::BufferView &bv : qAsConst(bvList)) {
        QJsonObject bufferView;
        bufferView["buffer"] = bufList[bv.bufIndex].name;
        bufferView["byteLength"] = int(bv.length);
        bufferView["byteOffset"] = int(bv.offset);
        if (bv.target)
            bufferView["target"] = int(bv.target);
        bufferViews[bv.name] = bufferView;
    }
    m_obj["bufferViews"] = bufferViews;

    QJsonObject accessors;
    for (const Importer::MeshInfo::Accessor &acc : qAsConst(accList)) {
        QJsonObject accessor;
        accessor["bufferView"] = acc.bufferView;
        accessor["byteOffset"] = int(acc.offset);
        accessor["byteStride"] = int(acc.stride);
        accessor["count"] = int(acc.count);
        accessor["componentType"] = int(acc.componentType);
        accessor["type"] = acc.type;
        if (!acc.minVal.isEmpty() && !acc.maxVal.isEmpty()) {
            accessor["min"] = vec2jsvec(acc.minVal);
            accessor["max"] = vec2jsvec(acc.maxVal);
        }
        accessors[acc.name] = accessor;
    }
    m_obj["accessors"] = accessors;

    QJsonObject meshes;
    for (uint i = 0; i < m_importer->meshCount(); ++i) {
        const Importer::MeshInfo meshInfo = m_importer->meshInfo(i);
        QJsonObject mesh;
        mesh["name"] = meshInfo.originalName;
        QJsonArray prims;
        QJsonObject prim;
        prim["mode"] = 4; // triangles
        QJsonObject attrs;
        for (const Importer::MeshInfo::Accessor &acc : meshInfo.accessors) {
            if (acc.usage != QStringLiteral("INDEX"))
                attrs[acc.usage] = acc.name;
            else
                prim["indices"] = acc.name;
        }
        prim["attributes"] = attrs;
        prim["material"] = m_importer->materialInfo(meshInfo.materialIndex).name;
        prims.append(prim);
        mesh["primitives"] = prims;
        meshes[meshInfo.name] = mesh;
    }
    m_obj["meshes"] = meshes;

    QJsonObject cameras;
    const auto cameraInfos = m_importer->cameraInfo();
    for (const Importer::CameraInfo &camInfo : cameraInfos) {
        QJsonObject camera;
        QJsonObject persp;
        persp["aspect_ratio"] = camInfo.aspectRatio;
        persp["yfov"] = camInfo.yfov;
        persp["znear"] = camInfo.znear;
        persp["zfar"] = camInfo.zfar;
        camera["perspective"] = persp;
        camera["type"] = QStringLiteral("perspective");
        cameras[camInfo.name] = camera;
    }
    m_obj["cameras"] = cameras;

    QJsonArray sceneNodes;
    QJsonObject nodes;
    for (const Importer::Node *n : qAsConst(m_importer->rootNode()->children)) {
        if (nodeIsUseful(n))
            sceneNodes << exportNode(n, nodes);
    }
    m_obj["nodes"] = nodes;

    QJsonObject scenes;
    QJsonObject defaultScene;
    defaultScene["nodes"] = sceneNodes;
    scenes["defaultScene"] = defaultScene;
    m_obj["scenes"] = scenes;
    m_obj["scene"] = QStringLiteral("defaultScene");

    QJsonObject materials;
    QHash<QString, QString> textureNameMap;
    exportMaterials(materials, &textureNameMap);
    m_obj["materials"] = materials;

    QJsonObject textures;
    QHash<QString, QString> imageMap; // uri -> key
    for (QHash<QString, QString>::const_iterator it = textureNameMap.constBegin(); it != textureNameMap.constEnd(); ++it) {
        QJsonObject texture;
        if (!imageMap.contains(it.key()))
            imageMap[it.key()] = newImageName();
        texture["source"] = imageMap[it.key()];
        texture["format"] = 0x1908; // RGBA
        const bool compressed = m_compressedTextures.contains(it.key());
        texture["internalFormat"] = !compressed ? 0x1908 : 0x8D64; // RGBA / ETC1
        texture["sampler"] = !compressed ? QStringLiteral("sampler_mip_rep") : QStringLiteral("sampler_nonmip_rep");
        texture["target"] = 3553; // TEXTURE_2D
        texture["type"] = 5121; // UNSIGNED_BYTE
        textures[it.value()] = texture;
    }
    m_obj["textures"] = textures;

    QJsonObject images;
    for (QHash<QString, QString>::const_iterator it = imageMap.constBegin(); it != imageMap.constEnd(); ++it) {
        QJsonObject image;
        image["uri"] = m_compressedTextures.contains(it.key()) ? m_compressedTextures[it.key()] : it.key();
        images[it.value()] = image;
    }
    m_obj["images"] = images;

    QJsonObject samplers;
    QJsonObject sampler;
    sampler["magFilter"] = 9729; // LINEAR
    sampler["minFilter"] = 9987; // LINEAR_MIPMAP_LINEAR
    sampler["wrapS"] = 10497; // REPEAT
    sampler["wrapT"] = 10497;
    samplers["sampler_mip_rep"] = sampler;
    // Compressed textures may not support mipmapping with GLES.
    if (!m_compressedTextures.isEmpty()) {
        sampler["minFilter"] = 9729; // LINEAR
        samplers["sampler_nonmip_rep"] = sampler;
    }
    m_obj["samplers"] = samplers;

    exportTechniques(m_obj, basename);

    m_doc.setObject(m_obj);

    QString gltfName = opts.outDir + basename + QStringLiteral(".qgltf");
    f.setFileName(gltfName);
    if (opts.showLog)
        qDebug().noquote() << (opts.genBin ? "Writing (binary JSON)" : "Writing") << gltfName;

    if (opts.genBin) {
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_files.insert(QFileInfo(f.fileName()).fileName());
            QByteArray json = m_doc.toBinaryData();
            f.write(json);
            f.close();
        }
    } else {
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_files.insert(QFileInfo(f.fileName()).fileName());
            QByteArray json = m_doc.toJson(opts.compact ? QJsonDocument::Compact : QJsonDocument::Indented);
            f.write(json);
            f.close();
        }
    }

    QString qrcName = opts.outDir + basename + QStringLiteral(".qrc");
    f.setFileName(qrcName);
    if (opts.showLog)
        qDebug().noquote() << "Writing" << qrcName;
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QByteArray pre = "<RCC><qresource prefix=\"/models\">\n";
        QByteArray post = "</qresource></RCC>\n";
        f.write(pre);
        for (const QString &file : qAsConst(m_files)) {
            QString line = QString(QStringLiteral("  <file>%1</file>\n")).arg(file);
            f.write(line.toUtf8());
        }
        f.write(post);
        f.close();
    }

    if (opts.showLog)
        qDebug() << "Done\n";
}

static const char *description =
        "qgltf uses Assimp to import a variety of 3D model formats "
        "and export it into fast-to-load, optimized glTF "
        "assets embedded into Qt resource files.\n\n"
        "Note: this tool should typically not be invoked directly. Instead, "
        "let qmake manage it based on QT3D_MODELS in the .pro file.\n\n"
        "For standard Qt 3D usage the recommended options are -b -S.";

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("0.2"));
    app.setApplicationName(QStringLiteral("Qt glTF converter"));

    QCommandLineParser cmdLine;
    cmdLine.addHelpOption();
    cmdLine.addVersionOption();
    cmdLine.setApplicationDescription(QString::fromUtf8(description));
    QCommandLineOption outDirOpt(QStringLiteral("d"), QStringLiteral("Place all output data into <dir>"), QStringLiteral("dir"));
    cmdLine.addOption(outDirOpt);
    QCommandLineOption binOpt(QStringLiteral("b"), QStringLiteral("Store binary JSON data in the .qgltf file"));
    cmdLine.addOption(binOpt);
    QCommandLineOption compactOpt(QStringLiteral("m"), QStringLiteral("Store compact JSON in the .qgltf file"));
    cmdLine.addOption(compactOpt);
    QCommandLineOption compOpt(QStringLiteral("c"), QStringLiteral("qCompress() vertex/index data in the .bin file"));
    cmdLine.addOption(compOpt);
    QCommandLineOption tangentOpt(QStringLiteral("t"), QStringLiteral("Generate tangent vectors"));
    cmdLine.addOption(tangentOpt);
    QCommandLineOption nonInterleavedOpt(QStringLiteral("n"), QStringLiteral("Use non-interleaved buffer layout"));
    cmdLine.addOption(nonInterleavedOpt);
    QCommandLineOption scaleOpt(QStringLiteral("e"), QStringLiteral("Scale vertices by the float scale factor <factor>"), QStringLiteral("factor"));
    cmdLine.addOption(scaleOpt);
    QCommandLineOption coreOpt(QStringLiteral("g"), QStringLiteral("Generate OpenGL 3.2+ core profile shaders too"));
    cmdLine.addOption(coreOpt);
    QCommandLineOption etc1Opt(QStringLiteral("1"), QStringLiteral("Generate ETC1 compressed textures by invoking etc1tool (PNG only)"));
    cmdLine.addOption(etc1Opt);
    QCommandLineOption noCommonMatOpt(QStringLiteral("T"), QStringLiteral("Do not generate KHR_materials_common block"));
    cmdLine.addOption(noCommonMatOpt);
    QCommandLineOption noShadersOpt(QStringLiteral("S"), QStringLiteral("Do not generate shaders/programs/techniques"));
    cmdLine.addOption(noShadersOpt);
    QCommandLineOption silentOpt(QStringLiteral("s"), QStringLiteral("Silence debug output"));
    cmdLine.addOption(silentOpt);
    cmdLine.process(app);
    opts.outDir = cmdLine.value(outDirOpt);
    opts.genBin = cmdLine.isSet(binOpt);
    opts.compact = cmdLine.isSet(compactOpt);
    opts.compress = cmdLine.isSet(compOpt);
    opts.genTangents = cmdLine.isSet(tangentOpt);
    opts.interleave = !cmdLine.isSet(nonInterleavedOpt);
    opts.scale = 1;
    if (cmdLine.isSet(scaleOpt)) {
        bool ok = false;
        float v;
        v = cmdLine.value(scaleOpt).toFloat(&ok);
        if (ok)
            opts.scale = v;
    }
    opts.genCore = cmdLine.isSet(coreOpt);
    opts.texComp = cmdLine.isSet(etc1Opt) ? Options::ETC1 : Options::NoTextureCompression;
    opts.commonMat = !cmdLine.isSet(noCommonMatOpt);
    opts.shaders = !cmdLine.isSet(noShadersOpt);
    opts.showLog = !cmdLine.isSet(silentOpt);
    if (!opts.outDir.isEmpty()) {
        if (!opts.outDir.endsWith('/'))
            opts.outDir.append('/');
        QDir().mkpath(opts.outDir);
    }

    const auto fileNames = cmdLine.positionalArguments();
    if (fileNames.isEmpty())
        cmdLine.showHelp();

    AssimpImporter importer;
    GltfExporter exporter(&importer);
    for (const QString &fn : fileNames) {
        if (!importer.load(fn)) {
            qWarning() << "Failed to import" << fn;
            continue;
        }
        exporter.save(fn);
    }

    return 0;
}
