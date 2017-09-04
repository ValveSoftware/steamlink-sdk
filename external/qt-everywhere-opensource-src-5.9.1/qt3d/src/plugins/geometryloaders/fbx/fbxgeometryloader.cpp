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

#include "fbxgeometryloader.h"

#include <QtCore/QFileDevice>

#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/private/renderlogging_p.h>

#include <fbxsdk.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

Q_LOGGING_CATEGORY(FbxGeometryLoaderLog, "Qt3D.FbxGeometryLoader", QtWarningMsg)

class FbxStreamWrapper : public FbxStream
{
    FbxManager *m_manager;
    QIODevice *m_device;
public:
    FbxStreamWrapper(FbxManager *manager)
        : m_manager(manager)
        , m_device(nullptr)
    {
    }

    EState GetState()
    {
        if (!m_device)
            return eEmpty;
        return m_device->isOpen() ? eOpen : eClosed;
    }

    bool Open(void *pStreamData)
    {
        m_device = reinterpret_cast<QIODevice *>(pStreamData);

        if (!m_device->isOpen())
            m_device->open(QIODevice::ReadOnly);

        return m_device && m_device->isOpen();
    }

    bool Close()
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return false;

        m_device->close();

        return !m_device->isOpen();
    }

    bool Flush()
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return false;

        QFileDevice *device = qobject_cast<QFileDevice*>(m_device);
        if (Q_LIKELY(device))
            return device->flush();

        return false;
    }

    int Write(const void *pData, int pSize)
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return -1;

        return m_device->write(reinterpret_cast<const char *>(pData), pSize);
    }

    int Read(void *pData, int pSize) const
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return -1;

        return m_device->read(reinterpret_cast<char *>(pData), pSize);
    }

    char *ReadString(char *pBuffer, int pMaxSize, bool pStopAtFirstWhiteSpace = false)
    {
        Q_UNUSED(pBuffer);
        Q_UNUSED(pMaxSize);
        Q_UNUSED(pStopAtFirstWhiteSpace);
        return nullptr;
    }

    int GetReaderID() const
    {
        return m_manager->GetIOPluginRegistry()->
            FindReaderIDByExtension(qPrintable(FBXGEOMETRYLOADER_EXT));
    }

    int GetWriterID() const
    {
        return m_manager->GetIOPluginRegistry()->
            FindReaderIDByExtension(qPrintable(FBXGEOMETRYLOADER_EXT));
    }

    void Seek(const FbxInt64 &pOffset, const FbxFile::ESeekPos &pSeekPos)
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return;

        switch (pSeekPos) {
        case FbxFile::eBegin:
            m_device->seek(pOffset);
            break;
        case FbxFile::eCurrent:
            m_device->seek(m_device->pos() + pOffset);
            break;
        case FbxFile::eEnd:
            m_device->seek(m_device->size() - pOffset);
            break;
        }
    }

    long GetPosition() const
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return -1;

        return m_device->pos();
    }

    void SetPosition(long pPosition)
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return;

        m_device->seek(pPosition);
    }

    int GetError() const
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return 1;

        QFileDevice *device = qobject_cast<QFileDevice *>(m_device);
        if (Q_LIKELY(device))
            return device->error() == QFileDevice::NoError ? 0 : 1;

        return 0;
    }

    void ClearError()
    {
        Q_ASSERT(m_device);

        if (Q_UNLIKELY(!m_device))
            return;

        QFileDevice *device = qobject_cast<QFileDevice *>(m_device);
        if (Q_LIKELY(device))
            device->unsetError();
    }
};

FbxGeometryLoader::FbxGeometryLoader()
    : m_manager(nullptr)
    , m_scene(nullptr)
    , m_geometry(nullptr)
{
    m_manager = FbxManager::Create();

    if (Q_LIKELY(m_manager))
        qInfo(FbxGeometryLoaderLog, "Autodesk FBX SDK version %s", m_manager->GetVersion());
    else
        qWarning(FbxGeometryLoaderLog, "Failed to create FBX Manager.");
}

FbxGeometryLoader::~FbxGeometryLoader()
{
    if (m_manager)
        m_manager->Destroy();
}

QGeometry *FbxGeometryLoader::geometry() const
{
    return m_geometry;
}

bool FbxGeometryLoader::load(QIODevice *ioDev, const QString &subMesh)
{
    if (m_scene)
        m_scene->Destroy();

    m_scene = FbxScene::Create(m_manager, "scene");
    if (!m_scene)
        qWarning(FbxGeometryLoaderLog, "Unable to create FBX scene!");

    QScopedPointer<FbxStreamWrapper> fbxStream(new FbxStreamWrapper(m_manager));

    FbxImporter *importer = FbxImporter::Create(m_manager, "");

    const bool hasInitialized = importer->Initialize(fbxStream.data(), ioDev);

    int fileVersion[3];

    importer->GetFileVersion(fileVersion[0], fileVersion[1], fileVersion[2]);

    if (!hasInitialized) {
        FbxString error = importer->GetStatus().GetErrorString();
        qWarning(FbxGeometryLoaderLog, "Call to FbxImporter::Initialize() failed.");
        qWarning(FbxGeometryLoaderLog, "Error returned: %s", error.Buffer());

        if (importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion) {
            int SdkVersion[3];
            FbxManager::GetFileFormatVersion(SdkVersion[0], SdkVersion[1], SdkVersion[2]);
            qWarning(FbxGeometryLoaderLog, "FBX file format version for this FBX SDK is %d.%d.%d",
                     SdkVersion[0], SdkVersion[1], SdkVersion[2]);
            qWarning(FbxGeometryLoaderLog, "FBX file format version for the file is %d.%d.%d",
                     fileVersion[0], fileVersion[1], fileVersion[2]);
        }

        return false;
    }

    if (importer->IsFBX()) {
        qInfo(FbxGeometryLoaderLog, "FBX file format version for file is %d.%d.%d",
               fileVersion[0], fileVersion[1], fileVersion[2]);

        const int stackCount = importer->GetAnimStackCount();
        for (int i = 0; i < stackCount; ++i) {
            FbxTakeInfo *lTakeInfo = importer->GetTakeInfo(i);
            qInfo(FbxGeometryLoaderLog, "  Animation Stack %d", i);
            qInfo(FbxGeometryLoaderLog, "       Name: \"%s\"", lTakeInfo->mName.Buffer());
            qInfo(FbxGeometryLoaderLog, "       Description: \"%s\"", lTakeInfo->mDescription.Buffer());
            qInfo(FbxGeometryLoaderLog, "       Import Name: \"%s\"", lTakeInfo->mImportName.Buffer());
            qInfo(FbxGeometryLoaderLog, "       Import State: %s", lTakeInfo->mSelect ? "true" : "false");
        }

        auto settings = importer->GetIOSettings();
        settings->SetBoolProp(IMP_FBX_MATERIAL, false);
        settings->SetBoolProp(IMP_FBX_TEXTURE, false);
        settings->SetBoolProp(IMP_FBX_LINK, false);
        settings->SetBoolProp(IMP_FBX_SHAPE, false);
        settings->SetBoolProp(IMP_FBX_GOBO, false);
        settings->SetBoolProp(IMP_FBX_ANIMATION, false);
        settings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, false);
    }

    const bool wasImported = importer->Import(m_scene);
    importer->Destroy();

    m_mesh = subMesh;

    if (wasImported)
        recurseNodes();

    return wasImported;
}

void FbxGeometryLoader::recurseNodes()
{
    Q_ASSERT(m_scene);

    if (!m_scene)
        return;

    FbxNode *node = m_scene->GetRootNode();
    if (node) {
        for (int i = 0; i < node->GetChildCount() && !m_geometry; ++i)
            processNode(node->GetChild(i));
    }
}

void FbxGeometryLoader::processNode(FbxNode *node)
{
    auto attr = node->GetNodeAttribute();
    if (!attr)
        return;

    switch (attr->GetAttributeType()) {
    case FbxNodeAttribute::eMesh:
        if (m_mesh.isEmpty() || m_mesh.compare(node->GetName(), Qt::CaseInsensitive) == 0) {
            qDebug(FbxGeometryLoaderLog, "Found mesh: %s", node->GetName());
            processMesh(node->GetMesh());
        }
        break;
    default:
        break;
    }

    if (m_geometry)
        return;

    for (int i = 0; i < node->GetChildCount(); ++i)
        processNode(node->GetChild(i));
}

void FbxGeometryLoader::processMesh(FbxMesh *mesh)
{
    const int normalCount = mesh->GetElementNormalCount();
    const int polygonCount = mesh->GetPolygonCount();
    const int tangentCount = mesh->GetElementTangentCount();
    const int uvCount = mesh->GetElementUVCount();

    const bool hasNormal = (normalCount > 0);
    const bool hasTangent = (tangentCount > 0);
    const bool hasUV = (uvCount > 0);

    const unsigned int elementSize = 3 + (hasUV ? 2 : 0) + (hasNormal ? 3 : 0) + (hasTangent ? 4 : 0);
    const unsigned int elementBytes = elementSize * sizeof(double);

    int vertexCount = 0;
    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
        vertexCount += mesh->GetPolygonSize(polygonIndex);
    const int indexCount = (polygonCount * 3) + ((vertexCount - (polygonCount * 3)) * 3);

    QByteArray indexPayload;
    indexPayload.resize(indexCount * sizeof(quint32));
    quint32 *indexData = reinterpret_cast<quint32 *>(indexPayload.data());

    QByteArray vertexPayload;
    vertexPayload.resize(vertexCount * elementBytes);
    double *vertexData = reinterpret_cast<double *>(vertexPayload.data());

    const FbxVector4 *controlPoints = mesh->GetControlPoints();

    int vertexIndex = 0;
    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex) {
        const int polygonSize = mesh->GetPolygonSize(polygonIndex);
        for (int pVertexIndex = 0; pVertexIndex < polygonSize; ++pVertexIndex) {
            const int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, pVertexIndex);
            const FbxVector4 *vertex = (controlPoints + controlPointIndex);
            *vertexData++ = (*vertex)[0];
            *vertexData++ = (*vertex)[1];
            *vertexData++ = (*vertex)[2];

            if (pVertexIndex >= 1 && pVertexIndex < (polygonSize - 1)) {
                *indexData++ = vertexIndex - pVertexIndex;
                *indexData++ = vertexIndex;
                *indexData++ = vertexIndex + 1;
            }

            if (hasUV) {
                FbxVector2 vector;
                bool unmapped;

                if (mesh->GetPolygonVertexUV(polygonIndex, pVertexIndex, NULL, vector, unmapped)) {
                    *vertexData++ = unmapped ? 0 : vector[0];
                    *vertexData++ = unmapped ? 0 : vector[1];
                } else {
                    qWarning(FbxGeometryLoaderLog,
                             "Irregularity encountered while parsing UV element.");
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                }
            }

            if (hasNormal) {
                FbxVector4 vector;
                if (mesh->GetPolygonVertexNormal(polygonIndex, pVertexIndex, vector)) {
                    *vertexData++ = vector[0];
                    *vertexData++ = vector[1];
                    *vertexData++ = vector[2];
                } else {
                    qWarning(FbxGeometryLoaderLog,
                             "Irregularity encountered while parsing Normal element.");
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                }
            }

            if (hasTangent) {
                int index = -1;

                for (int tangentIndex = 0; tangentIndex < tangentCount && index == -1; ++tangentIndex) {
                    const FbxGeometryElementTangent *tangent = mesh->GetElementTangent(tangentIndex);

                    if (tangent->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
                        switch (tangent->GetReferenceMode()) {
                        case FbxGeometryElement::eDirect:
                            index = vertexIndex;
                            break;
                        case FbxGeometryElement::eIndexToDirect:
                            index = tangent->GetIndexArray().GetAt(vertexIndex);
                            break;
                        default: break;
                        }
                    }

                    if (index != -1) {
                        const FbxVector4 vector = tangent->GetDirectArray().GetAt(index);
                        *vertexData++ = vector[0];
                        *vertexData++ = vector[1];
                        *vertexData++ = vector[2];
                        *vertexData++ = vector[3];
                    }
                }

                if (index == -1) {
                    qWarning(FbxGeometryLoaderLog,
                             "Irregularity encountered while parsing Tangent element.");
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                    *vertexData++ = 0;
                }
            }
            ++vertexIndex;
        }
    }

    /*
     * QGeometry Generation
     */
    m_geometry = new QGeometry();

    unsigned int offset = 0;

    QBuffer *vertexBuffer = new QBuffer(QBuffer::VertexBuffer);
    vertexBuffer->setData(vertexPayload);

    QBuffer *indexBuffer = new QBuffer(QBuffer::IndexBuffer);
    indexBuffer->setData(indexPayload);

    QAttribute *positionAttribute = new QAttribute(vertexBuffer, QAttribute::defaultPositionAttributeName(), QAttribute::Double, 3, vertexCount, offset, elementBytes);
    m_geometry->addAttribute(positionAttribute);
    offset += sizeof(double) * 3;

    if (hasUV) {
        QAttribute *attribute = new QAttribute(vertexBuffer, QAttribute::defaultTextureCoordinateAttributeName(), QAttribute::Double, 2, vertexCount, offset, elementBytes);
        m_geometry->addAttribute(attribute);
        offset += sizeof(double) * 2;
    }

    if (hasNormal) {
        QAttribute *attribute = new QAttribute(vertexBuffer, QAttribute::defaultNormalAttributeName(), QAttribute::Double, 3, vertexCount, offset, elementBytes);
        m_geometry->addAttribute(attribute);
        offset += sizeof(double) * 3;
    }

    if (hasTangent) {
        QAttribute *attribute = new QAttribute(vertexBuffer, QAttribute::defaultTangentAttributeName(),QAttribute::Double, 4, vertexCount, offset, elementBytes);
        m_geometry->addAttribute(attribute);
    }

    QAttribute *indexAttribute = new QAttribute(indexBuffer, QAttribute::UnsignedInt, 1, indexCount);
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    m_geometry->addAttribute(indexAttribute);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
