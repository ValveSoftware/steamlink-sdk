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

#ifndef QT3DRENDER_BASEGEOMETRYLOADER_H
#define QT3DRENDER_BASEGEOMETRYLOADER_H

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

#include <QtCore/QObject>
#include <QtCore/QVector>

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <Qt3DRender/private/qgeometryloaderinterface_p.h>

#include <private/qlocale_tools_p.h>

QT_BEGIN_NAMESPACE

class QIODevice;
class QString;

namespace Qt3DRender {

class QGeometry;

class BaseGeometryLoader : public QGeometryLoaderInterface
{
    Q_OBJECT
public:
    BaseGeometryLoader();

    void setLoadTextureCoordinatesEnabled(bool b) { m_loadTextureCoords = b; }
    bool isLoadTextureCoordinatesEnabled() const { return m_loadTextureCoords; }

    void setTangentGenerationEnabled(bool b) { m_generateTangents = b; }
    bool isTangentGenerationEnabled() const { return m_generateTangents; }

    void setMeshCenteringEnabled(bool b) { m_centerMesh = b; }
    bool isMeshCenteringEnabled() const { return m_centerMesh; }

    bool hasNormals() const { return !m_normals.isEmpty(); }
    bool hasTextureCoordinates() const { return !m_texCoords.isEmpty(); }
    bool hasTangents() const { return !m_tangents.isEmpty(); }

    QVector<QVector3D> vertices() const { return m_points; }
    QVector<QVector3D> normals() const { return m_normals; }
    QVector<QVector2D> textureCoordinates() const { return m_texCoords; }
    QVector<QVector4D> tangents() const { return m_tangents; }
    QVector<unsigned int> indices() const { return m_indices; }

    QGeometry *geometry() const Q_DECL_OVERRIDE;

    bool load(QIODevice *ioDev, const QString &subMesh = QString()) Q_DECL_OVERRIDE;

protected:
    virtual bool doLoad(QIODevice *ioDev, const QString &subMesh = QString()) = 0;

    void generateAveragedNormals(const QVector<QVector3D>& points,
                                 QVector<QVector3D>& normals,
                                 const QVector<unsigned int>& faces) const;
    void generateGeometry();
    void generateTangents(const QVector<QVector3D>& points,
                          const QVector<QVector3D>& normals,
                          const QVector<unsigned int>& faces,
                          const QVector<QVector2D>& texCoords,
                          QVector<QVector4D>& tangents) const;
    void center(QVector<QVector3D>& points);

    bool m_loadTextureCoords;
    bool m_generateTangents;
    bool m_centerMesh;

    QVector<QVector3D> m_points;
    QVector<QVector3D> m_normals;
    QVector<QVector2D> m_texCoords;
    QVector<QVector4D> m_tangents;
    QVector<unsigned int> m_indices;

    QGeometry *m_geometry;
};

struct FaceIndices
{
    FaceIndices()
        : positionIndex(std::numeric_limits<unsigned int>::max())
        , texCoordIndex(std::numeric_limits<unsigned int>::max())
        , normalIndex(std::numeric_limits<unsigned int>::max())
    {}

    FaceIndices(unsigned int posIndex, unsigned int tcIndex, unsigned int nIndex)
        : positionIndex(posIndex)
        , texCoordIndex(tcIndex)
        , normalIndex(nIndex)
    {}

    bool operator == (const FaceIndices &other) const
    {
        return positionIndex == other.positionIndex &&
               texCoordIndex == other.texCoordIndex &&
               normalIndex == other.normalIndex;
    }

    unsigned int positionIndex;
    unsigned int texCoordIndex;
    unsigned int normalIndex;
};
QT3D_DECLARE_TYPEINFO(Qt3DRender, FaceIndices, Q_PRIMITIVE_TYPE)

struct ByteArraySplitterEntry
{
    int start;
    int size;
};
QT3D_DECLARE_TYPEINFO(Qt3DRender, ByteArraySplitterEntry, Q_PRIMITIVE_TYPE)

/*
 * A helper class to split a QByteArray and access its sections without
 * additional memory allocations.
 */
class ByteArraySplitter
{
public:
    explicit ByteArraySplitter(const char *begin, const char *end, char delimiter, QString::SplitBehavior splitBehavior)
        : m_input(begin)
    {
        int position = 0;
        int lastPosition = 0;
        for (auto it = begin; it != end; ++it) {
            if (*it == delimiter) {
                if (position > lastPosition || splitBehavior == QString::KeepEmptyParts) { // skip multiple consecutive delimiters
                    const ByteArraySplitterEntry entry = { lastPosition, position - lastPosition };
                    m_entries.append(entry);
                }
                lastPosition = position + 1;
            }

            ++position;
        }

        const ByteArraySplitterEntry entry = { lastPosition, position - lastPosition };
        m_entries.append(entry);
    }

    int size() const
    {
        return m_entries.size();
    }

    const char *charPtrAt(int index) const
    {
        return m_input + m_entries[index].start;
    }

    float floatAt(int index) const
    {
        return qstrntod(m_input + m_entries[index].start, m_entries[index].size, nullptr, nullptr);
    }

    int intAt(int index) const
    {
        return strtol(m_input + m_entries[index].start, nullptr, 10);
    }

    QString stringAt(int index) const
    {
        return QString::fromLatin1(m_input + m_entries[index].start, m_entries[index].size);
    }

    ByteArraySplitter splitterAt(int index, char delimiter, QString::SplitBehavior splitBehavior) const
    {
        return ByteArraySplitter(m_input + m_entries[index].start, m_input + m_entries[index].start + m_entries[index].size, delimiter, splitBehavior);
    }

private:
    QVarLengthArray<ByteArraySplitterEntry, 16> m_entries;
    const char *m_input;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_BASEGEOMETRYLOADER_H
