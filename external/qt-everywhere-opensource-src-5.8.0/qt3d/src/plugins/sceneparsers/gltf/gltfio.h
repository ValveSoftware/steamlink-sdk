/****************************************************************************
**
** Copyright (C) 2014-2016 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef GLTFIO_H
#define GLTFIO_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QJsonDocument>
#include <QtCore/QMultiHash>

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/private/qsceneiohandler_p.h>

QT_BEGIN_NAMESPACE

class QByteArray;

namespace Qt3DCore {
class QEntity;
}

namespace Qt3DRender {

class QCamera;
class QCameraLens;
class QMaterial;
class QShaderProgram;
class QEffect;
class QAbstractTexture;
class QRenderState;
class QTechnique;
class QParameter;
class QGeometryRenderer;

Q_DECLARE_LOGGING_CATEGORY(GLTFIOLog)

class GLTFIO : public QSceneIOHandler
{
    Q_OBJECT

public:
    GLTFIO();
    ~GLTFIO();

    void setBasePath(const QString& path);
    bool setJSON( const QJsonDocument &json );

    // SceneParserInterface interface
    void setSource(const QUrl &source) Q_DECL_FINAL;
    bool isFileTypeSupported(const QUrl &source) const Q_DECL_FINAL;
    Qt3DCore::QEntity *node(const QString &id) Q_DECL_FINAL;
    Qt3DCore::QEntity *scene(const QString &id = QString()) Q_DECL_FINAL;

private:
    class BufferData
    {
    public:
        BufferData();
        explicit BufferData(const QJsonObject &json);

        quint64 length;
        QString path;
        QByteArray *data;
        // type if ever useful
    };

    class ParameterData
    {
    public:
        ParameterData();
        explicit ParameterData(const QJsonObject &json);

        QString semantic;
        int type;
    };

    class AccessorData
    {
    public:
        AccessorData();
        explicit AccessorData(const QJsonObject& json);

        QString bufferViewName;
        QAttribute::VertexBaseType type;
        uint dataSize;
        int count;
        int offset;
        int stride;
    };

    static bool isGLTFPath(const QString &path);
    static void renameFromJson(const QJsonObject& json, QObject * const object );
    static bool hasStandardUniformNameFromSemantic(const QString &semantic);
    static QString standardAttributeNameFromSemantic(const QString &semantic);
    static QParameter *parameterFromTechnique(QTechnique *technique, const QString &parameterName);

    Qt3DCore::QEntity *defaultScene();
    QMaterial *material(const QString &id);
    QCameraLens *camera(const QString &id) const;

    void parse();
    void cleanup();

    void processJSONBuffer(const QString &id, const QJsonObject &json);
    void processJSONBufferView(const QString &id, const QJsonObject &json);
    void processJSONShader(const QString &id, const QJsonObject &jsonObject);
    void processJSONProgram(const QString &id, const QJsonObject &jsonObject);
    void processJSONTechnique(const QString &id, const QJsonObject &jsonObject);
    void processJSONAccessor(const QString &id, const QJsonObject &json);
    void processJSONMesh(const QString &id, const QJsonObject &json);
    void processJSONImage(const QString &id, const QJsonObject &jsonObject);
    void processJSONTexture(const QString &id, const QJsonObject &jsonObject);

    void loadBufferData();
    void unloadBufferData();

    QByteArray resolveLocalData(const QString &path) const;

    QVariant parameterValueFromJSON(int type, const QJsonValue &value) const;
    static QAttribute::VertexBaseType accessorTypeFromJSON(int componentType);
    static uint accessorDataSizeFromJson(const QString &type);

    static QRenderState *buildStateEnable(int state);
    static QRenderState *buildState(const QString& functionName, const QJsonValue &value, int &type);

    QMaterial *materialWithCustomShader(const QString &id, const QJsonObject &jsonObj);
    QMaterial *commonMaterial(const QJsonObject &jsonObj);

    QJsonDocument m_json;
    QString m_basePath;
    bool m_parseDone;
    QString m_defaultScene;

    // multi-hash because our QMeshData corresponds to a single primitive
    // in glTf.
    QMultiHash<QString, QGeometryRenderer*> m_meshDict;

    // GLTF assigns materials at the mesh level, but we do them as siblings,
    // so record the association here for when we instantiate meshes
    QHash<QGeometryRenderer*, QString> m_meshMaterialDict;

    QHash<QString, AccessorData> m_accessorDict;

    QHash<QString, QMaterial*> m_materialCache;

    QHash<QString, BufferData> m_bufferDatas;
    QHash<QString, Qt3DRender::QBuffer*> m_buffers;

    QHash<QString, QString> m_shaderPaths;
    QHash<QString, QShaderProgram*> m_programs;

    QHash<QString, QTechnique *> m_techniques;
    QHash<QParameter*, ParameterData> m_parameterDataDict;

    QHash<QString, QAbstractTexture*> m_textures;
    QHash<QString, QString> m_imagePaths;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // GLTFIO_H
