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

#include "gltfexporter.h"

#include <QtCore/qiodevice.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qhash.h>
#include <QtCore/qdebug.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qmath.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qmetaobject.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qvector4d.h>
#include <QtGui/qmatrix4x4.h>

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qblendequation.h>
#include <Qt3DRender/qblendequationarguments.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qabstractlight.h>
#include <Qt3DRender/qpointlight.h>
#include <Qt3DRender/qspotlight.h>
#include <Qt3DRender/qdirectionallight.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qgeometryfactory.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qalphacoverage.h>
#include <Qt3DRender/qalphatest.h>
#include <Qt3DRender/qclipplane.h>
#include <Qt3DRender/qcolormask.h>
#include <Qt3DRender/qcullface.h>
#include <Qt3DRender/qdepthtest.h>
#include <Qt3DRender/qdithering.h>
#include <Qt3DRender/qfrontface.h>
#include <Qt3DRender/qmultisampleantialiasing.h>
#include <Qt3DRender/qnodepthmask.h>
#include <Qt3DRender/qpointsize.h>
#include <Qt3DRender/qpolygonoffset.h>
#include <Qt3DRender/qscissortest.h>
#include <Qt3DRender/qseamlesscubemap.h>
#include <Qt3DRender/qstencilmask.h>
#include <Qt3DRender/qstenciloperation.h>
#include <Qt3DRender/qstenciloperationarguments.h>
#include <Qt3DRender/qstenciltest.h>
#include <Qt3DRender/qstenciltestarguments.h>
#include <Qt3DExtras/qconemesh.h>
#include <Qt3DExtras/qcuboidmesh.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/qplanemesh.h>
#include <Qt3DExtras/qspheremesh.h>
#include <Qt3DExtras/qtorusmesh.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <Qt3DExtras/qphongalphamaterial.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qnormaldiffusemapmaterial.h>
#include <Qt3DExtras/qnormaldiffusemapalphamaterial.h>
#include <Qt3DExtras/qnormaldiffusespecularmapmaterial.h>
#include <Qt3DExtras/qgoochmaterial.h>
#include <Qt3DExtras/qpervertexcolormaterial.h>

#include <private/qurlhelper_p.h>

#ifndef qUtf16PrintableImpl
#  define qUtf16PrintableImpl(string) \
    static_cast<const wchar_t*>(static_cast<const void*>(string.utf16()))
#endif

namespace {

inline QJsonArray col2jsvec(const QColor &color, bool alpha = false)
{
    QJsonArray arr;
    arr << color.redF() << color.greenF() << color.blueF();
    if (alpha)
        arr << color.alphaF();
    return arr;
}

template <typename T>
inline QJsonArray vec2jsvec(const QVector<T> &v)
{
    QJsonArray arr;
    for (int i = 0; i < v.count(); ++i)
        arr << v.at(i);
    return arr;
}

inline QJsonArray size2jsvec(const QSize &size) {
    QJsonArray arr;
    arr << size.width() << size.height();
    return arr;
}

inline QJsonArray vec2jsvec(const QVector2D &v)
{
    QJsonArray arr;
    arr << v.x() << v.y();
    return arr;
}

inline QJsonArray vec2jsvec(const QVector3D &v)
{
    QJsonArray arr;
    arr << v.x() << v.y() << v.z();
    return arr;
}

inline QJsonArray vec2jsvec(const QVector4D &v)
{
    QJsonArray arr;
    arr << v.x() << v.y() << v.z() << v.w();
    return arr;
}

#if 0   // unused for now
inline QJsonArray matrix2jsvec(const QMatrix2x2 &matrix)
{
    QJsonArray jm;
    const float *mtxp = matrix.constData();
    for (int j = 0; j < 4; ++j)
        jm.append(*mtxp++);
    return jm;
}

inline QJsonArray matrix2jsvec(const QMatrix3x3 &matrix)
{
    QJsonArray jm;
    const float *mtxp = matrix.constData();
    for (int j = 0; j < 9; ++j)
        jm.append(*mtxp++);
    return jm;
}
#endif

inline QJsonArray matrix2jsvec(const QMatrix4x4 &matrix)
{
    QJsonArray jm;
    const float *mtxp = matrix.constData();
    for (int j = 0; j < 16; ++j)
        jm.append(*mtxp++);
    return jm;
}

inline void promoteColorsToRGBA(QJsonObject *obj)
{
    auto it = obj->begin();
    auto itEnd = obj->end();
    while (it != itEnd) {
        QJsonArray arr = it.value().toArray();
        if (arr.count() == 3) {
            const QString key = it.key();
            if (key == QStringLiteral("ambient")
                    || key == QStringLiteral("diffuse")
                    || key == QStringLiteral("specular")
                    || key == QStringLiteral("warm")
                    || key == QStringLiteral("cool")) {
                arr.append(1);
                *it = arr;
            }
        }
        ++it;
    }
}

} // namespace

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;
using namespace Qt3DExtras;

namespace Qt3DRender {

Q_LOGGING_CATEGORY(GLTFExporterLog, "Qt3D.GLTFExport", QtWarningMsg)

const QString MATERIAL_DIFFUSE_COLOR = QStringLiteral("kd");
const QString MATERIAL_SPECULAR_COLOR = QStringLiteral("ks");
const QString MATERIAL_AMBIENT_COLOR = QStringLiteral("ka");

const QString MATERIAL_DIFFUSE_TEXTURE = QStringLiteral("diffuseTexture");
const QString MATERIAL_SPECULAR_TEXTURE = QStringLiteral("specularTexture");
const QString MATERIAL_NORMALS_TEXTURE = QStringLiteral("normalTexture");

const QString MATERIAL_SHININESS = QStringLiteral("shininess");
const QString MATERIAL_ALPHA = QStringLiteral("alpha");

// Custom extension for Qt3D
const QString MATERIAL_TEXTURE_SCALE = QStringLiteral("texCoordScale");

// Custom gooch material values
const QString MATERIAL_BETA = QStringLiteral("beta");
const QString MATERIAL_COOL_COLOR = QStringLiteral("kblue");
const QString MATERIAL_WARM_COLOR = QStringLiteral("kyellow");

const QString VERTICES_ATTRIBUTE_NAME = QAttribute::defaultPositionAttributeName();
const QString NORMAL_ATTRIBUTE_NAME =  QAttribute::defaultNormalAttributeName();
const QString TANGENT_ATTRIBUTE_NAME = QAttribute::defaultTangentAttributeName();
const QString TEXTCOORD_ATTRIBUTE_NAME = QAttribute::defaultTextureCoordinateAttributeName();
const QString COLOR_ATTRIBUTE_NAME = QAttribute::defaultColorAttributeName();

GLTFExporter::GLTFExporter() : QSceneExporter()
  , m_sceneRoot(nullptr)
  , m_rootNode(nullptr)
  , m_rootNodeEmpty(false)

{
}

GLTFExporter::~GLTFExporter()
{
}

// sceneRoot  : The root entity that contains the exported scene. If the sceneRoot doesn't have
//              any exportable components, it is not exported itself. This is because importing a
//              scene creates an empty top level entity to hold the scene.
// outDir     : The directory where the scene export directory is created in.
// exportName : Name of the directory created in outDir to hold the exported scene. Also used as
//              the file name base for generated files.
// options    : Export options.
//
// Supported options are:
// "binaryJson"  (bool): Generates a binary JSON file, which is more efficient to parse.
// "compactJson" (bool): Removes unnecessary whitespace from the generated JSON file.
//                       Ignored if "binaryJson" option is true.
bool GLTFExporter::exportScene(QEntity *sceneRoot, const QString &outDir,
                               const QString &exportName, const QVariantHash &options)
{
    m_bufferViewCount = 0;
    m_accessorCount = 0;
    m_meshCount = 0;
    m_materialCount = 0;
    m_techniqueCount = 0;
    m_textureCount = 0;
    m_imageCount = 0;
    m_shaderCount = 0;
    m_programCount = 0;
    m_nodeCount = 0;
    m_cameraCount = 0;
    m_lightCount = 0;
    m_renderPassCount = 0;
    m_effectCount = 0;

    m_gltfOpts.binaryJson = options.value(QStringLiteral("binaryJson"),
                                          QVariant(false)).toBool();
    m_gltfOpts.compactJson = options.value(QStringLiteral("compactJson"),
                                           QVariant(false)).toBool();

    QFileInfo outDirFileInfo(outDir);
    QString absoluteOutDir = outDirFileInfo.absoluteFilePath();
    if (!absoluteOutDir.endsWith(QLatin1Char('/')))
        absoluteOutDir.append(QLatin1Char('/'));
    m_exportName = exportName;
    m_sceneRoot = sceneRoot;
    QString finalExportDir = absoluteOutDir + m_exportName;
    if (!finalExportDir.endsWith(QLatin1Char('/')))
        finalExportDir.append(QLatin1Char('/'));

    QDir outDirDir(absoluteOutDir);

    // Make sure outDir exists
    if (outDirFileInfo.exists()) {
        if (!outDirFileInfo.isDir()) {
            qCWarning(GLTFExporterLog, "outDir is not a directory: '%ls'",
                      qUtf16PrintableImpl(absoluteOutDir));
            return false;
        }
    } else {
        if (!outDirDir.mkpath(outDirFileInfo.absoluteFilePath())) {
            qCWarning(GLTFExporterLog, "outDir could not be created: '%ls'",
                      qUtf16PrintableImpl(absoluteOutDir));
            return false;
        }
    }

    // Create temporary directory for exporting
    QTemporaryDir exportDir;

    if (!exportDir.isValid()) {
        qCWarning(GLTFExporterLog, "Temporary export directory could not be created");
        return false;
    }
    m_exportDir = exportDir.path();
    m_exportDir.append(QStringLiteral("/"));

    qCDebug(GLTFExporterLog, "Output directory: %ls", qUtf16PrintableImpl(absoluteOutDir));
    qCDebug(GLTFExporterLog, "Export name: %ls", qUtf16PrintableImpl(m_exportName));
    qCDebug(GLTFExporterLog, "Temp export dir: %ls", qUtf16PrintableImpl(m_exportDir));
    qCDebug(GLTFExporterLog, "Final export dir: %ls", qUtf16PrintableImpl(finalExportDir));

    parseScene();

    // Export scene to temporary directory
    if (!saveScene()) {
        qCWarning(GLTFExporterLog, "Exporting GLTF scene failed");
        return false;
    }

    // Create final export directory
    if (!outDirDir.mkpath(m_exportName)) {
        qCWarning(GLTFExporterLog, "Final export directory could not be created: '%ls'",
                  qUtf16PrintableImpl(finalExportDir));
        return false;
    }

    // As a safety feature, we don't indiscriminately delete existing directory or it's contents,
    // but instead look for an old export and delete only related files.
    clearOldExport(finalExportDir);

    // Files copied from resources will have read-only permissions, which isn't ideal in cases
    // where export is done on top of an existing export.
    // Since different file systems handle permissions differently, we grab the target permissions
    // from the qgltf file, which we created ourselves.
    QFile gltfFile(m_exportDir + m_exportName + QStringLiteral(".qgltf"));
    QFile::Permissions targetPermissions = gltfFile.permissions();

    // Copy exported scene to actual export directory
    for (const auto &sourceFileStr : m_exportedFiles) {
        QFileInfo fiSource(m_exportDir + sourceFileStr);
        QFileInfo fiDestination(finalExportDir + sourceFileStr);
        if (fiDestination.exists()) {
            QFile(fiDestination.absoluteFilePath()).remove();
            qCDebug(GLTFExporterLog, "Removed old file: '%ls'",
                    qUtf16PrintableImpl(fiDestination.absoluteFilePath()));
        }
        QString srcPath = fiSource.absoluteFilePath();
        QString destPath = fiDestination.absoluteFilePath();
        if (!QFile(srcPath).copy(destPath)) {
            qCWarning(GLTFExporterLog, "  Failed to copy file: '%ls' -> '%ls'",
                      qUtf16PrintableImpl(srcPath), qUtf16PrintableImpl(destPath));
            // Don't fail entire export because file copy failed - if there is somehow a read-only
            // file with same name already in the export dir after cleanup we did, let's just assume
            // it's the same file we want rather than risk deleting unrelated protected file.
        } else {
            qCDebug(GLTFExporterLog, "  Copied file: '%ls' -> '%ls'",
                    qUtf16PrintableImpl(srcPath), qUtf16PrintableImpl(destPath));
            QFile(destPath).setPermissions(targetPermissions);
        }
    }

    // Clean up after export

    m_buffer.clear();
    m_meshMap.clear();
    m_materialMap.clear();
    m_cameraMap.clear();
    m_lightMap.clear();
    m_transformMap.clear();
    m_imageMap.clear();
    m_textureIdMap.clear();
    m_meshInfo.clear();
    m_materialInfo.clear();
    m_cameraInfo.clear();
    m_lightInfo.clear();
    m_exportedFiles.clear();
    m_renderPassIdMap.clear();
    m_shaderInfo.clear();
    m_programInfo.clear();
    m_techniqueIdMap.clear();
    m_effectIdMap.clear();
    qDeleteAll(m_defaultObjectCache);
    m_defaultObjectCache.clear();
    m_propertyCache.clear();

    delNode(m_rootNode);

    return true;
}

void GLTFExporter::cacheDefaultProperties(GLTFExporter::PropertyCacheType type)
{
    if (m_defaultObjectCache.contains(type))
        return;

    QObject *defaultObject = nullptr;

    switch (type) {
    case TypeConeMesh:
        defaultObject = new QConeMesh;
        break;
    case TypeCuboidMesh:
        defaultObject = new QCuboidMesh;
        break;
    case TypeCylinderMesh:
        defaultObject = new QCylinderMesh;
        break;
    case TypePlaneMesh:
        defaultObject = new QPlaneMesh;
        break;
    case TypeSphereMesh:
        defaultObject = new QSphereMesh;
        break;
    case TypeTorusMesh:
        defaultObject = new QTorusMesh;
        break;
    default:
        return; // Unsupported type
    }

    // Store the default object for property comparisons
    m_defaultObjectCache.insert(type, defaultObject);

    // Cache metaproperties of supported types (but not their parent class types)
    const QMetaObject *meta = defaultObject->metaObject();
    QVector<QMetaProperty> properties;
    properties.reserve(meta->propertyCount() - meta->propertyOffset());
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        if (meta->property(i).isWritable())
            properties.append(meta->property(i));
    }

    m_propertyCache.insert(type, properties);
}

// Copies textures from original locations to the temporary export directory.
// If texture names conflict, they are renamed.
void GLTFExporter::copyTextures()
{
    qCDebug(GLTFExporterLog, "Copying textures...");
    QHash<QString, QString> copiedMap;
    for (auto texIt = m_textureIdMap.constBegin(); texIt != m_textureIdMap.constEnd(); ++texIt) {
        QFileInfo fi(texIt.key());
        QString absoluteFilePath;
        if (texIt.key().startsWith(QStringLiteral(":")))
            absoluteFilePath = texIt.key();
        else
            absoluteFilePath = fi.absoluteFilePath();
        if (copiedMap.contains(absoluteFilePath)) {
            // Texture has already been copied
            qCDebug(GLTFExporterLog, "  Skipped copying duplicate texture: '%ls'",
                    qUtf16PrintableImpl(absoluteFilePath));
            if (!m_imageMap.contains(texIt.key()))
                m_imageMap.insert(texIt.key(), copiedMap.value(absoluteFilePath));
        } else {
            QString fileName = fi.fileName();
            QString outFile = m_exportDir;
            outFile.append(fileName);
            QFileInfo fiTry(outFile);
            if (fiTry.exists()) {
                static const QString outFileTemplate = QStringLiteral("%2_%3.%4");
                int counter = 0;
                QString tryFile = outFile;
                QString suffix = fiTry.suffix();
                QString base = fiTry.baseName();
                while (fiTry.exists()) {
                    fileName = outFileTemplate.arg(base).arg(counter++).arg(suffix);
                    tryFile = m_exportDir;
                    tryFile.append(fileName);
                    fiTry.setFile(tryFile);
                }
                outFile = tryFile;
            }
            if (!QFile(absoluteFilePath).copy(outFile)) {
                qCWarning(GLTFExporterLog, "  Failed to copy texture: '%ls' -> '%ls'",
                          qUtf16PrintableImpl(absoluteFilePath), qUtf16PrintableImpl(outFile));
            } else {
                qCDebug(GLTFExporterLog, "  Copied texture: '%ls' -> '%ls'",
                        qUtf16PrintableImpl(absoluteFilePath), qUtf16PrintableImpl(outFile));
            }
            // Generate actual target file (as current exportDir is temp dir)
            copiedMap.insert(absoluteFilePath, fileName);
            m_exportedFiles.insert(fileName);
            m_imageMap.insert(texIt.key(), fileName);
        }
    }
}

// Creates shaders to the temporary export directory.
void GLTFExporter::createShaders()
{
    qCDebug(GLTFExporterLog, "Creating shaders...");
    for (const auto &si : m_shaderInfo) {
        const QString fileName = m_exportDir + si.uri;
        QFile f(fileName);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_exportedFiles.insert(QFileInfo(f.fileName()).fileName());
            f.write(si.code);
            f.close();
        } else {
            qCWarning(GLTFExporterLog, "  Writing shaderfile '%ls' failed!",
                      qUtf16PrintableImpl(fileName));
        }
    }
}


void GLTFExporter::parseEntities(const QEntity *entity, Node *parentNode)
{
    if (entity) {
        Node *node =  new Node;
        node->name = entity->objectName();
        node->uniqueName = newNodeName();

        int irrelevantComponents = 0;
        for (auto component : entity->components()) {
            if (auto mesh = qobject_cast<QGeometryRenderer *>(component))
                m_meshMap.insert(node, mesh);
            else if (auto material = qobject_cast<QMaterial *>(component))
                m_materialMap.insert(node, material);
            else if (auto transform = qobject_cast<Qt3DCore::QTransform *>(component))
                m_transformMap.insert(node, transform);
            else if (auto camera = qobject_cast<QCameraLens *>(component))
                m_cameraMap.insert(node, camera);
            else if (auto light = qobject_cast<QAbstractLight *>(component))
                m_lightMap.insert(node, light);
            else
                irrelevantComponents++;
        }
        if (!parentNode) {
            m_rootNode = node;
            if (irrelevantComponents == entity->components().size())
                m_rootNodeEmpty = true;
        } else {
            parentNode->children.append(node);
        }
        qCDebug(GLTFExporterLog, "Parsed entity '%ls' -> '%ls'",
                qUtf16PrintableImpl(entity->objectName()), qUtf16PrintableImpl(node->uniqueName));

        for (auto child : entity->children())
            parseEntities(qobject_cast<QEntity *>(child), node);
    }
}

void GLTFExporter::parseScene()
{
    parseEntities(m_sceneRoot, nullptr);
    parseMaterials();
    parseMeshes();
    parseCameras();
    parseLights();
}

void GLTFExporter::parseMaterials()
{
    qCDebug(GLTFExporterLog, "Parsing materials...");

    int materialCount = 0;
    for (auto it = m_materialMap.constBegin(); it != m_materialMap.constEnd(); ++it) {
        QMaterial *material = it.value();

        MaterialInfo matInfo;
        matInfo.name = newMaterialName();
        matInfo.originalName = material->objectName();

        // Is material common or custom?
        if (qobject_cast<QPhongMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypePhong;
        } else if (auto phongAlpha = qobject_cast<QPhongAlphaMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypePhongAlpha;
            matInfo.blendArguments.resize(4);
            matInfo.blendEquations.resize(2);
            matInfo.blendArguments[0] = int(phongAlpha->sourceRgbArg());
            matInfo.blendArguments[1] = int(phongAlpha->sourceAlphaArg());
            matInfo.blendArguments[2] = int(phongAlpha->destinationRgbArg());
            matInfo.blendArguments[3] = int(phongAlpha->destinationAlphaArg());
            matInfo.blendEquations[0] = int(phongAlpha->blendFunctionArg());
            matInfo.blendEquations[1] = int(phongAlpha->blendFunctionArg());
        } else if (qobject_cast<QDiffuseMapMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypeDiffuseMap;
        } else if (qobject_cast<QDiffuseSpecularMapMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypeDiffuseSpecularMap;
        } else if (qobject_cast<QNormalDiffuseMapAlphaMaterial *>(material)) {
            matInfo.values.insert(QStringLiteral("transparent"), QVariant(true));
            matInfo.type = MaterialInfo::TypeNormalDiffuseMapAlpha;
        } else if (qobject_cast<QNormalDiffuseMapMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypeNormalDiffuseMap;
        } else if (qobject_cast<QNormalDiffuseSpecularMapMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypeNormalDiffuseSpecularMap;
        } else if (qobject_cast<QGoochMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypeGooch;
        } else if (qobject_cast<QPerVertexColorMaterial *>(material)) {
            matInfo.type = MaterialInfo::TypePerVertex;
        } else {
            matInfo.type = MaterialInfo::TypeCustom;
        }

        if (matInfo.type == MaterialInfo::TypeCustom) {
            if (material->effect()) {
                if (!m_effectIdMap.contains(material->effect()))
                    m_effectIdMap.insert(material->effect(), newEffectName());
                parseTechniques(material);
            }
        } else {
            // Default materials do not have separate effect, all effect parameters are stored as
            // material values.
            if (material->effect()) {
                QVector<QParameter *> parameters = material->effect()->parameters();
                for (auto param : parameters) {
                    if (param->value().type() == QVariant::Color) {
                        QColor color = param->value().value<QColor>();
                        if (param->name() == MATERIAL_AMBIENT_COLOR)
                            matInfo.colors.insert(QStringLiteral("ambient"), color);
                        else if (param->name() == MATERIAL_DIFFUSE_COLOR)
                            matInfo.colors.insert(QStringLiteral("diffuse"), color);
                        else if (param->name() == MATERIAL_SPECULAR_COLOR)
                            matInfo.colors.insert(QStringLiteral("specular"), color);
                        else if (param->name() == MATERIAL_COOL_COLOR) // Custom Qt3D gooch
                            matInfo.colors.insert(QStringLiteral("cool"), color);
                        else if (param->name() == MATERIAL_WARM_COLOR) // Custom Qt3D gooch
                            matInfo.colors.insert(QStringLiteral("warm"), color);
                        else
                            matInfo.colors.insert(param->name(), color);
                    } else if (param->value().canConvert<QAbstractTexture *>()) {
                        const QString urlString = textureVariantToUrl(param->value());
                        if (param->name() == MATERIAL_DIFFUSE_TEXTURE)
                            matInfo.textures.insert(QStringLiteral("diffuse"), urlString);
                        else if (param->name() == MATERIAL_SPECULAR_TEXTURE)
                            matInfo.textures.insert(QStringLiteral("specular"), urlString);
                        else if (param->name() == MATERIAL_NORMALS_TEXTURE)
                            matInfo.textures.insert(QStringLiteral("normal"), urlString);
                        else
                            matInfo.textures.insert(param->name(), urlString);
                    } else if (param->name() == MATERIAL_SHININESS) {
                        matInfo.values.insert(QStringLiteral("shininess"), param->value());
                    } else if (param->name() == MATERIAL_BETA) { // Custom Qt3D param for gooch
                        matInfo.values.insert(QStringLiteral("beta"), param->value());
                    } else if (param->name() == MATERIAL_ALPHA) {
                        if (matInfo.type == MaterialInfo::TypeGooch)
                            matInfo.values.insert(QStringLiteral("alpha"), param->value());
                        else
                            matInfo.values.insert(QStringLiteral("transparency"), param->value());
                    } else if (param->name() == MATERIAL_TEXTURE_SCALE) { // Custom Qt3D param
                        matInfo.values.insert(QStringLiteral("textureScale"), param->value());
                    } else {
                        qCDebug(GLTFExporterLog,
                                "Common material had unknown parameter: '%ls'",
                                qUtf16PrintableImpl(param->name()));
                    }
                }
            }
        }

        if (GLTFExporterLog().isDebugEnabled()) {
            qCDebug(GLTFExporterLog, "  Material #%i", materialCount);
            qCDebug(GLTFExporterLog, "    name: '%ls'", qUtf16PrintableImpl(matInfo.name));
            qCDebug(GLTFExporterLog, "    originalName: '%ls'",
                    qUtf16PrintableImpl(matInfo.originalName));
            qCDebug(GLTFExporterLog, "    type: %i", matInfo.type);
            qCDebug(GLTFExporterLog) << "    colors:" << matInfo.colors;
            qCDebug(GLTFExporterLog) << "    values:" << matInfo.values;
            qCDebug(GLTFExporterLog) << "    textures:" << matInfo.textures;
        }

        m_materialInfo.insert(material, matInfo);
        materialCount++;
    }
}

void GLTFExporter::parseMeshes()
{
    qCDebug(GLTFExporterLog, "Parsing meshes...");

    int meshCount = 0;
    for (auto it = m_meshMap.constBegin(); it != m_meshMap.constEnd(); ++it) {
        Node *node = it.key();
        QGeometryRenderer *mesh = it.value();

        MeshInfo meshInfo;
        meshInfo.originalName = mesh->objectName();
        meshInfo.name = newMeshName();
        meshInfo.materialName = m_materialInfo.value(m_materialMap.value(node)).name;

        if (qobject_cast<QConeMesh *>(mesh)) {
            meshInfo.meshType = TypeConeMesh;
            meshInfo.meshTypeStr = QStringLiteral("cone");
        } else if (qobject_cast<QCuboidMesh *>(mesh)) {
            meshInfo.meshType = TypeCuboidMesh;
            meshInfo.meshTypeStr = QStringLiteral("cuboid");
        } else if (qobject_cast<QCylinderMesh *>(mesh)) {
            meshInfo.meshType = TypeCylinderMesh;
            meshInfo.meshTypeStr = QStringLiteral("cylinder");
        } else if (qobject_cast<QPlaneMesh *>(mesh)) {
            meshInfo.meshType = TypePlaneMesh;
            meshInfo.meshTypeStr = QStringLiteral("plane");
        } else if (qobject_cast<QSphereMesh *>(mesh)) {
            meshInfo.meshType = TypeSphereMesh;
            meshInfo.meshTypeStr = QStringLiteral("sphere");
        } else if (qobject_cast<QTorusMesh *>(mesh)) {
            meshInfo.meshType = TypeTorusMesh;
            meshInfo.meshTypeStr = QStringLiteral("torus");
        } else {
            meshInfo.meshType = TypeNone;
        }

        if (meshInfo.meshType != TypeNone) {
            meshInfo.meshComponent = mesh;
            cacheDefaultProperties(meshInfo.meshType);

            if (GLTFExporterLog().isDebugEnabled()) {
                qCDebug(GLTFExporterLog, "  Mesh #%i: (%ls/%ls)", meshCount,
                        qUtf16PrintableImpl(meshInfo.name), qUtf16PrintableImpl(meshInfo.originalName));
                qCDebug(GLTFExporterLog, "    material: '%ls'",
                        qUtf16PrintableImpl(meshInfo.materialName));
                qCDebug(GLTFExporterLog, "    basic mesh type: '%s'",
                        mesh->metaObject()->className());
            }
        } else {
            meshInfo.meshComponent = nullptr;
            QGeometry *meshGeometry = nullptr;
            QGeometryFactoryPtr geometryFunctorPtr = mesh->geometryFactory();
            if (!geometryFunctorPtr.data()) {
                meshGeometry = mesh->geometry();
            } else {
                // Execute the geometry functor to get the geometry, if it is available.
                // Functor gives us the latest data if geometry has changed.
                meshGeometry = geometryFunctorPtr.data()->operator()();
            }

            if (!meshGeometry) {
                qCWarning(GLTFExporterLog, "Ignoring mesh without geometry!");
                continue;
            }

            QAttribute *indexAttrib = nullptr;
            const quint16 *indexPtr = nullptr;

            struct VertexAttrib {
                QAttribute *att;
                const float *ptr;
                QString usage;
                uint offset;
                uint stride;
                int index;
            };

            QVector<VertexAttrib> vAttribs;
            vAttribs.reserve(meshGeometry->attributes().size());

            uint stride(0);

            for (QAttribute *att : meshGeometry->attributes()) {
                if (att->attributeType() == QAttribute::IndexAttribute) {
                    indexAttrib = att;
                    indexPtr = reinterpret_cast<const quint16 *>(att->buffer()->data().constData());
                } else {
                    VertexAttrib vAtt;
                    vAtt.att = att;
                    vAtt.ptr = reinterpret_cast<const float *>(att->buffer()->data().constData());
                    if (att->name() == VERTICES_ATTRIBUTE_NAME)
                        vAtt.usage = QStringLiteral("POSITION");
                    else if (att->name() == NORMAL_ATTRIBUTE_NAME)
                        vAtt.usage = QStringLiteral("NORMAL");
                    else if (att->name() == TEXTCOORD_ATTRIBUTE_NAME)
                        vAtt.usage = QStringLiteral("TEXCOORD_0");
                    else if (att->name() == COLOR_ATTRIBUTE_NAME)
                        vAtt.usage = QStringLiteral("COLOR");
                    else if (att->name() == TANGENT_ATTRIBUTE_NAME)
                        vAtt.usage = QStringLiteral("TANGENT");
                    else
                        vAtt.usage = att->name();

                    vAtt.offset = att->byteOffset() / sizeof(float);
                    vAtt.index = vAtt.offset;
                    vAtt.stride = att->byteStride() > 0
                            ? att->byteStride() / sizeof(float) - att->vertexSize() : 0;
                    stride += att->vertexSize();

                    vAttribs << vAtt;
                }
            }

            int attribCount(vAttribs.size());
            if (!attribCount) {
                qCWarning(GLTFExporterLog, "Ignoring mesh without any attributes!");
                continue;
            }

            QByteArray vertexBuf;
            const int vertexCount = vAttribs.at(0).att->count();
            vertexBuf.resize(stride * vertexCount * sizeof(float));
            float *p = reinterpret_cast<float *>(vertexBuf.data());

            // Create interleaved buffer
            for (int i = 0; i < vertexCount; ++i) {
                for (int j = 0; j < attribCount; ++j) {
                    VertexAttrib &vAtt = vAttribs[j];
                    for (uint k = 0; k < vAtt.att->vertexSize(); ++k)
                        *p++ = vAtt.ptr[vAtt.index++];
                    vAtt.index += vAtt.stride;
                }
            }

            MeshInfo::BufferView vertexBufView;
            vertexBufView.name = newBufferViewName();
            vertexBufView.length = vertexBuf.size();
            vertexBufView.offset = m_buffer.size();
            vertexBufView.componentType = GL_FLOAT;
            vertexBufView.target = GL_ARRAY_BUFFER;
            meshInfo.views.append(vertexBufView);

            QByteArray indexBuf;
            MeshInfo::BufferView indexBufView;
            uint indexCount = 0;
            if (indexAttrib) {
                const uint indexSize = indexAttrib->vertexBaseType() == QAttribute::UnsignedShort
                        ? sizeof(quint16) : sizeof(quint32);
                indexCount = indexAttrib->count();
                uint srcIndex = indexAttrib->byteOffset() / indexSize;
                const uint indexStride = indexAttrib->byteStride()
                        ? indexAttrib->byteStride() / indexSize - 1: 0;
                indexBuf.resize(indexCount * indexSize);
                if (indexSize == sizeof(quint32)) {
                    quint32 *dst = reinterpret_cast<quint32 *>(indexBuf.data());
                    const quint32 *src = reinterpret_cast<const quint32 *>(indexPtr);
                    for (uint j = 0; j < indexCount; ++j) {
                        *dst++ = src[srcIndex++];
                        srcIndex += indexStride;
                    }
                } else {
                    quint16 *dst = reinterpret_cast<quint16 *>(indexBuf.data());
                    for (uint j = 0; j < indexCount; ++j) {
                        *dst++ = indexPtr[srcIndex++];
                        srcIndex += indexStride;
                    }
                }

                indexBufView.name = newBufferViewName();
                indexBufView.length = indexBuf.size();
                indexBufView.offset = vertexBufView.offset + vertexBufView.length;
                indexBufView.componentType = indexSize == sizeof(quint32)
                        ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
                indexBufView.target = GL_ELEMENT_ARRAY_BUFFER;
                meshInfo.views.append(indexBufView);
            }

            MeshInfo::Accessor acc;
            uint startOffset = 0;

            acc.bufferView = vertexBufView.name;
            acc.stride = stride * sizeof(float);
            acc.count = vertexCount;
            acc.componentType = vertexBufView.componentType;
            for (int i = 0; i < attribCount; ++i) {
                const VertexAttrib &vAtt = vAttribs.at(i);
                acc.name = newAccessorName();
                acc.usage = vAtt.usage;
                acc.offset = startOffset * sizeof(float);
                switch (vAtt.att->vertexSize()) {
                case 1:
                    acc.type = QStringLiteral("SCALAR");
                    break;
                case 2:
                    acc.type = QStringLiteral("VEC2");
                    break;
                case 3:
                    acc.type = QStringLiteral("VEC3");
                    break;
                case 4:
                    acc.type = QStringLiteral("VEC4");
                    break;
                case 9:
                    acc.type = QStringLiteral("MAT3");
                    break;
                case 16:
                    acc.type = QStringLiteral("MAT4");
                    break;
                default:
                    qCWarning(GLTFExporterLog, "Invalid vertex size: %d", vAtt.att->vertexSize());
                    break;
                }
                meshInfo.accessors.append(acc);
                startOffset += vAtt.att->vertexSize();
            }

            // Index
            if (indexAttrib) {
                acc.name = newAccessorName();
                acc.usage = QStringLiteral("INDEX");
                acc.bufferView = indexBufView.name;
                acc.offset = 0;
                acc.stride = 0;
                acc.count = indexCount;
                acc.componentType = indexBufView.componentType;
                acc.type = QStringLiteral("SCALAR");
                meshInfo.accessors.append(acc);
            }
            m_buffer.append(vertexBuf);
            m_buffer.append(indexBuf);

            if (GLTFExporterLog().isDebugEnabled()) {
                qCDebug(GLTFExporterLog, "  Mesh #%i: (%ls/%ls)", meshCount,
                        qUtf16PrintableImpl(meshInfo.name), qUtf16PrintableImpl(meshInfo.originalName));
                qCDebug(GLTFExporterLog, "    Vertex count: %i", vertexCount);
                qCDebug(GLTFExporterLog, "    Bytes per vertex: %i", stride);
                qCDebug(GLTFExporterLog, "    Vertex buffer size (bytes): %i", vertexBuf.size());
                qCDebug(GLTFExporterLog, "    Index buffer size (bytes): %i", indexBuf.size());
                QStringList sl;
                for (const auto &bv : meshInfo.views)
                    sl << bv.name;
                qCDebug(GLTFExporterLog) << "    buffer views:" << sl;
                sl.clear();
                for (const auto &acc : meshInfo.accessors)
                    sl << acc.name;
                qCDebug(GLTFExporterLog) << "    accessors:" << sl;
                qCDebug(GLTFExporterLog, "    material: '%ls'",
                        qUtf16PrintableImpl(meshInfo.materialName));
            }
        }

        meshCount++;
        m_meshInfo.insert(mesh, meshInfo);
    }

    qCDebug(GLTFExporterLog, "Total buffer size: %i", m_buffer.size());
}

void GLTFExporter::parseCameras()
{
    qCDebug(GLTFExporterLog, "Parsing cameras...");
    int cameraCount = 0;

    for (auto it = m_cameraMap.constBegin(); it != m_cameraMap.constEnd(); ++it) {
        QCameraLens *camera = it.value();
        CameraInfo c;

        if (camera->projectionType() == QCameraLens::PerspectiveProjection) {
            c.perspective = true;
            c.aspectRatio = camera->aspectRatio();
            c.yfov = qDegreesToRadians(camera->fieldOfView());
        } else {
            c.perspective = false;
            // Note that accurate conversion from four properties of QCameraLens to just two
            // properties of gltf orthographic cameras is not feasible. Only centered cases
            // convert properly.
            c.xmag = qAbs(camera->left() - camera->right());
            c.ymag = qAbs(camera->top() - camera->bottom());
        }

        c.originalName = camera->objectName();
        c.name = newCameraName();
        c.znear = camera->nearPlane();
        c.zfar = camera->farPlane();

        // GLTF cameras point in -Z by default, the rest is in the
        // node matrix, so no separate look-at params given here, unless it's actually QCamera.
        QCamera *cameraEntity = nullptr;
        const QVector<QEntity *> entities = camera->entities();
        if (entities.size() == 1)
            cameraEntity = qobject_cast<QCamera *>(entities.at(0));
        c.cameraEntity = cameraEntity;

        m_cameraInfo.insert(camera, c);
        if (GLTFExporterLog().isDebugEnabled()) {
            qCDebug(GLTFExporterLog, "  Camera: #%i: (%ls/%ls)", cameraCount++,
                    qUtf16PrintableImpl(c.name), qUtf16PrintableImpl(c.originalName));
            qCDebug(GLTFExporterLog, "    Aspect ratio: %f", c.aspectRatio);
            qCDebug(GLTFExporterLog, "    Fov: %f", c.yfov);
            qCDebug(GLTFExporterLog, "    Near: %f", c.znear);
            qCDebug(GLTFExporterLog, "    Far: %f", c.zfar);
        }
    }
}

void GLTFExporter::parseLights()
{
    qCDebug(GLTFExporterLog, "Parsing lights...");
    int lightCount = 0;
    for (auto it = m_lightMap.constBegin(); it != m_lightMap.constEnd(); ++it) {
        QAbstractLight *light = it.value();
        LightInfo lightInfo;
        lightInfo.direction = QVector3D();
        lightInfo.attenuation = QVector3D();
        lightInfo.cutOffAngle = 0.0f;
        lightInfo.type = light->type();
        if (light->type() == QAbstractLight::SpotLight) {
            QSpotLight *spot = qobject_cast<QSpotLight *>(light);
            lightInfo.direction = spot->localDirection();
            lightInfo.attenuation = QVector3D(spot->constantAttenuation(),
                                              spot->linearAttenuation(),
                                              spot->quadraticAttenuation());
            lightInfo.cutOffAngle = spot->cutOffAngle();
        } else if (light->type() == QAbstractLight::PointLight) {
            QPointLight *point = qobject_cast<QPointLight *>(light);
            lightInfo.attenuation = QVector3D(point->constantAttenuation(),
                                              point->linearAttenuation(),
                                              point->quadraticAttenuation());
        } else if (light->type() == QAbstractLight::DirectionalLight) {
            QDirectionalLight *directional = qobject_cast<QDirectionalLight *>(light);
            lightInfo.direction = directional->worldDirection();
        }
        lightInfo.color = light->color();
        lightInfo.intensity = light->intensity();

        lightInfo.originalName = light->objectName();
        lightInfo.name = newLightName();

        m_lightInfo.insert(light, lightInfo);

        if (GLTFExporterLog().isDebugEnabled()) {
            qCDebug(GLTFExporterLog, "  Light #%i: (%ls/%ls)", lightCount++,
                    qUtf16PrintableImpl(lightInfo.name), qUtf16PrintableImpl(lightInfo.originalName));
            qCDebug(GLTFExporterLog, "    Type: %i", lightInfo.type);
            qCDebug(GLTFExporterLog, "    Color: (%i, %i, %i, %i)", lightInfo.color.red(),
                    lightInfo.color.green(), lightInfo.color.blue(), lightInfo.color.alpha());
            qCDebug(GLTFExporterLog, "    Intensity: %f", lightInfo.intensity);
            qCDebug(GLTFExporterLog, "    Direction: (%f, %f, %f)", lightInfo.direction.x(),
                    lightInfo.direction.y(), lightInfo.direction.z());
            qCDebug(GLTFExporterLog, "    Attenuation: (%f, %f, %f)", lightInfo.attenuation.x(),
                    lightInfo.attenuation.y(), lightInfo.attenuation.z());
            qCDebug(GLTFExporterLog, "    CutOffAngle: %f", lightInfo.cutOffAngle);
        }
    }
}

void GLTFExporter::parseTechniques(QMaterial *material)
{
    int techniqueCount = 0;
    qCDebug(GLTFExporterLog, "  Parsing material techniques...");

    for (auto technique : material->effect()->techniques()) {
        QString techName;
        if (m_techniqueIdMap.contains(technique)) {
            techName = m_techniqueIdMap.value(technique);
        } else {
            techName = newTechniqueName();
            parseRenderPasses(technique);

        }
        m_techniqueIdMap.insert(technique, techName);

        techniqueCount++;

        if (GLTFExporterLog().isDebugEnabled()) {
            qCDebug(GLTFExporterLog, "    Technique #%i", techniqueCount);
            qCDebug(GLTFExporterLog, "      name: '%ls'", qUtf16PrintableImpl(techName));
        }
    }
}

void GLTFExporter::parseRenderPasses(QTechnique *technique)
{
    int passCount = 0;
    qCDebug(GLTFExporterLog, "    Parsing render passes for technique...");

    for (auto pass : technique->renderPasses()) {
        QString name;
        if (m_renderPassIdMap.contains(pass)) {
            name = m_renderPassIdMap.value(pass);
        } else {
            name = newRenderPassName();
            m_renderPassIdMap.insert(pass, name);
            if (pass->shaderProgram() && !m_programInfo.contains(pass->shaderProgram())) {
                ProgramInfo pi;
                pi.name = newProgramName();
                pi.vertexShader = addShaderInfo(QShaderProgram::Vertex,
                                                pass->shaderProgram()->vertexShaderCode());
                pi.tessellationControlShader =
                        addShaderInfo(QShaderProgram::Fragment,
                                      pass->shaderProgram()->tessellationControlShaderCode());
                pi.tessellationEvaluationShader =
                        addShaderInfo(QShaderProgram::TessellationControl,
                                      pass->shaderProgram()->tessellationEvaluationShaderCode());
                pi.geometryShader = addShaderInfo(QShaderProgram::TessellationEvaluation,
                                                  pass->shaderProgram()->geometryShaderCode());
                pi.fragmentShader = addShaderInfo(QShaderProgram::Geometry,
                                                  pass->shaderProgram()->fragmentShaderCode());
                pi.computeShader = addShaderInfo(QShaderProgram::Compute,
                                                 pass->shaderProgram()->computeShaderCode());
                m_programInfo.insert(pass->shaderProgram(), pi);
                qCDebug(GLTFExporterLog, "      program: '%ls'", qUtf16PrintableImpl(pi.name));
            }
        }
        passCount++;

        if (GLTFExporterLog().isDebugEnabled()) {
            qCDebug(GLTFExporterLog, "      Render pass #%i", passCount);
            qCDebug(GLTFExporterLog, "        name: '%ls'", qUtf16PrintableImpl(name));
        }
    }
}

QString GLTFExporter::addShaderInfo(QShaderProgram::ShaderType type, QByteArray code)
{
    if (code.isEmpty())
        return QString();

    for (const auto &si : m_shaderInfo) {
        if (si.type == QShaderProgram::Vertex && code == si.code)
            return si.name;
    }

    ShaderInfo newInfo;
    newInfo.type = type;
    newInfo.code = code;
    newInfo.name = newShaderName();
    newInfo.uri = newInfo.name + QStringLiteral(".glsl");

    m_shaderInfo.append(newInfo);

    qCDebug(GLTFExporterLog, "      shader: '%ls'", qUtf16PrintableImpl(newInfo.name));

    return newInfo.name;
}

bool GLTFExporter::saveScene()
{
    qCDebug(GLTFExporterLog, "Saving scene...");

    QVector<MeshInfo::BufferView> bvList;
    QVector<MeshInfo::Accessor> accList;
    for (auto it = m_meshInfo.begin(); it != m_meshInfo.end(); ++it) {
        auto &mi = it.value();
        for (auto &v : mi.views)
            bvList << v;
        for (auto &acc : mi.accessors)
            accList << acc;
    }

    m_obj = QJsonObject();

    QJsonObject asset;
    asset["generator"] = QString(QStringLiteral("GLTFExporter %1")).arg(qVersion());
    asset["version"] = QStringLiteral("1.0");
    asset["premultipliedAlpha"] = true;
    m_obj["asset"] = asset;

    QString bufName = m_exportName + QStringLiteral(".bin");
    QString binFileName = m_exportDir + bufName;
    QFile f(binFileName);
    QFileInfo fiBin(binFileName);

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCDebug(GLTFExporterLog, "  Writing '%ls'", qUtf16PrintableImpl(binFileName));
        m_exportedFiles.insert(fiBin.fileName());
        f.write(m_buffer);
        f.close();
    } else {
        qCWarning(GLTFExporterLog, "  Creating buffers file '%ls' failed!",
                  qUtf16PrintableImpl(binFileName));
        return false;
    }

    QJsonObject buffers;
    QJsonObject buffer;
    buffer["byteLength"] = m_buffer.size();
    buffer["type"] = QStringLiteral("arraybuffer");
    buffer["uri"] = bufName;
    buffers["buf"] = buffer;
    m_obj["buffers"] = buffers;

    QJsonObject bufferViews;
    for (const auto &bv : bvList) {
        QJsonObject bufferView;
        bufferView["buffer"] = QStringLiteral("buf");
        bufferView["byteLength"] = int(bv.length);
        bufferView["byteOffset"] = int(bv.offset);
        if (bv.target)
            bufferView["target"] = int(bv.target);
        bufferViews[bv.name] = bufferView;
    }
    if (bufferViews.size())
        m_obj["bufferViews"] = bufferViews;

    QJsonObject accessors;
    for (const auto &acc : accList) {
        QJsonObject accessor;
        accessor["bufferView"] = acc.bufferView;
        accessor["byteOffset"] = int(acc.offset);
        accessor["byteStride"] = int(acc.stride);
        accessor["count"] = int(acc.count);
        accessor["componentType"] = int(acc.componentType);
        accessor["type"] = acc.type;
        accessors[acc.name] = accessor;
    }
    if (accessors.size())
        m_obj["accessors"] = accessors;

    QJsonObject meshes;
    for (auto it = m_meshInfo.begin(); it != m_meshInfo.end(); ++it) {
        auto &meshInfo = it.value();
        QJsonObject mesh;
        mesh["name"] = meshInfo.originalName;
        if (meshInfo.meshType != TypeNone) {
            QJsonObject properties;
            exportGenericProperties(properties, meshInfo.meshType, meshInfo.meshComponent);
            mesh["type"] = meshInfo.meshTypeStr;
            mesh["properties"] = properties;
            mesh["material"] = meshInfo.materialName;
        } else {
            QJsonArray prims;
            QJsonObject prim;
            prim["mode"] = 4; // triangles
            QJsonObject attrs;
            for (const auto &acc : meshInfo.accessors) {
                if (acc.usage != QStringLiteral("INDEX"))
                    attrs[acc.usage] = acc.name;
                else
                    prim["indices"] = acc.name;
            }
            prim["attributes"] = attrs;
            prim["material"] = meshInfo.materialName;
            prims.append(prim);
            mesh["primitives"] = prims;
        }
        meshes[meshInfo.name] = mesh;
    }
    if (meshes.size())
        m_obj["meshes"] = meshes;

    QJsonObject cameras;
    for (auto it = m_cameraInfo.begin(); it != m_cameraInfo.end(); ++it) {
        const auto &camInfo = it.value();
        QJsonObject camera;
        QJsonObject proj;
        proj["znear"] = camInfo.znear;
        proj["zfar"] = camInfo.zfar;
        if (camInfo.perspective) {
            proj["aspect_ratio"] = camInfo.aspectRatio;
            proj["yfov"] = camInfo.yfov;
            camera["type"] = QStringLiteral("perspective");
            camera["perspective"] = proj;
        } else {
            proj["xmag"] = camInfo.xmag;
            proj["ymag"] = camInfo.ymag;
            camera["type"] = QStringLiteral("orthographic");
            camera["orthographic"] = proj;
        }
        if (camInfo.cameraEntity) {
            camera["position"] = vec2jsvec(camInfo.cameraEntity->position());
            camera["upVector"] = vec2jsvec(camInfo.cameraEntity->upVector());
            camera["viewCenter"] = vec2jsvec(camInfo.cameraEntity->viewCenter());
        }
        camera["name"] = camInfo.originalName;
        cameras[camInfo.name] = camera;
    }
    if (cameras.size())
        m_obj["cameras"] = cameras;

    QJsonArray sceneNodes;
    QJsonObject nodes;
    if (m_rootNodeEmpty) {
        // Don't export the root node if it is there just to group the scene, so we don't get
        // an extra empty node when we import the scene back.
        for (auto c : qAsConst(m_rootNode->children))
            sceneNodes << exportNodes(c, nodes);
    } else {
        sceneNodes << exportNodes(m_rootNode, nodes);
    }
    m_obj["nodes"] = nodes;

    QJsonObject scenes;
    QJsonObject defaultScene;
    defaultScene["nodes"] = sceneNodes;
    scenes["defaultScene"] = defaultScene;
    m_obj["scenes"] = scenes;
    m_obj["scene"] = QStringLiteral("defaultScene");

    QJsonObject materials;

    exportMaterials(materials);
    if (materials.size())
        m_obj["materials"] = materials;

    // Lights must be declared as extensions to the top-level glTF object
    QJsonObject lights;
    for (auto it = m_lightInfo.begin(); it != m_lightInfo.end(); ++it) {
        const auto &lightInfo = it.value();
        QJsonObject light;
        QJsonObject lightDetails;
        QString type;
        if (lightInfo.type == QAbstractLight::SpotLight) {
            type = QStringLiteral("spot");
            lightDetails["falloffAngle"] = lightInfo.cutOffAngle;
        } else if (lightInfo.type == QAbstractLight::PointLight) {
            type = QStringLiteral("point");
        } else if (lightInfo.type == QAbstractLight::DirectionalLight) {
            type = QStringLiteral("directional");
        }
        light["type"] = type;
        if (lightInfo.type == QAbstractLight::SpotLight
                || lightInfo.type == QAbstractLight::DirectionalLight) {
            // The GLTF specs are bit unclear whether there is a direction parameter
            // for spot/directional lights, or are they supposed to just use the
            // parent transforms for direction, but we do need it in any case, so we add it.
            lightDetails["direction"] = vec2jsvec(lightInfo.direction);

        }
        if (lightInfo.type == QAbstractLight::SpotLight
                || lightInfo.type == QAbstractLight::PointLight) {
            lightDetails["constantAttenuation"] = lightInfo.attenuation.x();
            lightDetails["linearAttenuation"] = lightInfo.attenuation.y();
            lightDetails["quadraticAttenuation"] = lightInfo.attenuation.z();
        }
        lightDetails["color"] = col2jsvec(lightInfo.color, false);
        lightDetails["intensity"] = lightInfo.intensity; // Not in spec but needed
        light["name"] = lightInfo.originalName; // Not in spec but we want to pass the name anyway
        light[type] = lightDetails;
        lights[lightInfo.name] = light;
    }
    if (lights.size()) {
        QJsonObject extensions;
        QJsonObject common;
        common["lights"] = lights;
        extensions["KHR_materials_common"] = common;
        m_obj["extensions"] = extensions;
    }

    // Save effects for custom materials
    // Note that we are not saving effects, techniques, render passes, shader programs, or shaders
    // strictly according to GLTF format, but rather in our expanded QGLTF custom format,
    // since the GLTF format doesn't quite match our needs.
    // Having our own format also vastly simplifies export and import of custom materials,
    // since we are not trying to push a round peg into a square hole.
    // If use cases arise in future where our exported GLTF scenes need to be loaded by third party
    // GLTF loaders, we could add an export option to do so, but the exported scene would never
    // be quite the same as the original.
    QJsonObject effects;
    for (auto it = m_effectIdMap.constBegin(); it != m_effectIdMap.constEnd(); ++it) {
        QEffect *effect = it.key();
        const QString effectName = it.value();
        QJsonObject effectObj;
        QJsonObject paramObj;

        for (QParameter *param : effect->parameters())
            exportParameter(paramObj, param->name(), param->value());
        if (!effect->objectName().isEmpty())
            effectObj["name"] = effect->objectName();
        if (!paramObj.isEmpty())
            effectObj["parameters"] = paramObj;
        QJsonArray techs;
        for (auto tech : effect->techniques())
            techs << m_techniqueIdMap.value(tech);
        effectObj["techniques"] = techs;
        effects[effectName] = effectObj;
    }
    if (effects.size())
        m_obj["effects"] = effects;

    // Save techniques for custom materials.
    QJsonObject techniques;
    for (auto it = m_techniqueIdMap.constBegin(); it != m_techniqueIdMap.constEnd(); ++it) {
        QTechnique *technique = it.key();

        QJsonObject techObj;
        QJsonObject filterKeyObj;
        QJsonObject paramObj;
        QJsonArray renderPassArr;

        for (QFilterKey *filterKey : technique->filterKeys())
            setVarToJSonObject(filterKeyObj, filterKey->name(), filterKey->value());

        for (QRenderPass *pass : technique->renderPasses())
            renderPassArr << m_renderPassIdMap.value(pass);

        for (QParameter *param : technique->parameters())
            exportParameter(paramObj, param->name(), param->value());

        const QGraphicsApiFilter *gFilter = technique->graphicsApiFilter();
        if (gFilter) {
            QJsonObject graphicsApiFilterObj;
            graphicsApiFilterObj["api"] = gFilter->api();
            graphicsApiFilterObj["profile"] = gFilter->profile();
            graphicsApiFilterObj["minorVersion"] = gFilter->minorVersion();
            graphicsApiFilterObj["majorVersion"] = gFilter->majorVersion();
            if (!gFilter->vendor().isEmpty())
                graphicsApiFilterObj["vendor"] = gFilter->vendor();
            QJsonArray extensions;
            for (const auto &extName : gFilter->extensions())
                extensions << extName;
            if (!extensions.isEmpty())
                graphicsApiFilterObj["extensions"] = extensions;
            techObj["gapifilter"] = graphicsApiFilterObj;
        }
        if (!technique->objectName().isEmpty())
            techObj["name"] = technique->objectName();
        if (!filterKeyObj.isEmpty())
            techObj["filterkeys"] = filterKeyObj;
        if (!paramObj.isEmpty())
            techObj["parameters"] = paramObj;
        if (!renderPassArr.isEmpty())
            techObj["renderpasses"] = renderPassArr;
        techniques[it.value()] = techObj;
    }
    if (techniques.size())
        m_obj["techniques"] = techniques;

    // Save render passes for custom materials.
    QJsonObject passes;
    for (auto it = m_renderPassIdMap.constBegin(); it != m_renderPassIdMap.constEnd(); ++it) {
        const QRenderPass *pass = it.key();
        const QString passId = it.value();

        QJsonObject passObj;
        QJsonObject filterKeyObj;
        QJsonObject paramObj;
        QJsonObject stateObj;

        for (QFilterKey *filterKey : pass->filterKeys())
            setVarToJSonObject(filterKeyObj, filterKey->name(), filterKey->value());
        for (QParameter *param : pass->parameters())
            exportParameter(paramObj, param->name(), param->value());
        exportRenderStates(stateObj, pass);

        if (!pass->objectName().isEmpty())
            passObj["name"] = pass->objectName();
        if (!filterKeyObj.isEmpty())
            passObj["filterkeys"] = filterKeyObj;
        if (!paramObj.isEmpty())
            passObj["parameters"] = paramObj;
        if (!stateObj.isEmpty())
            passObj["states"] = stateObj;
        passObj["program"] = m_programInfo.value(pass->shaderProgram()).name;
        passes[passId] = passObj;

    }
    if (passes.size())
        m_obj["renderpasses"] = passes;

    // Save programs for custom materials
    QJsonObject programs;
    for (auto it = m_programInfo.constBegin(); it != m_programInfo.constEnd(); ++it) {
        const QShaderProgram *program = it.key();
        const ProgramInfo pi = it.value();

        QJsonObject progObj;
        if (!program->objectName().isEmpty())
            progObj["name"] = program->objectName();
        progObj["vertexShader"] = pi.vertexShader;
        progObj["fragmentShader"] = pi.fragmentShader;
        // Qt3D additions
        if (!pi.tessellationControlShader.isEmpty())
            progObj["tessCtrlShader"] = pi.tessellationControlShader;
        if (!pi.tessellationEvaluationShader.isEmpty())
            progObj["tessEvalShader"] = pi.tessellationEvaluationShader;
        if (!pi.geometryShader.isEmpty())
            progObj["geometryShader"] = pi.geometryShader;
        if (!pi.computeShader.isEmpty())
            progObj["computeShader"] = pi.computeShader;
        programs[pi.name] = progObj;

    }
    if (programs.size())
        m_obj["programs"] = programs;

    // Save shaders for custom materials
    QJsonObject shaders;
    for (const auto &si : m_shaderInfo) {
        QJsonObject shaderObj;
        shaderObj["uri"] = si.uri;
        shaders[si.name] = shaderObj;

    }
    if (shaders.size())
        m_obj["shaders"] = shaders;

    // Copy textures and shaders into temporary directory
    copyTextures();
    createShaders();

    QJsonObject textures;
    QHash<QString, QString> imageKeyMap; // uri -> key
    for (auto it = m_textureIdMap.constBegin(); it != m_textureIdMap.constEnd(); ++it) {
        QJsonObject texture;
        if (!imageKeyMap.contains(it.key()))
            imageKeyMap[it.key()] = newImageName();
        texture["source"] = imageKeyMap[it.key()];
        texture["format"] = GL_RGBA;
        texture["internalFormat"] = GL_RGBA;
        texture["sampler"] = QStringLiteral("sampler_mip_rep");
        texture["target"] = GL_TEXTURE_2D;
        texture["type"] = GL_UNSIGNED_BYTE;
        textures[it.value()] = texture;
    }
    if (textures.size()) {
        m_obj["textures"] = textures;
        QJsonObject samplers;
        QJsonObject sampler;
        sampler["magFilter"] = GL_LINEAR;
        sampler["minFilter"] = GL_LINEAR_MIPMAP_LINEAR;
        sampler["wrapS"] = GL_REPEAT;
        sampler["wrapT"] = GL_REPEAT;
        samplers["sampler_mip_rep"] = sampler;
        m_obj["samplers"] = samplers;
    }

    QJsonObject images;
    for (auto it = imageKeyMap.constBegin(); it != imageKeyMap.constEnd(); ++it) {
        QJsonObject image;
        image["uri"] = m_imageMap.value(it.key());
        images[it.value()] = image;
    }
    if (images.size())
        m_obj["images"] = images;

    m_doc.setObject(m_obj);

    QString gltfName = m_exportDir + m_exportName + QStringLiteral(".qgltf");
    f.setFileName(gltfName);
    qCDebug(GLTFExporterLog, "  Writing %sJSON file: '%ls'",
            m_gltfOpts.binaryJson ? "binary " : "", qUtf16PrintableImpl(gltfName));

    if (m_gltfOpts.binaryJson) {
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_exportedFiles.insert(QFileInfo(f.fileName()).fileName());
            QByteArray json = m_doc.toBinaryData();
            f.write(json);
            f.close();
        } else {
            qCWarning(GLTFExporterLog, "  Writing binary JSON file '%ls' failed!",
                      qUtf16PrintableImpl(gltfName));
            return false;
        }
    } else {
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_exportedFiles.insert(QFileInfo(f.fileName()).fileName());
            QByteArray json = m_doc.toJson(m_gltfOpts.compactJson ? QJsonDocument::Compact
                                                                  : QJsonDocument::Indented);
            f.write(json);
            f.close();
        } else {
            qCWarning(GLTFExporterLog, "  Writing JSON file '%ls' failed!",
                      qUtf16PrintableImpl(gltfName));
            return false;
        }
    }

    QString qrcName = m_exportDir + m_exportName + QStringLiteral(".qrc");
    f.setFileName(qrcName);
    qCDebug(GLTFExporterLog, "Writing '%ls'", qUtf16PrintableImpl(qrcName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QByteArray pre = "<RCC><qresource prefix=\"/gltf_res\">\n";
        QByteArray post = "</qresource></RCC>\n";
        f.write(pre);
        for (const auto &file : qAsConst(m_exportedFiles)) {
            QString line = QString(QStringLiteral("  <file>%1</file>\n")).arg(file);
            f.write(line.toUtf8());
        }
        f.write(post);
        f.close();
        m_exportedFiles.insert(QFileInfo(f.fileName()).fileName());
    } else {
        qCWarning(GLTFExporterLog, "  Creating qrc file '%ls' failed!",
                  qUtf16PrintableImpl(qrcName));
        return false;
    }

    qCDebug(GLTFExporterLog, "Saving done!");

    return true;
}

void GLTFExporter::delNode(GLTFExporter::Node *n)
{
    if (!n)
        return;
    for (auto *c : qAsConst(n->children))
        delNode(c);
    delete n;
}

QString GLTFExporter::exportNodes(GLTFExporter::Node *n, QJsonObject &nodes)
{
    QJsonObject node;
    node["name"] = n->name;
    QJsonArray children;
    for (auto c : qAsConst(n->children))
        children << exportNodes(c, nodes);
    node["children"] = children;
    if (auto transform = m_transformMap.value(n))
        node["matrix"] = matrix2jsvec(transform->matrix());

    if (auto mesh = m_meshMap.value(n)) {
        QJsonArray meshList;
        meshList.append(m_meshInfo.value(mesh).name);
        node["meshes"] = meshList;
    }

    if (auto camera = m_cameraMap.value(n))
        node["camera"] = m_cameraInfo.value(camera).name;

    if (auto light = m_lightMap.value(n)) {
        QJsonObject extensions;
        QJsonObject lights;
        lights["light"] = m_lightInfo.value(light).name;
        extensions["KHR_materials_common"] = lights;
        node["extensions"] = extensions;
    }

    nodes[n->uniqueName] = node;
    return n->uniqueName;
}

void GLTFExporter::exportMaterials(QJsonObject &materials)
{
    QHash<QString, bool> imageHasAlpha;

    QHashIterator<QMaterial *, MaterialInfo> matIt(m_materialInfo);
    for (auto matIt = m_materialInfo.constBegin(); matIt != m_materialInfo.constEnd(); ++matIt) {
        const QMaterial *material = matIt.key();
        const MaterialInfo &matInfo = matIt.value();

        QJsonObject materialObj;
        materialObj["name"] = matInfo.originalName;

        if (matInfo.type == MaterialInfo::TypeCustom) {
            QVector<QParameter *> parameters = material->parameters();
            QJsonObject paramObj;
            for (auto param : parameters)
                exportParameter(paramObj, param->name(), param->value());
            materialObj["effect"] = m_effectIdMap.value(material->effect());
            materialObj["parameters"] = paramObj;
        } else {
            bool opaque = true;
            QJsonObject vals;
            for (auto it = matInfo.textures.constBegin(); it != matInfo.textures.constEnd(); ++it) {
                QString key = it.key();
                if (key == QStringLiteral("normal")) // avoid clashing with the vertex normals
                    key = QStringLiteral("normalmap");
                // Alpha is supported for diffuse textures, but have to check the image data to
                // decide if blending is needed
                if (key == QStringLiteral("diffuse")) {
                    QString imgFn = it.value();
                    if (imageHasAlpha.contains(imgFn)) {
                        if (imageHasAlpha[imgFn])
                            opaque = false;
                    } else {
                        QImage img(imgFn);
                        if (!img.isNull()) {
                            if (img.hasAlphaChannel()) {
                                for (int y = 0; opaque && y < img.height(); ++y) {
                                    for (int x = 0; opaque && x < img.width(); ++x) {
                                        if (qAlpha(img.pixel(x, y)) < 255)
                                            opaque = false;
                                    }
                                }
                            }
                            imageHasAlpha[imgFn] = !opaque;
                        } else {
                            qCWarning(GLTFExporterLog,
                                      "Cannot determine presence of alpha for '%ls'",
                                      qUtf16PrintableImpl(imgFn));
                        }
                    }
                }
                vals[key] = m_textureIdMap.value(it.value());
            }
            for (auto it = matInfo.values.constBegin(); it != matInfo.values.constEnd(); ++it) {
                if (vals.contains(it.key()))
                    continue;
                setVarToJSonObject(vals, it.key(), it.value());
            }
            for (auto it = matInfo.colors.constBegin(); it != matInfo.colors.constEnd(); ++it) {
                if (vals.contains(it.key()))
                    continue;
                // Alpha is supported for the diffuse color. < 1 will enable blending.
                const bool alpha = (it.key() == QStringLiteral("diffuse"))
                        && (matInfo.type != MaterialInfo::TypeCustom);
                if (alpha && it.value().alphaF() < 1.0f)
                    opaque = false;
                vals[it.key()] = col2jsvec(it.value(), alpha);
            }
            // Material is a common material, so export it as such.
            QJsonObject commonMat;
            if (matInfo.type == MaterialInfo::TypeGooch)
                commonMat["technique"] = QStringLiteral("GOOCH"); // Qt3D specific extension
            else if (matInfo.type == MaterialInfo::TypePerVertex)
                commonMat["technique"] = QStringLiteral("PERVERTEX"); // Qt3D specific extension
            else
                commonMat["technique"] = QStringLiteral("PHONG");

            // Set the values as-is. "normalmap" is our own extension, not in the spec.
            // However, RGB colors have to be promoted to RGBA since the spec uses
            // vec4, and all types are pre-defined for common material values.
            promoteColorsToRGBA(&vals);
            if (!vals.isEmpty())
                commonMat["values"] = vals;

            // Blend function handling is our own extension used for Phong Alpha material.
            QJsonObject functions;
            if (!matInfo.blendEquations.isEmpty())
                functions["blendEquationSeparate"] = vec2jsvec(matInfo.blendEquations);
            if (!matInfo.blendArguments.isEmpty())
                functions["blendFuncSeparate"] = vec2jsvec(matInfo.blendArguments);
            if (!functions.isEmpty())
                commonMat["functions"] = functions;
            QJsonObject extensions;
            extensions["KHR_materials_common"] = commonMat;
            materialObj["extensions"] = extensions;
        }

        materials[matInfo.name] = materialObj;
    }
}

void GLTFExporter::exportGenericProperties(QJsonObject &jsonObj, PropertyCacheType type,
                                           QObject *obj)
{
    QVector<QMetaProperty> properties = m_propertyCache.value(type);
    QObject *defaultObject = m_defaultObjectCache.value(type);
    for (const QMetaProperty &property : properties) {
        // Only output property if it is different from default
        QVariant defaultValue = defaultObject->property(property.name());
        QVariant objectValue = obj->property(property.name());
        if (defaultValue != objectValue)
            setVarToJSonObject(jsonObj, QString::fromLatin1(property.name()), objectValue);
    }
}

void GLTFExporter::clearOldExport(const QString &dir)
{
    // Look for .qrc file with same name
    QRegularExpression re(QStringLiteral("<file>(.*)</file>"));
    QFile qrcFile(dir + m_exportName + QStringLiteral(".qrc"));
    if (qrcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!qrcFile.atEnd()) {
            QByteArray line = qrcFile.readLine();
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                QString fileName = match.captured(1);
                QString filePathName = dir + fileName;
                QFile::remove(filePathName);
                qCDebug(GLTFExporterLog, "Removed old file: '%ls'",
                        qUtf16PrintableImpl(filePathName));
            }
        }
        qrcFile.close();
        qrcFile.remove();
        qCDebug(GLTFExporterLog, "Removed old file: '%ls'",
                qUtf16PrintableImpl(qrcFile.fileName()));
    }
}

void GLTFExporter::exportParameter(QJsonObject &jsonObj, const QString &name,
                                   const QVariant &variant)
{
    QLatin1String typeStr("type");
    QLatin1String valueStr("value");

    QJsonObject paramObj;

    if (variant.canConvert<QAbstractTexture *>()) {
        paramObj[typeStr] = GL_SAMPLER_2D;
        paramObj[valueStr] = m_textureIdMap.value(textureVariantToUrl(variant));
    } else {
        switch (QMetaType::Type(variant.type())) {
        case QMetaType::Bool:
            paramObj[typeStr] = GL_BOOL;
            paramObj[valueStr] = variant.toBool();
            break;
        case QMetaType::Int: // fall through
        case QMetaType::Long: // fall through
        case QMetaType::LongLong:
            paramObj[typeStr] = GL_INT;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::UInt: // fall through
        case QMetaType::ULong: // fall through
        case QMetaType::ULongLong:
            paramObj[typeStr] = GL_UNSIGNED_INT;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::Short:
            paramObj[typeStr] = GL_SHORT;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::UShort:
            paramObj[typeStr] = GL_UNSIGNED_SHORT;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::Char:
            paramObj[typeStr] = GL_BYTE;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::UChar:
            paramObj[typeStr] = GL_UNSIGNED_BYTE;
            paramObj[valueStr] = variant.toInt();
            break;
        case QMetaType::QColor:
            paramObj[typeStr] = GL_FLOAT_VEC4;
            paramObj[valueStr] = col2jsvec(variant.value<QColor>(), true);
            break;
        case QMetaType::Float:
            paramObj[typeStr] = GL_FLOAT;
            paramObj[valueStr] = variant.value<float>();
            break;
        case QMetaType::QVector2D:
            paramObj[typeStr] = GL_FLOAT_VEC2;
            paramObj[valueStr] = vec2jsvec(variant.value<QVector2D>());
            break;
        case QMetaType::QVector3D:
            paramObj[typeStr] = GL_FLOAT_VEC3;
            paramObj[valueStr] = vec2jsvec(variant.value<QVector3D>());
            break;
        case QMetaType::QVector4D:
            paramObj[typeStr] = GL_FLOAT_VEC4;
            paramObj[valueStr] = vec2jsvec(variant.value<QVector4D>());
            break;
        case QMetaType::QMatrix4x4:
            paramObj[typeStr] = GL_FLOAT_MAT4;
            paramObj[valueStr] = matrix2jsvec(variant.value<QMatrix4x4>());
            break;
        default:
            qCWarning(GLTFExporterLog, "Unknown value type for '%ls'", qUtf16PrintableImpl(name));
            break;
        }
    }

    jsonObj[name] = paramObj;
}

void GLTFExporter::exportRenderStates(QJsonObject &jsonObj, const QRenderPass *pass)
{
    QJsonArray enableStates;
    QJsonObject funcObj;
    for (QRenderState *state : pass->renderStates()) {
        QJsonArray arr;
        if (qobject_cast<QAlphaCoverage *>(state)) {
            enableStates << GL_SAMPLE_ALPHA_TO_COVERAGE;
        } else if (qobject_cast<QAlphaTest *>(state)) {
            auto s = qobject_cast<QAlphaTest *>(state);
            arr << s->alphaFunction();
            arr << s->referenceValue();
            funcObj["alphaTest"] = arr;
        } else if (qobject_cast<QBlendEquation *>(state)) {
            auto s = qobject_cast<QBlendEquation *>(state);
            arr << s->blendFunction();
            funcObj["blendEquationSeparate"] = arr;
        } else if (qobject_cast<QBlendEquationArguments *>(state)) {
            auto s = qobject_cast<QBlendEquationArguments *>(state);
            arr << s->sourceRgb();
            arr << s->sourceAlpha();
            arr << s->destinationRgb();
            arr << s->destinationAlpha();
            arr << s->bufferIndex();
            funcObj["blendFuncSeparate"] = arr;
        } else if (qobject_cast<QClipPlane *>(state)) {
            auto s = qobject_cast<QClipPlane *>(state);
            arr << s->planeIndex();
            arr << s->normal().x();
            arr << s->normal().y();
            arr << s->normal().z();
            arr << s->distance();
            funcObj["clipPlane"] = arr;
        } else if (qobject_cast<QColorMask *>(state)) {
            auto s = qobject_cast<QColorMask *>(state);
            arr << s->isRedMasked();
            arr << s->isGreenMasked();
            arr << s->isBlueMasked();
            arr << s->isAlphaMasked();
            funcObj["colorMask"] = arr;
        } else if (qobject_cast<QCullFace *>(state)) {
            auto s = qobject_cast<QCullFace *>(state);
            arr << s->mode();
            funcObj["cullFace"] = arr;
        } else if (qobject_cast<QDepthTest *>(state)) {
            auto s = qobject_cast<QDepthTest *>(state);
            arr << s->depthFunction();
            funcObj["depthFunc"] = arr;
        } else if (qobject_cast<QDithering *>(state)) {
            enableStates << GL_DITHER;
        } else if (qobject_cast<QFrontFace *>(state)) {
            auto s = qobject_cast<QFrontFace *>(state);
            arr << s->direction();
            funcObj["frontFace"] = arr;
        } else if (qobject_cast<QFrontFace *>(state)) {
            auto s = qobject_cast<QFrontFace *>(state);
            arr << s->direction();
            funcObj["frontFace"] = arr;
        } else if (qobject_cast<QMultiSampleAntiAliasing *>(state)) {
            enableStates << 0x809D; // GL_MULTISAMPLE
        } else if (qobject_cast<QNoDepthMask *>(state)) {
            arr << false;
            funcObj["depthMask"] = arr;
        } else if (qobject_cast<QPointSize *>(state)) {
            auto s = qobject_cast<QPointSize *>(state);
            arr << s->sizeMode();
            arr << s->value();
            funcObj["pointSize"] = arr;
        } else if (qobject_cast<QPolygonOffset *>(state)) {
            auto s = qobject_cast<QPolygonOffset *>(state);
            arr << s->scaleFactor();
            arr << s->depthSteps();
            funcObj["polygonOffset"] = arr;
        } else if (qobject_cast<QScissorTest *>(state)) {
            auto s = qobject_cast<QScissorTest *>(state);
            arr << s->left();
            arr << s->bottom();
            arr << s->width();
            arr << s->height();
            funcObj["scissor"] = arr;
        } else if (qobject_cast<QSeamlessCubemap *>(state)) {
            enableStates << 0x884F; // GL_TEXTURE_CUBE_MAP_SEAMLESS
        } else if (qobject_cast<QStencilMask *>(state)) {
            auto s = qobject_cast<QStencilMask *>(state);
            arr << int(s->frontOutputMask());
            arr << int(s->backOutputMask());
            funcObj["stencilMask"] = arr;
        } else if (qobject_cast<QStencilOperation *>(state)) {
            auto s = qobject_cast<QStencilOperation *>(state);
            arr << s->front()->stencilTestFailureOperation();
            arr << s->front()->depthTestFailureOperation();
            arr << s->front()->allTestsPassOperation();
            arr << s->back()->stencilTestFailureOperation();
            arr << s->back()->depthTestFailureOperation();
            arr << s->back()->allTestsPassOperation();
            funcObj["stencilOperation"] = arr;
        } else if (qobject_cast<QStencilTest *>(state)) {
            auto s = qobject_cast<QStencilTest *>(state);
            arr << int(s->front()->comparisonMask());
            arr << s->front()->referenceValue();
            arr << s->front()->stencilFunction();
            arr << int(s->back()->comparisonMask());
            arr << s->back()->referenceValue();
            arr << s->back()->stencilFunction();
            funcObj["stencilTest"] = arr;
        }
    }
    if (!enableStates.isEmpty())
        jsonObj["enable"] = enableStates;
    if (!funcObj.isEmpty())
        jsonObj["functions"] = funcObj;
}

QString GLTFExporter::newBufferViewName()
{
    return QString(QStringLiteral("bufferView_%1")).arg(++m_bufferViewCount);
}

QString GLTFExporter::newAccessorName()
{
    return QString(QStringLiteral("accessor_%1")).arg(++m_accessorCount);
}

QString GLTFExporter::newMeshName()
{
    return QString(QStringLiteral("mesh_%1")).arg(++m_meshCount);
}

QString GLTFExporter::newMaterialName()
{
    return QString(QStringLiteral("material_%1")).arg(++m_materialCount);
}

QString GLTFExporter::newTechniqueName()
{
    return QString(QStringLiteral("technique_%1")).arg(++m_techniqueCount);
}

QString GLTFExporter::newTextureName()
{
    return QString(QStringLiteral("texture_%1")).arg(++m_textureCount);
}

QString GLTFExporter::newImageName()
{
    return QString(QStringLiteral("image_%1")).arg(++m_imageCount);
}

QString GLTFExporter::newShaderName()
{
    return QString(QStringLiteral("shader_%1")).arg(++m_shaderCount);
}

QString GLTFExporter::newProgramName()
{
    return QString(QStringLiteral("program_%1")).arg(++m_programCount);
}

QString GLTFExporter::newNodeName()
{
    return QString(QStringLiteral("node_%1")).arg(++m_nodeCount);
}

QString GLTFExporter::newCameraName()
{
    return QString(QStringLiteral("camera_%1")).arg(++m_cameraCount);
}

QString GLTFExporter::newLightName()
{
    return QString(QStringLiteral("light_%1")).arg(++m_lightCount);
}

QString GLTFExporter::newRenderPassName()
{
    return QString(QStringLiteral("renderpass_%1")).arg(++m_renderPassCount);
}

QString GLTFExporter::newEffectName()
{
    return QString(QStringLiteral("effect_%1")).arg(++m_effectCount);
}

QString GLTFExporter::textureVariantToUrl(const QVariant &var)
{
    QString urlString;
    QAbstractTexture *texture = var.value<QAbstractTexture *>();
    if (texture->textureImages().size()) {
        QTextureImage *image = qobject_cast<QTextureImage *>(texture->textureImages().at(0));
        if (image) {
            urlString = QUrlHelper::urlToLocalFileOrQrc(image->source());
            if (!m_textureIdMap.contains(urlString))
                m_textureIdMap.insert(urlString, newTextureName());
        }
    }
    return urlString;
}

void GLTFExporter::setVarToJSonObject(QJsonObject &jsObj, const QString &key, const QVariant &var)
{
    switch (QMetaType::Type(var.type())) {
    case QMetaType::Bool:
        jsObj[key] = var.toBool();
        break;
    case QMetaType::Int:
        jsObj[key] = var.toInt();
        break;
    case QMetaType::Float:
        jsObj[key] = var.value<float>();
        break;
    case QMetaType::QSize:
        jsObj[key] = size2jsvec(var.toSize());
        break;
    case QMetaType::QVector2D:
        jsObj[key] = vec2jsvec(var.value<QVector2D>());
        break;
    case QMetaType::QVector3D:
        jsObj[key] = vec2jsvec(var.value<QVector3D>());
        break;
    case QMetaType::QVector4D:
        jsObj[key] = vec2jsvec(var.value<QVector4D>());
        break;
    case QMetaType::QMatrix4x4:
        jsObj[key] = matrix2jsvec(var.value<QMatrix4x4>());
        break;
    case QMetaType::QString:
        jsObj[key] = var.toString();
        break;
    case QMetaType::QColor:
        jsObj[key] = col2jsvec(var.value<QColor>(), true);
        break;
    default:
        qCWarning(GLTFExporterLog, "Unknown value type for '%ls'", qUtf16PrintableImpl(key));
        break;
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "moc_gltfexporter.cpp"
