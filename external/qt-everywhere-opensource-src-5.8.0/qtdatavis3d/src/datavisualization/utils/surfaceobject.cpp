/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "surfaceobject_p.h"
#include "surface3drenderer_p.h"

#include <QtGui/QVector2D>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

SurfaceObject::SurfaceObject(Surface3DRenderer *renderer)
    : m_surfaceType(Undefined),
      m_columns(0),
      m_rows(0),
      m_gridIndexCount(0),
      m_axisCacheX(renderer->m_axisCacheX),
      m_axisCacheY(renderer->m_axisCacheY),
      m_axisCacheZ(renderer->m_axisCacheZ),
      m_renderer(renderer),
      m_returnTextureBuffer(false),
      m_dataDimension(0),
      m_oldDataDimension(-1)
{
    glGenBuffers(1, &m_vertexbuffer);
    glGenBuffers(1, &m_normalbuffer);
    glGenBuffers(1, &m_uvbuffer);
    glGenBuffers(1, &m_elementbuffer);
    glGenBuffers(1, &m_gridElementbuffer);
    glGenBuffers(1, &m_uvTextureBuffer);
}

SurfaceObject::~SurfaceObject()
{
    if (QOpenGLContext::currentContext()) {
        glDeleteBuffers(1, &m_gridElementbuffer);
        glDeleteBuffers(1, &m_uvTextureBuffer);
    }
}

void SurfaceObject::setUpSmoothData(const QSurfaceDataArray &dataArray, const QRect &space,
                                    bool changeGeometry, bool polar, bool flipXZ)
{
    m_columns = space.width();
    m_rows = space.height();
    int totalSize = m_rows * m_columns;
    GLfloat uvX = 1.0f / GLfloat(m_columns - 1);
    GLfloat uvY = 1.0f / GLfloat(m_rows - 1);

    m_surfaceType = SurfaceSmooth;

    checkDirections(dataArray);
    bool indicesDirty = false;
    if (m_dataDimension != m_oldDataDimension)
        indicesDirty = true;
    m_oldDataDimension = m_dataDimension;

    // Create/populate vertix table
    if (changeGeometry)
        m_vertices.resize(totalSize);

    QVector<QVector2D> uvs;
    if (changeGeometry)
        uvs.resize(totalSize);
    int totalIndex = 0;

    // Init min and max to ridiculous values
    m_minY = 10000000.0;
    m_maxY = -10000000.0f;

    for (int i = 0; i < m_rows; i++) {
        const QSurfaceDataRow &p = *dataArray.at(i);
        for (int j = 0; j < m_columns; j++) {
            getNormalizedVertex(p.at(j), m_vertices[totalIndex], polar, flipXZ);
            if (changeGeometry)
                uvs[totalIndex] = QVector2D(GLfloat(j) * uvX, GLfloat(i) * uvY);
            totalIndex++;
        }
    }

    if (flipXZ) {
        for (int i = 0; i < m_vertices.size(); i++) {
            m_vertices[i].setX(-m_vertices.at(i).x());
            m_vertices[i].setZ(-m_vertices.at(i).z());
        }
    }

    // Create normals
    int rowLimit = m_rows - 1;
    int colLimit = m_columns - 1;
    if (changeGeometry)
        m_normals.resize(totalSize);

    totalIndex = 0;

    if ((m_dataDimension == BothAscending) || (m_dataDimension == XDescending)) {
        for (int row = 0; row < rowLimit; row++)
            createSmoothNormalBodyLine(totalIndex, row * m_columns);
        createSmoothNormalUpperLine(totalIndex);
    } else { // BothDescending || ZDescending
        createSmoothNormalUpperLine(totalIndex);
        for (int row = 1; row < m_rows; row++)
            createSmoothNormalBodyLine(totalIndex, row * m_columns);
    }

    // Create indices table
    if (changeGeometry || indicesDirty)
        createSmoothIndices(0, 0, colLimit, rowLimit);

    // Create line element indices
    if (changeGeometry)
        createSmoothGridlineIndices(0, 0, colLimit, rowLimit);

    createBuffers(m_vertices, uvs, m_normals, 0);
}

void SurfaceObject::createSmoothNormalBodyLine(int &totalIndex, int column)
{
    int colLimit = m_columns - 1;

    if (m_dataDimension == BothAscending) {
        int end = colLimit + column;
        for (int j = column; j < end; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j + 1),
                                             m_vertices.at(j + m_columns));
        }
        m_normals[totalIndex++] = normal(m_vertices.at(end),
                                         m_vertices.at(end + m_columns),
                                         m_vertices.at(end - 1));
    } else if (m_dataDimension == XDescending) {
        m_normals[totalIndex++] = normal(m_vertices.at(column),
                                         m_vertices.at(column + m_columns),
                                         m_vertices.at(column + 1));
        int end = column + m_columns;
        for (int j = column + 1; j < end; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j - 1),
                                             m_vertices.at(j + m_columns));
        }
    } else if (m_dataDimension == ZDescending) {
        int end = colLimit + column;
        for (int j = column; j < end; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j + 1),
                                             m_vertices.at(j - m_columns));
        }
        m_normals[totalIndex++] = normal(m_vertices.at(end),
                                         m_vertices.at(end - m_columns),
                                         m_vertices.at(end - 1));
    } else { // BothDescending
        m_normals[totalIndex++] = normal(m_vertices.at(column),
                                         m_vertices.at(column - m_columns),
                                         m_vertices.at(column + 1));
        int end = column + m_columns;
        for (int j = column + 1; j < end; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j - 1),
                                             m_vertices.at(j - m_columns));
        }
    }
}

void SurfaceObject::createSmoothNormalUpperLine(int &totalIndex)
{
    if (m_dataDimension == BothAscending) {
        int lineEnd = m_rows * m_columns - 1;
        for (int j = (m_rows - 1) * m_columns; j < lineEnd; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j - m_columns),
                                             m_vertices.at(j + 1));
        }
        m_normals[totalIndex++] = normal(m_vertices.at(lineEnd),
                                         m_vertices.at(lineEnd - 1),
                                         m_vertices.at(lineEnd - m_columns));
    } else if (m_dataDimension == XDescending) {
        int lineStart = (m_rows - 1) * m_columns;
        int lineEnd = m_rows * m_columns;
        m_normals[totalIndex++] = normal(m_vertices.at(lineStart),
                                         m_vertices.at(lineStart + 1),
                                         m_vertices.at(lineStart - m_columns));
        for (int j = lineStart + 1; j < lineEnd; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j - m_columns),
                                             m_vertices.at(j - 1));
        }
    } else if (m_dataDimension == ZDescending) {
        int colLimit = m_columns - 1;
        for (int j = 0; j < colLimit; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j + m_columns),
                                             m_vertices.at(j + 1));
        }
        m_normals[totalIndex++] = normal(m_vertices.at(colLimit),
                                         m_vertices.at(colLimit - 1),
                                         m_vertices.at(colLimit + m_columns));
    } else { // BothDescending
        m_normals[totalIndex++] = normal(m_vertices.at(0),
                                         m_vertices.at(1),
                                         m_vertices.at(m_columns));
        for (int j = 1; j < m_columns; j++) {
            m_normals[totalIndex++] = normal(m_vertices.at(j),
                                             m_vertices.at(j + m_columns),
                                             m_vertices.at(j - 1));
        }
    }
}

QVector3D SurfaceObject::createSmoothNormalBodyLineItem(int x, int y)
{
    int p = y * m_columns + x;
    if (m_dataDimension == BothAscending) {
        if (x < m_columns - 1) {
            return normal(m_vertices.at(p), m_vertices.at(p + 1),
                          m_vertices.at(p + m_columns));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p + m_columns),
                          m_vertices.at(p - 1));
        }
    } else if (m_dataDimension == XDescending) {
        if (x == 0) {
            return normal(m_vertices.at(p), m_vertices.at(p + m_columns),
                          m_vertices.at(p + 1));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - 1),
                          m_vertices.at(p + m_columns));
        }
    } else if (m_dataDimension == ZDescending) {
        if (x < m_columns - 1) {
            return normal(m_vertices.at(p), m_vertices.at(p + 1),
                          m_vertices.at(p - m_columns));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - m_columns),
                          m_vertices.at(p - 1));
        }
    } else { // BothDescending
        if (x == 0) {
            return normal(m_vertices.at(p), m_vertices.at(p - m_columns),
                          m_vertices.at(p + 1));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - 1),
                          m_vertices.at(p - m_columns));
        }
    }
}

QVector3D SurfaceObject::createSmoothNormalUpperLineItem(int x, int y)
{
    int p = y * m_columns + x;
    if (m_dataDimension == BothAscending) {
        if (x < m_columns - 1) {
            return normal(m_vertices.at(p), m_vertices.at(p - m_columns),
                          m_vertices.at(p + 1));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - 1),
                          m_vertices.at(p - m_columns));
        }
    } else if (m_dataDimension == XDescending) {
        if (x == 0) {
            return normal(m_vertices.at(p), m_vertices.at(p + 1),
                          m_vertices.at(p - m_columns));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - m_columns),
                          m_vertices.at(p - 1));
        }
    } else if (m_dataDimension == ZDescending) {
        if (x < m_columns - 1) {
            return normal(m_vertices.at(p), m_vertices.at(p + m_columns),
                          m_vertices.at(p + 1));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p - 1),
                          m_vertices.at(p + m_columns));
        }
    } else { // BothDescending
        if (x == 0) {
            return normal(m_vertices.at(0), m_vertices.at(1),
                          m_vertices.at(m_columns));
        } else {
            return normal(m_vertices.at(p), m_vertices.at(p + m_columns),
                          m_vertices.at(p - 1));
        }
    }
}

void SurfaceObject::smoothUVs(const QSurfaceDataArray &dataArray,
                              const QSurfaceDataArray &modelArray)
{
    if (dataArray.size() == 0 || modelArray.size() == 0)
        return;

    int columns = dataArray.at(0)->size();
    int rows = dataArray.size();
    float xRangeNormalizer = dataArray.at(0)->at(columns - 1).x() - dataArray.at(0)->at(0).x();
    float zRangeNormalizer = dataArray.at(rows - 1)->at(0).z() - dataArray.at(0)->at(0).z();
    float xMin = dataArray.at(0)->at(0).x();
    float zMin = dataArray.at(0)->at(0).z();
    const bool zDescending = m_dataDimension.testFlag(SurfaceObject::ZDescending);
    const bool xDescending = m_dataDimension.testFlag(SurfaceObject::XDescending);

    QVector<QVector2D> uvs;
    uvs.resize(m_rows * m_columns);
    int index = 0;
    for (int i = 0; i < m_rows; i++) {
        float y = (modelArray.at(i)->at(0).z() - zMin) / zRangeNormalizer;
        if (zDescending)
            y = 1.0f - y;
        const QSurfaceDataRow &p = *modelArray.at(i);
        for (int j = 0; j < m_columns; j++) {
            float x = (p.at(j).x() - xMin) / xRangeNormalizer;
            if (xDescending)
                x = 1.0f - x;
            uvs[index] = QVector2D(x, y);
            index++;
        }
    }

    if (uvs.size() > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_uvTextureBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(QVector2D),
                     &uvs.at(0), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_returnTextureBuffer = true;
    }
}

void SurfaceObject::updateSmoothRow(const QSurfaceDataArray &dataArray, int rowIndex, bool polar)
{
    // Update vertices
    int p = rowIndex * m_columns;
    const QSurfaceDataRow &dataRow = *dataArray.at(rowIndex);

    for (int j = 0; j < m_columns; j++)
        getNormalizedVertex(dataRow.at(j), m_vertices[p++], polar, false);

    // Create normals
    bool upwards = (m_dataDimension == BothAscending) || (m_dataDimension == XDescending);
    int startRow = rowIndex;
    if ((startRow > 0) && upwards)
        startRow--;
    int endRow = rowIndex;
    if (!upwards && (rowIndex < m_rows - 1))
        endRow++;
    if ((endRow == m_rows - 1) && upwards)
        endRow--;
    int totalIndex = startRow * m_columns;

    if ((startRow == 0) && !upwards) {
        createSmoothNormalUpperLine(totalIndex);
        startRow++;
    }

    for (int row = startRow; row <= endRow; row++)
       createSmoothNormalBodyLine(totalIndex, row * m_columns);

    if ((rowIndex == m_rows - 1) && upwards)
        createSmoothNormalUpperLine(totalIndex);
}

void SurfaceObject::updateSmoothItem(const QSurfaceDataArray &dataArray, int row, int column,
                                     bool polar)
{
    // Update a vertice
    getNormalizedVertex(dataArray.at(row)->at(column),
                        m_vertices[row * m_columns + column], polar, false);

    // Create normals
    bool upwards = (m_dataDimension == BothAscending) || (m_dataDimension == XDescending);
    bool rightwards = (m_dataDimension == BothAscending) || (m_dataDimension == ZDescending);
    int startRow = row;
    if ((startRow > 0) && upwards)
        startRow--;
    int endRow = row;
    if (!upwards && (row < m_rows - 1))
        endRow++;
    if ((endRow == m_rows - 1) && upwards)
        endRow--;
    int startCol = column;
    if ((startCol > 0) && rightwards)
        startCol--;
    int endCol = column;
    if ((endCol < m_columns - 1) && !rightwards)
        endCol++;

    for (int i = startRow; i <= endRow; i++) {
        for (int j = startCol; j <= endCol; j++) {
            int p = i * m_columns + j;
            if ((i == 0) && !upwards)
                m_normals[p] = createSmoothNormalUpperLineItem(j, i);
            else if ((i == m_rows - 1) && upwards)
                m_normals[p] = createSmoothNormalUpperLineItem(j, i);
            else
                m_normals[p] = createSmoothNormalBodyLineItem(j, i);
         }
    }
}


void SurfaceObject::createSmoothIndices(int x, int y, int endX, int endY)
{
    if (endX >= m_columns)
        endX = m_columns - 1;
    if (endY >= m_rows)
        endY = m_rows - 1;
    if (x > endX)
        x = endX - 1;
    if (y > endY)
        y = endY - 1;

    m_indexCount = 6 * (endX - x) * (endY - y);
    GLint *indices = new GLint[m_indexCount];
    int p = 0;
    int rowEnd = endY * m_columns;
    for (int row = y * m_columns; row < rowEnd; row += m_columns) {
        for (int j = x; j < endX; j++) {
            if ((m_dataDimension == BothAscending) || (m_dataDimension == BothDescending)) {
                // Left triangle
                indices[p++] = row + j + 1;
                indices[p++] = row + m_columns + j;
                indices[p++] = row + j;

                // Right triangle
                indices[p++] = row + m_columns + j + 1;
                indices[p++] = row + m_columns + j;
                indices[p++] = row + j + 1;
            } else if (m_dataDimension == XDescending) {
                // Right triangle
                indices[p++] = row + m_columns + j;
                indices[p++] = row + m_columns + j + 1;
                indices[p++] = row + j;

                // Left triangle
                indices[p++] = row + j;
                indices[p++] = row + m_columns + j + 1;
                indices[p++] = row + j + 1;
            } else {
                // Left triangle
                indices[p++] = row + m_columns + j;
                indices[p++] = row + m_columns + j + 1;
                indices[p++] = row + j;

                // Right triangle
                indices[p++] = row + j;
                indices[p++] = row + m_columns + j + 1;
                indices[p++] = row + j + 1;

            }
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(GLint),
                 indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    delete[] indices;
}

void SurfaceObject::createSmoothGridlineIndices(int x, int y, int endX, int endY)
{
    if (endX >= m_columns)
        endX = m_columns - 1;
    if (endY >= m_rows)
        endY = m_rows - 1;
    if (x > endX)
        x = endX - 1;
    if (y > endY)
        y = endY - 1;

    int nColumns = endX - x + 1;
    int nRows = endY - y + 1;
    m_gridIndexCount = 2 * nColumns * (nRows - 1) + 2 * nRows * (nColumns - 1);
    GLint *gridIndices = new GLint[m_gridIndexCount];
    int p = 0;
    for (int i = y, row = m_columns * y; i <= endY; i++, row += m_columns) {
        for (int j = x; j < endX; j++) {
            gridIndices[p++] = row + j;
            gridIndices[p++] = row + j + 1;
        }
    }
    for (int i = y, row = m_columns * y; i < endY; i++, row += m_columns) {
        for (int j = x; j <= endX; j++) {
            gridIndices[p++] = row + j;
            gridIndices[p++] = row + j + m_columns;
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gridElementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_gridIndexCount * sizeof(GLint),
                 gridIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    delete[] gridIndices;
}

void SurfaceObject::setUpData(const QSurfaceDataArray &dataArray, const QRect &space,
                              bool changeGeometry, bool polar, bool flipXZ)
{
    m_columns = space.width();
    m_rows = space.height();
    int totalSize = m_rows * m_columns * 2;
    GLfloat uvX = 1.0f / GLfloat(m_columns - 1);
    GLfloat uvY = 1.0f / GLfloat(m_rows - 1);

    checkDirections(dataArray);
    bool indicesDirty = false;
    if (m_dataDimension != m_oldDataDimension)
        indicesDirty = true;
    m_oldDataDimension = m_dataDimension;

    m_surfaceType = SurfaceFlat;

    // Create vertix table
    if (changeGeometry)
        m_vertices.resize(totalSize);

    QVector<QVector2D> uvs;
    if (changeGeometry)
        uvs.resize(totalSize);

    int totalIndex = 0;
    int rowLimit = m_rows - 1;
    int colLimit = m_columns - 1;
    int doubleColumns = m_columns * 2 - 2;
    int rowColLimit = rowLimit * doubleColumns;

    // Init min and max to ridiculous values
    m_minY = 10000000.0;
    m_maxY = -10000000.0f;

    for (int i = 0; i < m_rows; i++) {
        const QSurfaceDataRow &row = *dataArray.at(i);
        for (int j = 0; j < m_columns; j++) {
            getNormalizedVertex(row.at(j), m_vertices[totalIndex], polar, flipXZ);
            if (changeGeometry)
                uvs[totalIndex] = QVector2D(GLfloat(j) * uvX, GLfloat(i) * uvY);

            totalIndex++;

            if (j > 0 && j < colLimit) {
                m_vertices[totalIndex] = m_vertices[totalIndex - 1];
                if (changeGeometry)
                    uvs[totalIndex] = uvs[totalIndex - 1];
                totalIndex++;
            }
        }
    }

    if (flipXZ) {
        for (int i = 0; i < m_vertices.size(); i++) {
            m_vertices[i].setX(-m_vertices.at(i).x());
            m_vertices[i].setZ(-m_vertices.at(i).z());
        }
    }

    // Create normals & indices table
    GLint *indices = 0;
    if (changeGeometry || indicesDirty) {
        int normalCount = 2 * colLimit * rowLimit;
        m_indexCount = 3 * normalCount;
        indices = new GLint[m_indexCount];
        m_normals.resize(normalCount);
    }

    int p = 0;
    totalIndex = 0;
    for (int row = 0, upperRow = doubleColumns;
         row < rowColLimit;
         row += doubleColumns, upperRow += doubleColumns) {
        for (int j = 0; j < doubleColumns; j += 2) {
            createNormals(totalIndex, row, upperRow, j);

            if (changeGeometry || indicesDirty) {
                createCoarseIndices(indices, p, row, upperRow, j);
            }
        }
    }

    // Create grid line element indices
    if (changeGeometry)
        createCoarseGridlineIndices(0, 0, colLimit, rowLimit);

    createBuffers(m_vertices, uvs, m_normals, indices);

    delete[] indices;
}

void SurfaceObject::coarseUVs(const QSurfaceDataArray &dataArray,
                              const QSurfaceDataArray &modelArray)
{
    if (dataArray.size() == 0 || modelArray.size() == 0)
        return;

    int columns = dataArray.at(0)->size();
    int rows = dataArray.size();
    float xRangeNormalizer = dataArray.at(0)->at(columns - 1).x() - dataArray.at(0)->at(0).x();
    float zRangeNormalizer = dataArray.at(rows - 1)->at(0).z() - dataArray.at(0)->at(0).z();
    float xMin = dataArray.at(0)->at(0).x();
    float zMin = dataArray.at(0)->at(0).z();
    const bool zDescending = m_dataDimension.testFlag(SurfaceObject::ZDescending);
    const bool xDescending = m_dataDimension.testFlag(SurfaceObject::XDescending);

    QVector<QVector2D> uvs;
    uvs.resize(m_rows * m_columns * 2);
    int index = 0;
    int colLimit = m_columns - 1;
    for (int i = 0; i < m_rows; i++) {
        float y = (modelArray.at(i)->at(0).z() - zMin) / zRangeNormalizer;
        if (zDescending)
            y = 1.0f - y;
        const QSurfaceDataRow &p = *modelArray.at(i);
        for (int j = 0; j < m_columns; j++) {
            float x = (p.at(j).x() - xMin) / xRangeNormalizer;
            if (xDescending)
                x = 1.0f - x;
            uvs[index] = QVector2D(x, y);
            index++;
            if (j > 0 && j < colLimit) {
                uvs[index] = uvs[index - 1];
                index++;
            }
        }
    }

    if (uvs.size() > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_uvTextureBuffer);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(QVector2D),
                     &uvs.at(0), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_returnTextureBuffer = true;
    }
}

void SurfaceObject::updateCoarseRow(const QSurfaceDataArray &dataArray, int rowIndex, bool polar)
{
    int colLimit = m_columns - 1;
    int doubleColumns = m_columns * 2 - 2;

    int p = rowIndex * doubleColumns;
    const QSurfaceDataRow &dataRow = *dataArray.at(rowIndex);

    for (int j = 0; j < m_columns; j++) {
        getNormalizedVertex(dataRow.at(j), m_vertices[p++], polar, false);
        if (j > 0 && j < colLimit) {
            m_vertices[p] = m_vertices[p - 1];
            p++;
        }
    }

    // Create normals
    p = rowIndex * doubleColumns;
    if (p > 0)
        p -= doubleColumns;
    int rowLimit = (rowIndex + 1) * doubleColumns;
    if (rowIndex == m_rows - 1)
        rowLimit = rowIndex * doubleColumns; //Topmost row, no normals
    for (int row = p, upperRow = p + doubleColumns;
         row < rowLimit;
         row += doubleColumns, upperRow += doubleColumns) {
        for (int j = 0; j < doubleColumns; j += 2)
            createNormals(p, row, upperRow, j);
    }
}

void SurfaceObject::updateCoarseItem(const QSurfaceDataArray &dataArray, int row, int column,
                                     bool polar)
{
    int colLimit = m_columns - 1;
    int doubleColumns = m_columns * 2 - 2;

    // Update a vertice
    int p = row * doubleColumns + column * 2 - (column > 0);
    getNormalizedVertex(dataArray.at(row)->at(column), m_vertices[p++], polar, false);

    if (column > 0 && column < colLimit)
        m_vertices[p] = m_vertices[p - 1];

    // Create normals
    int startRow = row;
    if (startRow > 0)
        startRow--; // Change the normal for previous row also
    int startCol = column;
    if (startCol > 0)
        startCol--;
    if (row == m_rows - 1)
        row--;
    if (column == m_columns - 1)
        column--;

    for (int i = startRow; i <= row; i++) {
        for (int j = startCol; j <= column; j++) {
            p = i * doubleColumns + j * 2;
            createNormals(p, i * doubleColumns, (i + 1) * doubleColumns, j * 2);
        }
    }
}

void SurfaceObject::createCoarseSubSection(int x, int y, int columns, int rows)
{
    if (columns > m_columns)
        columns = m_columns;
    if (rows > m_rows)
        rows = m_rows;
    if (x > columns)
        x = columns - 1;
    if (y > rows)
        y = rows - 1;

    int rowLimit = rows - 1;
    int doubleColumns = m_columns * 2 - 2;
    int doubleColumnsLimit = columns * 2 - 2;
    int rowColLimit = rowLimit * doubleColumns;
    m_indexCount = 6 * (columns - 1 - x) * (rowLimit - y);

    int p = 0;
    GLint *indices = new GLint[m_indexCount];
    for (int row = y * doubleColumns, upperRow = (y + 1) * doubleColumns;
         row < rowColLimit;
         row += doubleColumns, upperRow += doubleColumns) {
        for (int j = 2 * x; j < doubleColumnsLimit; j += 2)
            createCoarseIndices(indices, p, row, upperRow, j);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(GLint),
                 indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    delete[] indices;
}

void SurfaceObject::createCoarseGridlineIndices(int x, int y, int endX, int endY)
{
    if (endX >= m_columns)
        endX = m_columns - 1;
    if (endY >= m_rows)
        endY = m_rows - 1;
    if (x > endX)
        x = endX - 1;
    if (y > endY)
        y = endY - 1;

    int nColumns = endX - x + 1;
    int nRows = endY - y + 1;
    int doubleEndX = endX * 2;
    int doubleColumns = m_columns * 2 - 2;
    int rowColLimit = endY * doubleColumns;

    m_gridIndexCount = 2 * nColumns * (nRows - 1) + 2 * nRows * (nColumns - 1);
    GLint *gridIndices = new GLint[m_gridIndexCount];
    int p = 0;

    for (int row = y * doubleColumns; row <= rowColLimit; row += doubleColumns) {
        for (int j = x * 2; j < doubleEndX; j += 2) {
            // Horizontal line
            gridIndices[p++] = row + j;
            gridIndices[p++] = row + j + 1;

            if (row < rowColLimit) {
                // Vertical line
                gridIndices[p++] = row + j;
                gridIndices[p++] = row + j + doubleColumns;
            }
        }
    }
    // The rightmost line separately, since there isn't double vertice
    for (int i = y * doubleColumns + doubleEndX - 1; i < rowColLimit; i += doubleColumns) {
        gridIndices[p++] = i;
        gridIndices[p++] = i  + doubleColumns;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gridElementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_gridIndexCount * sizeof(GLint),
                 gridIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    delete[] gridIndices;
}

void SurfaceObject::uploadBuffers()
{
    QVector<QVector2D> uvs; // Empty dummy
    createBuffers(m_vertices, uvs, m_normals, 0);
}

void SurfaceObject::createBuffers(const QVector<QVector3D> &vertices, const QVector<QVector2D> &uvs,
                                  const QVector<QVector3D> &normals, const GLint *indices)
{
    // Move to buffers
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(QVector3D),
                 &vertices.at(0), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_normalbuffer);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(QVector3D),
                 &normals.at(0), GL_DYNAMIC_DRAW);

    if (uvs.size()) {
        glBindBuffer(GL_ARRAY_BUFFER, m_uvbuffer);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(QVector2D),
                     &uvs.at(0), GL_STATIC_DRAW);
    }

    if (indices) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementbuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(GLint),
                     indices, GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_meshDataLoaded = true;
}

void SurfaceObject::checkDirections(const QSurfaceDataArray &array)
{
    m_dataDimension = BothAscending;

    if (array.at(0)->at(0).x() > array.at(0)->at(array.at(0)->size() - 1).x())
        m_dataDimension |= XDescending;
    if (m_axisCacheX.reversed())
        m_dataDimension ^= XDescending;

    if (array.at(0)->at(0).z() > array.at(array.size() - 1)->at(0).z())
        m_dataDimension |= ZDescending;
    if (m_axisCacheZ.reversed())
        m_dataDimension ^= ZDescending;
}

void SurfaceObject::getNormalizedVertex(const QSurfaceDataItem &data, QVector3D &vertex,
                                        bool polar, bool flipXZ)
{
    float normalizedX;
    float normalizedZ;
    if (polar) {
        // Slice don't use polar, so don't care about flip
        m_renderer->calculatePolarXZ(data.position(), normalizedX, normalizedZ);
    } else {
        if (flipXZ) {
            normalizedX = m_axisCacheZ.positionAt(data.x());
            normalizedZ = m_axisCacheX.positionAt(data.z());
        } else {
            normalizedX = m_axisCacheX.positionAt(data.x());
            normalizedZ = m_axisCacheZ.positionAt(data.z());
        }
    }
    float normalizedY = m_axisCacheY.positionAt(data.y());
    m_minY = qMin(normalizedY, m_minY);
    m_maxY = qMax(normalizedY, m_maxY);
    vertex.setX(normalizedX);
    vertex.setY(normalizedY);
    vertex.setZ(normalizedZ);
}

GLuint SurfaceObject::gridElementBuf()
{
    if (!m_meshDataLoaded)
        qFatal("No loaded object");
    return m_gridElementbuffer;
}

GLuint SurfaceObject::uvBuf()
{
    if (!m_meshDataLoaded)
        qFatal("No loaded object");

    if (m_returnTextureBuffer)
        return m_uvTextureBuffer;
    else
        return m_uvbuffer;
}

GLuint SurfaceObject::gridIndexCount()
{
    return m_gridIndexCount;
}

QVector3D SurfaceObject::vertexAt(int column, int row)
{
    int pos = 0;
    if (m_surfaceType == Undefined || !m_vertices.size())
        return zeroVector;

    if (m_surfaceType == SurfaceFlat)
        pos = row * (m_columns * 2 - 2) + column * 2 - (column > 0);
    else
        pos = row * m_columns + column;
    return m_vertices.at(pos);
}

void SurfaceObject::clear()
{
    m_gridIndexCount = 0;
    m_indexCount = 0;
    m_surfaceType = Undefined;
    m_vertices.clear();
    m_normals.clear();
}

void SurfaceObject::createCoarseIndices(GLint *indices, int &p, int row, int upperRow, int j)
{
     if ((m_dataDimension == BothAscending) || (m_dataDimension == BothDescending)) {
        // Left triangle
        indices[p++] = row + j + 1;
        indices[p++] = upperRow + j;
        indices[p++] = row + j;

        // Right triangle
        indices[p++] = upperRow + j + 1;
        indices[p++] = upperRow + j;
        indices[p++] = row + j + 1;
    } else if (m_dataDimension == XDescending) {
        indices[p++] = upperRow + j;
        indices[p++] = upperRow + j + 1;
        indices[p++] = row + j;

        indices[p++] = row + j;
        indices[p++] = upperRow + j + 1;
        indices[p++] = row + j + 1;
    } else {
        // Left triangle
        indices[p++] = upperRow + j;
        indices[p++] = upperRow + j + 1;
        indices[p++] = row + j;

        // Right triangle
        indices[p++] = row + j;
        indices[p++] = upperRow + j + 1;
        indices[p++] = row + j + 1;
    }
}

void SurfaceObject::createNormals(int &p, int row, int upperRow, int j)
{
    if ((m_dataDimension == BothAscending) || (m_dataDimension == BothDescending)) {
        m_normals[p++] = normal(m_vertices.at(row + j),
                                m_vertices.at(row + j + 1),
                                m_vertices.at(upperRow + j));

        m_normals[p++] = normal(m_vertices.at(row + j + 1),
                                m_vertices.at(upperRow + j + 1),
                                m_vertices.at(upperRow + j));
    } else if (m_dataDimension == XDescending) {
        m_normals[p++] = normal(m_vertices.at(row + j),
                                m_vertices.at(upperRow + j),
                                m_vertices.at(upperRow + j + 1));

        m_normals[p++] = normal(m_vertices.at(row + j + 1),
                                m_vertices.at(row + j),
                                m_vertices.at(upperRow + j + 1));
    } else {
        m_normals[p++] = normal(m_vertices.at(row + j),
                                m_vertices.at(upperRow + j),
                                m_vertices.at(upperRow + j + 1));

        m_normals[p++] = normal(m_vertices.at(row + j + 1),
                                m_vertices.at(row + j),
                                m_vertices.at(upperRow + j + 1));
    }
}

QVector3D SurfaceObject::normal(const QVector3D &a, const QVector3D &b, const QVector3D &c)
{
    QVector3D v1 = b - a;
    QVector3D v2 = c - a;
    QVector3D normal = QVector3D::crossProduct(v1, v2);

    return normal;
}

QT_END_NAMESPACE_DATAVISUALIZATION
