/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "objgeometryloader.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

Q_LOGGING_CATEGORY(ObjGeometryLoaderLog, "Qt3D.ObjGeometryLoader", QtWarningMsg)

static void addFaceVertex(const FaceIndices &faceIndices,
                          QVector<FaceIndices>& faceIndexVector,
                          QHash<FaceIndices, unsigned int>& faceIndexMap);

inline uint qHash(const FaceIndices &faceIndices)
{
    return faceIndices.positionIndex
            + 10 * faceIndices.texCoordIndex
            + 100 * faceIndices.normalIndex;
}

bool ObjGeometryLoader::doLoad(QIODevice *ioDev, const QString &subMesh)
{
    int faceCount = 0;

    // Parse faces taking into account each vertex in a face can index different indices
    // for the positions, normals and texture coords;
    // Generate unique vertices (in OpenGL parlance) and output to points, texCoords,
    // normals and calculate mapping from faces to unique indices
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<QVector2D> texCoords;
    QHash<FaceIndices, unsigned int> faceIndexMap;
    QVector<FaceIndices> faceIndexVector;

    bool skipping = false;
    int positionsOffset = 0;
    int normalsOffset = 0;
    int texCoordsOffset = 0;

    QRegularExpression subMeshMatch(subMesh);
    if (!subMeshMatch.isValid())
        subMeshMatch.setPattern(QLatin1String("^(") + subMesh + QLatin1String(")$"));
    Q_ASSERT(subMeshMatch.isValid());

    char lineBuffer[1024];
    const char *line;
    QByteArray longLine;
    while (!ioDev->atEnd()) {
        // try to read into lineBuffer first, if the line fits (common case) we can do this without expensive allocations
        // if not, fall back to dynamically allocated QByteArrays
        auto lineSize = ioDev->readLine(lineBuffer, sizeof(lineBuffer));
        if (lineSize == sizeof(lineBuffer) - 1 && lineBuffer[lineSize - 1] != '\n') {
            longLine = QByteArray(lineBuffer, lineSize);
            longLine += ioDev->readLine();
            line = longLine.constData();
            lineSize = longLine.size();
        } else {
            line = lineBuffer;
        }

        if (lineSize > 0 && line[0] != '#') {
            if (line[lineSize - 1] == '\n')
                --lineSize; // chop newline

            const ByteArraySplitter tokens(line, line + lineSize, ' ', QString::SkipEmptyParts);

            if (qstrncmp(tokens.charPtrAt(0), "v ", 2) == 0) {
                if (tokens.size() < 4) {
                    qCWarning(ObjGeometryLoaderLog) << "Unsupported number of components in vertex";
                } else {
                    if (!skipping) {
                        const float x = tokens.floatAt(1);
                        const float y = tokens.floatAt(2);
                        const float z = tokens.floatAt(3);
                        positions.append(QVector3D(x, y, z));
                    } else {
                        positionsOffset++;
                    }
                }
            } else if (m_loadTextureCoords && qstrncmp(tokens.charPtrAt(0), "vt ", 3) == 0) {
                if (tokens.size() < 3) {
                    qCWarning(ObjGeometryLoaderLog) << "Unsupported number of components in texture coordinate";
                } else {
                    if (!skipping) {
                        // Process texture coordinate
                        const float s = tokens.floatAt(1);
                        const float t = tokens.floatAt(2);
                        texCoords.append(QVector2D(s, t));
                    } else {
                        ++texCoordsOffset;
                    }
                }
            } else if (qstrncmp(tokens.charPtrAt(0), "vn ", 3) == 0) {
                if (tokens.size() < 4) {
                    qCWarning(ObjGeometryLoaderLog) << "Unsupported number of components in vertex normal";
                } else {
                    if (!skipping) {
                        const float x = tokens.floatAt(1);
                        const float y = tokens.floatAt(2);
                        const float z = tokens.floatAt(3);
                        normals.append(QVector3D(x, y, z));
                    } else {
                        ++normalsOffset;
                    }
                }
            } else if (!skipping && qstrncmp(tokens.charPtrAt(0), "f ", 2) == 0) {
                // Process face
                ++faceCount;

                int faceVertices = tokens.size() - 1;

                QVarLengthArray<FaceIndices, 4> face; // try to avoid allocations in the common case of triangulated data
                face.reserve(faceVertices);

                for (int i = 0; i < faceVertices; i++) {
                    FaceIndices faceIndices;
                    const ByteArraySplitter indices = tokens.splitterAt(i + 1, '/', QString::KeepEmptyParts);
                    switch (indices.size()) {
                    case 3:
                        faceIndices.normalIndex = indices.intAt(2) - 1 - normalsOffset;  // fall through
                    case 2:
                        faceIndices.texCoordIndex = indices.intAt(1) - 1 - texCoordsOffset; // fall through
                    case 1:
                        faceIndices.positionIndex = indices.intAt(0) - 1 - positionsOffset;
                        break;
                    default:
                        qCWarning(ObjGeometryLoaderLog) << "Unsupported number of indices in face element";
                    }

                    face.append(faceIndices);
                }

                // If number of edges in face is greater than 3,
                // decompose into triangles as a triangle fan.
                FaceIndices v0 = face[0];
                FaceIndices v1 = face[1];
                FaceIndices v2 = face[2];

                // First face
                addFaceVertex(v0, faceIndexVector, faceIndexMap);
                addFaceVertex(v1, faceIndexVector, faceIndexMap);
                addFaceVertex(v2, faceIndexVector, faceIndexMap);

                for (int i = 3; i < face.size(); ++i) {
                    v1 = v2;
                    v2 = face[i];
                    addFaceVertex(v0, faceIndexVector, faceIndexMap);
                    addFaceVertex(v1, faceIndexVector, faceIndexMap);
                    addFaceVertex(v2, faceIndexVector, faceIndexMap);
                }

                // end of face
            } else if (qstrncmp(tokens.charPtrAt(0), "o ", 2) == 0) {
                if (tokens.size() < 2) {
                    qCWarning(ObjGeometryLoaderLog) << "Missing submesh name";
                } else {
                    if (!subMesh.isEmpty() ) {
                        const QString objName = tokens.stringAt(1);
                        QRegularExpressionMatch match = subMeshMatch.match(objName);
                        skipping = !match.hasMatch();
                    }
                }
            }
        } // empty input line
    } // while (!ioDev->atEnd())

    // Iterate over the faceIndexMap and pull out pos, texCoord and normal data
    // thereby generating unique vertices of data (by OpenGL definition)
    const int vertexCount = faceIndexMap.size();
    const bool hasTexCoords = !texCoords.isEmpty();
    const bool hasNormals = !normals.isEmpty();

    m_points.resize(vertexCount);
    m_texCoords.clear();
    if (hasTexCoords)
        m_texCoords.resize(vertexCount);
    m_normals.clear();
    if (hasNormals)
        m_normals.resize(vertexCount);

    for (auto it = faceIndexMap.cbegin(), endIt = faceIndexMap.cend(); it != endIt; ++it) {
        m_points[it.value()] = positions[it.key().positionIndex];
        if (hasTexCoords)
            m_texCoords[it.value()] = std::numeric_limits<unsigned int>::max() != it.key().texCoordIndex ? texCoords[it.key().texCoordIndex] : QVector2D();
        if (hasNormals)
            m_normals[it.value()] = normals[it.key().normalIndex];
    }

    // Now iterate over the face indices and lookup the unique vertex index
    const int indexCount = faceIndexVector.size();
    m_indices.clear();
    m_indices.reserve(indexCount);
    for (const FaceIndices faceIndices : qAsConst(faceIndexVector)) {
        const unsigned int i = faceIndexMap.value(faceIndices);
        m_indices.append(i);
    }

    return true;
}

static void addFaceVertex(const FaceIndices &faceIndices,
                          QVector<FaceIndices>& faceIndexVector,
                          QHash<FaceIndices, unsigned int>&faceIndexMap)
{
    if (faceIndices.positionIndex != std::numeric_limits<unsigned int>::max()) {
        faceIndexVector.append(faceIndices);
        if (!faceIndexMap.contains(faceIndices))
            faceIndexMap.insert(faceIndices, faceIndexMap.size());
    } else {
        qCWarning(ObjGeometryLoaderLog) << "Missing position index";
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
