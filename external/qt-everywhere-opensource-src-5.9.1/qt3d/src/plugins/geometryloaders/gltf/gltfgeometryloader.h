/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef GLTFGEOMETRYLOADER_H
#define GLTFGEOMETRYLOADER_H

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

#include <Qt3DRender/private/qgeometryloaderinterface_p.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>

#include <private/qlocale_tools_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

#define GLTFGEOMETRYLOADER_EXT QLatin1String("gltf")
#define JSONGEOMETRYLOADER_EXT QLatin1String("json")
#define QGLTFGEOMETRYLOADER_EXT QLatin1String("qgltf")

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
class QGeometry;

class GLTFGeometryLoader : public QGeometryLoaderInterface
{
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
        explicit AccessorData(const QJsonObject &json);

        QString bufferViewName;
        QAttribute::VertexBaseType type;
        uint dataSize;
        int count;
        int offset;
        int stride;
    };

    Q_OBJECT
public:
    GLTFGeometryLoader();
    ~GLTFGeometryLoader();

    QGeometry *geometry() const Q_DECL_FINAL;

    bool load(QIODevice *ioDev, const QString &subMesh = QString()) Q_DECL_FINAL;

protected:
    void setBasePath(const QString &path);
    bool setJSON(const QJsonDocument &json);

    static QString standardAttributeNameFromSemantic(const QString &semantic);

    void parse();
    void cleanup();

    void processJSONBuffer(const QString &id, const QJsonObject &json);
    void processJSONBufferView(const QString &id, const QJsonObject &json);
    void processJSONAccessor(const QString &id, const QJsonObject &json);
    void processJSONMesh(const QString &id, const QJsonObject &json);

    void loadBufferData();
    void unloadBufferData();

    QByteArray resolveLocalData(const QString &path) const;

    static QAttribute::VertexBaseType accessorTypeFromJSON(int componentType);
    static uint accessorDataSizeFromJson(const QString &type);

private:
    QJsonDocument m_json;
    QString m_basePath;
    QString m_mesh;

    QHash<QString, AccessorData> m_accessorDict;

    QHash<QString, BufferData> m_bufferDatas;
    QHash<QString, Qt3DRender::QBuffer*> m_buffers;

    QGeometry *m_geometry;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // GLTFGEOMETRYLOADER_H
