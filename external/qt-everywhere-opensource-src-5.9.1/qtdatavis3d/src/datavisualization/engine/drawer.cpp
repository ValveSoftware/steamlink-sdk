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

#include "drawer_p.h"
#include "shaderhelper_p.h"
#include "surfaceobject_p.h"
#include "utils_p.h"
#include "texturehelper_p.h"
#include "abstract3drenderer_p.h"
#include "scatterpointbufferhelper_p.h"

#include <QtGui/QMatrix4x4>
#include <QtCore/qmath.h>

// Resources need to be explicitly initialized when building as static library
class StaticLibInitializer
{
public:
    StaticLibInitializer()
    {
        Q_INIT_RESOURCE(engine);
    }
};
StaticLibInitializer staticLibInitializer;

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

// Vertex array buffer for point
const GLfloat point_data[] = {0.0f, 0.0f, 0.0f};

// Vertex array buffer for line
const GLfloat line_data[] = {
    -1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
};

Drawer::Drawer(Q3DTheme *theme)
    : m_theme(theme),
      m_textureHelper(0),
      m_pointbuffer(0),
      m_linebuffer(0),
      m_scaledFontSize(0.0f)
{
}

Drawer::~Drawer()
{
    delete m_textureHelper;
    if (QOpenGLContext::currentContext()) {
        glDeleteBuffers(1, &m_pointbuffer);
        glDeleteBuffers(1, &m_linebuffer);
    }
}

void Drawer::initializeOpenGL()
{
    initializeOpenGLFunctions();
    if (!m_textureHelper)
        m_textureHelper = new TextureHelper();
}

void Drawer::setTheme(Q3DTheme *theme)
{
    m_theme = theme;
    m_scaledFontSize = 0.05f + m_theme->font().pointSizeF() / 500.0f;
    emit drawerChanged();
}

Q3DTheme *Drawer::theme() const
{
    return m_theme;
}

QFont Drawer::font() const
{
    return m_theme->font();
}

void Drawer::drawObject(ShaderHelper *shader, AbstractObjectHelper *object, GLuint textureId,
                        GLuint depthTextureId, GLuint textureId3D)
{
#if defined(QT_OPENGL_ES_2)
    Q_UNUSED(textureId3D)
#endif
    if (textureId) {
        // Activate texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        shader->setUniformValue(shader->texture(), 0);
    }

    if (depthTextureId) {
        // Activate depth texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTextureId);
        shader->setUniformValue(shader->shadow(), 1);
    }
#if !defined(QT_OPENGL_ES_2)
    if (textureId3D) {
        // Activate texture
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, textureId3D);
        shader->setUniformValue(shader->texture(), 2);
    }
#endif

    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, object->vertexBuf());
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // 2nd attribute buffer : normals
    if (shader->normalAtt() >= 0) {
        glEnableVertexAttribArray(shader->normalAtt());
        glBindBuffer(GL_ARRAY_BUFFER, object->normalBuf());
        glVertexAttribPointer(shader->normalAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    // 3rd attribute buffer : UVs
    if (shader->uvAtt() >= 0) {
        glEnableVertexAttribArray(shader->uvAtt());
        glBindBuffer(GL_ARRAY_BUFFER, object->uvBuf());
        glVertexAttribPointer(shader->uvAtt(), 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->elementBuf());

    // Draw the triangles
    glDrawElements(GL_TRIANGLES, object->indexCount(), GL_UNSIGNED_INT, (void*)0);

    // Free buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (shader->uvAtt() >= 0)
        glDisableVertexAttribArray(shader->uvAtt());
    if (shader->normalAtt() >= 0)
        glDisableVertexAttribArray(shader->normalAtt());
    glDisableVertexAttribArray(shader->posAtt());

    // Release textures
#if !defined(QT_OPENGL_ES_2)
    if (textureId3D) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, 0);
    }
#endif
    if (depthTextureId) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (textureId) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Drawer::drawSelectionObject(ShaderHelper *shader, AbstractObjectHelper *object)
{
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, object->vertexBuf());
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->elementBuf());
    glDrawElements(GL_TRIANGLES, object->indexCount(), GL_UNSIGNED_INT, (void *)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(shader->posAtt());
}

void Drawer::drawSurfaceGrid(ShaderHelper *shader, SurfaceObject *object)
{
    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, object->vertexBuf());
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->gridElementBuf());

    // Draw the lines
    glDrawElements(GL_LINES, object->gridIndexCount(), GL_UNSIGNED_INT, (void*)0);

    // Free buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(shader->posAtt());
}

void Drawer::drawPoint(ShaderHelper *shader)
{
    // Draw a single point

    // Generate vertex buffer for point if it does not exist
    if (!m_pointbuffer) {
        glGenBuffers(1, &m_pointbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_pointbuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(point_data), point_data, GL_STATIC_DRAW);
    }

    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, m_pointbuffer);
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw the point
    glDrawArrays(GL_POINTS, 0, 1);

    // Free buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(shader->posAtt());
}

void Drawer::drawPoints(ShaderHelper *shader, ScatterPointBufferHelper *object, GLuint textureId)
{
    if (textureId) {
        // Activate texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        shader->setUniformValue(shader->texture(), 0);
    }

    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, object->pointBuf());
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // 2nd attribute buffer : UVs
    if (textureId) {
        glEnableVertexAttribArray(shader->uvAtt());
        glBindBuffer(GL_ARRAY_BUFFER, object->uvBuf());
        glVertexAttribPointer(shader->uvAtt(), 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    // Draw the points
    glDrawArrays(GL_POINTS, 0, object->indexCount());

    // Free buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(shader->posAtt());

    if (textureId) {
        glDisableVertexAttribArray(shader->uvAtt());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Drawer::drawLine(ShaderHelper *shader)
{
    // Draw a single line

    // Generate vertex buffer for line if it does not exist
    if (!m_linebuffer) {
        glGenBuffers(1, &m_linebuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_linebuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_data), line_data, GL_STATIC_DRAW);
    }

    // 1st attribute buffer : vertices
    glEnableVertexAttribArray(shader->posAtt());
    glBindBuffer(GL_ARRAY_BUFFER, m_linebuffer);
    glVertexAttribPointer(shader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw the line
    glDrawArrays(GL_LINES, 0, 2);

    // Free buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(shader->posAtt());
}

void Drawer::drawLabel(const AbstractRenderItem &item, const LabelItem &labelItem,
                       const QMatrix4x4 &viewmatrix, const QMatrix4x4 &projectionmatrix,
                       const QVector3D &positionComp, const QQuaternion &rotation,
                       GLfloat itemHeight, QAbstract3DGraph::SelectionFlags mode,
                       ShaderHelper *shader, ObjectHelper *object,
                       const Q3DCamera *camera, bool useDepth, bool rotateAlong,
                       LabelPosition position, Qt::AlignmentFlag alignment, bool isSlicing,
                       bool isSelecting)
{
    // Draw label
    if (!labelItem.textureId())
        return; // No texture, skip

    QSize textureSize = labelItem.size();
    QMatrix4x4 modelMatrix;
    QMatrix4x4 MVPMatrix;
    GLfloat xPosition = 0.0f;
    GLfloat yPosition = 0.0f;
    GLfloat zPosition = positionComp.z();

    switch (position) {
    case LabelBelow: {
        yPosition = item.translation().y() - (positionComp.y() / 2.0f) + itemHeight - 0.1f;
        break;
    }
    case LabelLow: {
        yPosition = -positionComp.y();
        break;
    }
    case LabelMid: {
        yPosition = item.translation().y();
        break;
    }
    case LabelHigh: {
        yPosition = item.translation().y() + itemHeight / 2.0f;
        break;
    }
    case LabelOver: {
        yPosition = item.translation().y() - (positionComp.y() / 2.0f) + itemHeight + 0.1f;
        break;
    }
    case LabelBottom: {
        yPosition = -2.75f + positionComp.y();
        xPosition = 0.0f;
        break;
    }
    case LabelTop: {
        yPosition = 2.75f - positionComp.y();
        xPosition = 0.0f;
        break;
    }
    case LabelLeft: {
        yPosition = 0.0f;
        xPosition = -2.75f;
        break;
    }
    case LabelRight: {
        yPosition = 0.0f;
        xPosition = 2.75f;
        break;
    }
    }

    // Calculate scale factor to get uniform font size
    GLfloat scaleFactor = m_scaledFontSize / (GLfloat)textureSize.height();

    // Apply alignment
    QVector3D anchorPoint;

    if (alignment & Qt::AlignLeft)
        anchorPoint.setX(float(textureSize.width()) * scaleFactor);
    else if (alignment & Qt::AlignRight)
        anchorPoint.setX(float(-textureSize.width()) * scaleFactor);

    if (alignment & Qt::AlignTop)
        anchorPoint.setY(float(-textureSize.height()) * scaleFactor);
    else if (alignment & Qt::AlignBottom)
        anchorPoint.setY(float(textureSize.height()) * scaleFactor);

    if (position < LabelBottom) {
        xPosition = item.translation().x();
        if (useDepth)
            zPosition = item.translation().z();
        else if (mode.testFlag(QAbstract3DGraph::SelectionColumn) && isSlicing)
            xPosition = -(item.translation().z()) + positionComp.z(); // flip first to left
    }

    // Position label
    modelMatrix.translate(xPosition, yPosition, zPosition);

    // Rotate
    if (useDepth && !rotateAlong) {
        float yComp = float(qRadiansToDegrees(qTan(positionComp.y() / cameraDistance)));
        // Apply negative camera rotations to keep labels facing camera
        float camRotationX = camera->xRotation();
        float camRotationY = camera->yRotation();
        modelMatrix.rotate(-camRotationX, 0.0f, 1.0f, 0.0f);
        modelMatrix.rotate(-camRotationY - yComp, 1.0f, 0.0f, 0.0f);
    } else {
        modelMatrix.rotate(rotation);
    }
    modelMatrix.translate(anchorPoint);

    // Scale label based on text size
    modelMatrix.scale(QVector3D((GLfloat)textureSize.width() * scaleFactor,
                                m_scaledFontSize,
                                0.0f));

    MVPMatrix = projectionmatrix * viewmatrix * modelMatrix;

    shader->setUniformValue(shader->MVP(), MVPMatrix);

    if (isSelecting) {
        // Draw the selection object
        drawSelectionObject(shader, object);
    } else {
        // Draw the object
        drawObject(shader, object, labelItem.textureId());
    }
}

void Drawer::generateSelectionLabelTexture(Abstract3DRenderer *renderer)
{
    LabelItem &labelItem = renderer->selectionLabelItem();
    generateLabelItem(labelItem, renderer->selectionLabel());
}

void Drawer::generateLabelItem(LabelItem &item, const QString &text, int widestLabel)
{
    initializeOpenGL();

    item.clear();

    if (!text.isEmpty()) {
        // Create labels
        // Print label into a QImage using QPainter
        QImage label = Utils::printTextToImage(m_theme->font(),
                                               text,
                                               m_theme->labelBackgroundColor(),
                                               m_theme->labelTextColor(),
                                               m_theme->isLabelBackgroundEnabled(),
                                               m_theme->isLabelBorderEnabled(),
                                               widestLabel);

        // Set label size
        item.setSize(label.size());
        // Insert text texture into label (also deletes the old texture)
        item.setTextureId(m_textureHelper->create2DTexture(label, true, true));
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
