/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt scene graph research project.
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

#include "qsggeometry.h"
#include "qsggeometry_p.h"
#ifndef QT_NO_OPENGL
# include <qopenglcontext.h>
# include <qopenglfunctions.h>
# include <private/qopenglextensions_p.h>
#endif

#ifdef Q_OS_QNX
#include <malloc.h>
#endif

QT_BEGIN_NAMESPACE


QSGGeometry::Attribute QSGGeometry::Attribute::create(int attributeIndex, int tupleSize, int primitiveType, bool isPrimitive)
{
    Attribute a = { attributeIndex, tupleSize, primitiveType, isPrimitive, UnknownAttribute, 0 };
    return a;
}

QSGGeometry::Attribute QSGGeometry::Attribute::createWithAttributeType(int pos, int tupleSize, int primitiveType, AttributeType attributeType)
{
    Attribute a;
    a.position = pos;
    a.tupleSize = tupleSize;
    a.type = primitiveType;
    a.isVertexCoordinate = attributeType == PositionAttribute;
    a.attributeType = attributeType;
    a.reserved = 0;
    return a;
}

/*!
    Convenience function which returns attributes to be used for 2D solid
    color drawing.
 */

const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_Point2D()
{
    static Attribute data[] = {
        Attribute::createWithAttributeType(0, 2, FloatType, PositionAttribute)
    };
    static AttributeSet attrs = { 1, sizeof(float) * 2, data };
    return attrs;
}

/*!
    Convenience function which returns attributes to be used for textured 2D drawing.
 */

const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_TexturedPoint2D()
{
    static Attribute data[] = {
        Attribute::createWithAttributeType(0, 2, FloatType, PositionAttribute),
        Attribute::createWithAttributeType(1, 2, FloatType, TexCoordAttribute)
    };
    static AttributeSet attrs = { 2, sizeof(float) * 4, data };
    return attrs;
}

/*!
    Convenience function which returns attributes to be used for per vertex colored 2D drawing.
 */

const QSGGeometry::AttributeSet &QSGGeometry::defaultAttributes_ColoredPoint2D()
{
    static Attribute data[] = {
        Attribute::createWithAttributeType(0, 2, FloatType, PositionAttribute),
        Attribute::createWithAttributeType(1, 4, UnsignedByteType, ColorAttribute)
    };
    static AttributeSet attrs = { 2, 2 * sizeof(float) + 4 * sizeof(char), data };
    return attrs;
}


/*!
    \class QSGGeometry::Attribute
    \brief The QSGGeometry::Attribute describes a single vertex attribute in a QSGGeometry
    \inmodule QtQuick

    The QSGGeometry::Attribute struct describes the attribute register position,
    the size of the attribute tuple and the attribute type.

    It also contains a hint to the renderer if this attribute is the attribute
    describing the position. The scene graph renderer may use this information
    to perform optimizations.

    It contains a number of bits which are reserved for future use.

    \sa QSGGeometry
 */

/*!
    \fn QSGGeometry::Attribute QSGGeometry::Attribute::create(int pos, int tupleSize, int primitiveType, bool isPosition)

    Creates a new QSGGeometry::Attribute for attribute register \a pos with \a
    tupleSize. The \a primitiveType can be any of the supported types from
    QSGGeometry::Type, such as QSGGeometry::FloatType or
    QSGGeometry::UnsignedByteType.

    If the attribute describes the position for the vertex, the \a isPosition
    hint should be set to \c true. The scene graph renderer may use this
    information to perform optimizations.

    \note Scene graph backends for APIs other than OpenGL may require an
    accurate description of attributes' usage, and therefore it is recommended
    to use createWithAttributeType() instead.

    Use the create function to construct the attribute, rather than an
    initialization list, to ensure that all fields are initialized.
 */

/*!
    \fn QSGGeometry::Attribute QSGGeometry::Attribute::createWithAttributeType(int pos, int tupleSize, int primitiveType, AttributeType attributeType)

    Creates a new QSGGeometry::Attribute for attribute register \a pos with \a
    tupleSize. The \a primitiveType can be any of the supported types from
    QSGGeometry::Type, such as QSGGeometry::FloatType or
    QSGGeometry::UnsignedByteType.

    \a attributeType describes the intended use of the attribute.

    Use the create function to construct the attribute, rather than an
    initialization list, to ensure that all fields are initialized.
 */


/*!
    \class QSGGeometry::AttributeSet
    \brief The QSGGeometry::AttributeSet describes how the vertices in a QSGGeometry
    are built up.
    \inmodule QtQuick

    \sa QSGGeometry
 */


/*!
   \class QSGGeometry::Point2D
   \brief The QSGGeometry::Point2D struct is a convenience struct for accessing
   2D Points.

   \inmodule QtQuick
 */


/*!
   \fn void QSGGeometry::Point2D::set(float x, float y)

   Sets the x and y values of this point to \a x and \a y.
 */


/*!
   \class QSGGeometry::ColoredPoint2D
   \brief The QSGGeometry::ColoredPoint2D struct is a convenience struct for accessing
   2D Points with a color.

   \inmodule QtQuick
 */


/*!
   \fn void QSGGeometry::ColoredPoint2D::set(float x, float y, uchar red, uchar green, uchar blue, uchar alpha)

   Sets the position of the vertex to \a x and \a y and the color to \a red, \a
   green, \a blue, and \a alpha.
 */


/*!
   \class QSGGeometry::TexturedPoint2D
   \brief The QSGGeometry::TexturedPoint2D struct is a convenience struct for accessing
   2D Points with texture coordinates.

   \inmodule QtQuick
 */


/*!
   \fn void QSGGeometry::TexturedPoint2D::set(float x, float y, float tx, float ty)

   Sets the position of the vertex to \a x and \a y and the texture coordinate
   to \a tx and \a ty.
 */



/*!
    \class QSGGeometry

    \brief The QSGGeometry class provides low-level
    storage for graphics primitives in the \l{Qt Quick Scene Graph}.

    \inmodule QtQuick

    The QSGGeometry class stores the geometry of the primitives
    rendered with the scene graph. It contains vertex data and
    optionally index data. The mode used to draw the geometry is
    specified with setDrawingMode(), which maps directly to the graphics API's
    drawing mode, such as \c GL_TRIANGLE_STRIP, \c GL_TRIANGLES, or
    \c GL_POINTS in case of OpenGL.

    Vertices can be as simple as points defined by x and y values or
    can be more complex where each vertex contains a normal, texture
    coordinates and a 3D position. The QSGGeometry::AttributeSet is
    used to describe how the vertex data is built up. The attribute
    set can only be specified on construction.  The QSGGeometry class
    provides a few convenience attributes and attribute sets by
    default. The defaultAttributes_Point2D() function returns an
    attribute set to be used in normal solid color rectangles, while
    the defaultAttributes_TexturedPoint2D function returns attributes
    to be used for textured 2D geometry. The vertex data is internally
    stored as a \c {void *} and is accessible with the vertexData()
    function. Convenience accessors for the common attribute sets are
    available with vertexDataAsPoint2D() and
    vertexDataAsTexturedPoint2D(). Vertex data is allocated by passing
    a vertex count to the constructor or by calling allocate() later.

    The QSGGeometry can optionally contain indices of either unsigned
    32-bit, unsigned 16-bit, or unsigned 8-bit integers. The index type
    must be specified during construction and cannot be changed.

    Below is a snippet illustrating how a geometry composed of
    position and color vertices can be built.

    \code
    struct MyPoint2D {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;

        void set(float x_, float y_, float r_, float g_, float b_, float a_) {
            x = x_;
            y = y_;
            r = r_;
            g = g_;
            b = b_;
            a = a_;
        }
    };

    QSGGeometry::Attribute MyPoint2D_Attributes[] = {
        QSGGeometry::Attribute::create(0, 2, GL_FLOAT, true),
        QSGGeometry::Attribute::create(1, 4, GL_FLOAT, false)
    };

    QSGGeometry::AttributeSet MyPoint2D_AttributeSet = {
        2,
        sizeof(MyPoint2D),
        MyPoint2D_Attributes
    };

    ...

    geometry = new QSGGeometry(MyPoint2D_AttributeSet, 2);
    geometry->setDrawingMode(GL_LINES);

    MyPoint2D *vertices = static_cast<MyPoint2D *>(geometry->vertexData());
    vertices[0].set(0, 0, 1, 0, 0, 1);
    vertices[1].set(width(), height(), 0, 0, 1, 1);
    \endcode

    The QSGGeometry is a software buffer and client-side in terms of
    OpenGL rendering, as the buffers used in 2D graphics typically consist of
    many small buffers that change every frame and do not benefit from
    being uploaded to graphics memory. However, the QSGGeometry
    supports hinting to the renderer that a buffer should be
    uploaded using the setVertexDataPattern() and
    setIndexDataPattern() functions. Whether this hint is respected or
    not is implementation specific.

    \sa QSGGeometryNode, {Scene Graph - Custom Geometry}

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

 */

/*!
    \fn int QSGGeometry::attributeCount() const

    Returns the number of attributes in the attrbute set used by this geometry.
 */

/*!
    \fn QSGGeometry::Attribute *QSGGeometry::attributes() const

    Returns an array with the attributes of this geometry. The size of the array
    is given with attributeCount().
 */

/*!
    \fn uint *QSGGeometry::indexDataAsUInt()

    Convenience function to access the index data as a mutable array of
    32-bit unsigned integers.
 */

/*!
    \fn const uint *QSGGeometry::indexDataAsUInt() const

    Convenience function to access the index data as an immutable array of
    32-bit unsigned integers.
 */

/*!
    \fn quint16 *QSGGeometry::indexDataAsUShort()

    Convenience function to access the index data as a mutable array of
    16-bit unsigned integers.
 */

/*!
    \fn const quint16 *QSGGeometry::indexDataAsUShort() const

    Convenience function to access the index data as an immutable array of
    16-bit unsigned integers.
 */

/*!
    \fn const QSGGeometry::ColoredPoint2D *QSGGeometry::vertexDataAsColoredPoint2D() const

    Convenience function to access the vertex data as an immutable
    array of QSGGeometry::ColoredPoint2D.
 */

/*!
    \fn QSGGeometry::ColoredPoint2D *QSGGeometry::vertexDataAsColoredPoint2D()

    Convenience function to access the vertex data as a mutable
    array of QSGGeometry::ColoredPoint2D.
 */

/*!
    \fn const QSGGeometry::TexturedPoint2D *QSGGeometry::vertexDataAsTexturedPoint2D() const

    Convenience function to access the vertex data as an immutable
    array of QSGGeometry::TexturedPoint2D.
 */

/*!
    \fn QSGGeometry::TexturedPoint2D *QSGGeometry::vertexDataAsTexturedPoint2D()

    Convenience function to access the vertex data as a mutable
    array of QSGGeometry::TexturedPoint2D.
 */

/*!
    \fn const QSGGeometry::Point2D *QSGGeometry::vertexDataAsPoint2D() const

    Convenience function to access the vertex data as an immutable
    array of QSGGeometry::Point2D.
 */

/*!
    \fn QSGGeometry::Point2D *QSGGeometry::vertexDataAsPoint2D()

    Convenience function to access the vertex data as a mutable
    array of QSGGeometry::Point2D.
 */


/*!
    Constructs a geometry object based on \a attributes.

    The object allocate space for \a vertexCount vertices based on the
    accumulated size in \a attributes and for \a indexCount.

    The \a indexType maps to the OpenGL index type and can be
    \c GL_UNSIGNED_SHORT and \c GL_UNSIGNED_BYTE. On OpenGL implementations that
    support it, such as desktop OpenGL, \c GL_UNSIGNED_INT can also be used.

    Geometry objects are constructed with \c GL_TRIANGLE_STRIP as default
    drawing mode.

    The attribute structure is assumed to be POD and the geometry object
    assumes this will not go away. There is no memory management involved.
 */

QSGGeometry::QSGGeometry(const QSGGeometry::AttributeSet &attributes,
                         int vertexCount,
                         int indexCount,
                         int indexType)
    : m_drawing_mode(DrawTriangleStrip)
    , m_vertex_count(0)
    , m_index_count(0)
    , m_index_type(indexType)
    , m_attributes(attributes)
    , m_data(0)
    , m_index_data_offset(-1)
    , m_server_data(0)
    , m_owns_data(false)
    , m_index_usage_pattern(AlwaysUploadPattern)
    , m_vertex_usage_pattern(AlwaysUploadPattern)
    , m_line_width(1.0)
{
    Q_UNUSED(m_reserved_bits);
    Q_ASSERT(m_attributes.count > 0);
    Q_ASSERT(m_attributes.stride > 0);
#ifndef QT_NO_OPENGL
    Q_ASSERT_X(indexType != GL_UNSIGNED_INT
               || static_cast<QOpenGLExtensions *>(QOpenGLContext::currentContext()->functions())
                  ->hasOpenGLExtension(QOpenGLExtensions::ElementIndexUint),
               "QSGGeometry::QSGGeometry",
               "GL_UNSIGNED_INT is not supported, geometry will not render"
               );
#endif
    if (indexType != UnsignedByteType
        && indexType != UnsignedShortType
        && indexType != UnsignedIntType) {
        qFatal("QSGGeometry: Unsupported index type, %x.\n", indexType);
    }

    // Because allocate reads m_vertex_count, m_index_count and m_owns_data, these
    // need to be set before calling allocate...
    allocate(vertexCount, indexCount);
}

/*!
    \fn int QSGGeometry::sizeOfVertex() const

    Returns the size in bytes of one vertex.

    This value comes from the attributes.
 */

/*!
    \fn int QSGGeometry::sizeOfIndex() const

    Returns the byte size of the index type.

    This value is either \c 1 when index type is \c GL_UNSIGNED_BYTE or \c 2
    when index type is \c GL_UNSIGNED_SHORT. For Desktop OpenGL,
    \c GL_UNSIGNED_INT with the value \c 4 is also supported.
 */

/*!
    Destroys the geometry object and the vertex and index data it has allocated.
 */

QSGGeometry::~QSGGeometry()
{
    if (m_owns_data)
        free(m_data);

    if (m_server_data)
        delete m_server_data;
}

/*!
    \fn int QSGGeometry::vertexCount() const

    Returns the number of vertices in this geometry object.
 */

/*!
    \fn int QSGGeometry::indexCount() const

    Returns the number of indices in this geometry object.
 */



/*!
    \fn void *QSGGeometry::vertexData()

    Returns a pointer to the raw vertex data of this geometry object.

    \sa vertexDataAsPoint2D(), vertexDataAsTexturedPoint2D()
 */

/*!
    \fn const void *QSGGeometry::vertexData() const

    Returns a pointer to the raw vertex data of this geometry object.

    \sa vertexDataAsPoint2D(), vertexDataAsTexturedPoint2D()
 */

/*!
    Returns a pointer to the raw index data of this geometry object.

    \sa indexDataAsUShort(), indexDataAsUInt()
 */
void *QSGGeometry::indexData()
{
    return m_index_data_offset < 0
            ? 0
            : ((char *) m_data + m_index_data_offset);
}

/*!
    Returns a pointer to the raw index data of this geometry object.

    \sa indexDataAsUShort(), indexDataAsUInt()
 */
const void *QSGGeometry::indexData() const
{
    return m_index_data_offset < 0
            ? 0
            : ((char *) m_data + m_index_data_offset);
}

/*!
    \enum QSGGeometry::DrawingMode

    The values correspond to OpenGL enum values like \c GL_POINTS, \c GL_LINES,
    etc. QSGGeometry provies its own type in order to be able to provide the
    same API with non-OpenGL backends as well.

    \value DrawPoints
    \value DrawLines
    \value DrawLineLoop
    \value DrawLineStrip
    \value DrawTriangles
    \value DrawTriangleStrip
    \value DrawTriangleFan
 */

/*!
    \enum QSGGeometry::Type

    The values correspond to OpenGL type constants like \c GL_BYTE, \c
    GL_UNSIGNED_BYTE, etc. QSGGeometry provies its own type in order to be able
    to provide the same API with non-OpenGL backends as well.

    \value ByteType
    \value UnsignedByteType
    \value ShortType
    \value UnsignedShortType
    \value IntType
    \value UnsignedIntType
    \value FloatType
 */

/*!
    Sets the \a mode to be used for drawing this geometry.

    The default value is QSGGeometry::DrawTriangleStrip.

    \sa DrawingMode
 */
void QSGGeometry::setDrawingMode(unsigned int mode)
{
    m_drawing_mode = mode;
}

/*!
    Gets the current line or point width or to be used for this geometry. This
    property only applies to line width when the drawingMode is DrawLines,
    DarwLineStrip, or DrawLineLoop. For desktop OpenGL, it also applies to
    point size when the drawingMode is DrawPoints.

    The default value is \c 1.0

    \note When not using OpenGL, support for point and line drawing may be
    limited. For example, some APIs do not support point sprites and so setting
    a size other than 1 is not possible. Some backends may be able implement
    support via geometry shaders, but this is not guaranteed to be always
    available.

    \sa setLineWidth(), drawingMode()
*/
float QSGGeometry::lineWidth() const
{
    return m_line_width;
}

/*!
    Sets the line or point width to be used for this geometry to \a width. This
    property only applies to line width when the drawingMode is DrawLines,
    DrawLineStrip, or DrawLineLoop. For Desktop OpenGL, it also applies to
    point size when the drawingMode is DrawPoints.

    \note How line width and point size are treated is implementation
    dependent: The application should not rely on these, but rather create
    triangles or similar to draw areas. On OpenGL ES, line width support is
    limited and point size is unsupported.

    \sa lineWidth(), drawingMode()
*/
void QSGGeometry::setLineWidth(float width)
{
    m_line_width = width;
}

/*!
    \fn int QSGGeometry::drawingMode() const

    Returns the drawing mode of this geometry.

    The default value is \c GL_TRIANGLE_STRIP.
 */

/*!
    \fn int QSGGeometry::indexType() const

    Returns the primitive type used for indices in this
    geometry object.
 */


/*!
    Resizes the vertex and index data of this geometry object to fit \a vertexCount
    vertices and \a indexCount indices.

    Vertex and index data will be invalidated after this call and the caller must
    mark the associated geometry node as dirty, by calling
    node->markDirty(QSGNode::DirtyGeometry) to ensure that the renderer has
    a chance to update internal buffers.
 */
void QSGGeometry::allocate(int vertexCount, int indexCount)
{
    if (vertexCount == m_vertex_count && indexCount == m_index_count)
        return;

    m_vertex_count = vertexCount;
    m_index_count = indexCount;

    bool canUsePrealloc = m_index_count <= 0;
    int vertexByteSize = m_attributes.stride * m_vertex_count;

    if (m_owns_data)
        free(m_data);

    if (canUsePrealloc && vertexByteSize <= (int) sizeof(m_prealloc)) {
        m_data = (void *) &m_prealloc[0];
        m_index_data_offset = -1;
        m_owns_data = false;
    } else {
        Q_ASSERT(m_index_type == UnsignedIntType || m_index_type == UnsignedShortType);
        int indexByteSize = indexCount * (m_index_type == UnsignedShortType ? sizeof(quint16) : sizeof(quint32));
        m_data = (void *) malloc(vertexByteSize + indexByteSize);
        m_index_data_offset = vertexByteSize;
        m_owns_data = true;
    }

    // If we have associated vbo data we could potentially crash later if
    // the old buffers are used with the new vertex and index count, so we force
    // an update in the renderer in that case. This is really the users responsibility
    // but it is cheap for us to enforce this, so why not...
    if (m_server_data) {
        markIndexDataDirty();
        markVertexDataDirty();
    }

}

/*!
    Updates the geometry \a g with the coordinates in \a rect.

    The function assumes the geometry object contains a single triangle strip
    of QSGGeometry::Point2D vertices
 */
void QSGGeometry::updateRectGeometry(QSGGeometry *g, const QRectF &rect)
{
    Point2D *v = g->vertexDataAsPoint2D();
    v[0].x = rect.left();
    v[0].y = rect.top();

    v[1].x = rect.left();
    v[1].y = rect.bottom();

    v[2].x = rect.right();
    v[2].y = rect.top();

    v[3].x = rect.right();
    v[3].y = rect.bottom();
}

/*!
    Updates the geometry \a g with the coordinates in \a rect and texture
    coordinates from \a textureRect.

    \a textureRect should be in normalized coordinates.

    \a g is assumed to be a triangle strip of four vertices of type
    QSGGeometry::TexturedPoint2D.
 */
void QSGGeometry::updateTexturedRectGeometry(QSGGeometry *g, const QRectF &rect, const QRectF &textureRect)
{
    TexturedPoint2D *v = g->vertexDataAsTexturedPoint2D();
    v[0].x = rect.left();
    v[0].y = rect.top();
    v[0].tx = textureRect.left();
    v[0].ty = textureRect.top();

    v[1].x = rect.left();
    v[1].y = rect.bottom();
    v[1].tx = textureRect.left();
    v[1].ty = textureRect.bottom();

    v[2].x = rect.right();
    v[2].y = rect.top();
    v[2].tx = textureRect.right();
    v[2].ty = textureRect.top();

    v[3].x = rect.right();
    v[3].y = rect.bottom();
    v[3].tx = textureRect.right();
    v[3].ty = textureRect.bottom();
}

/*!
    Updates the geometry \a g with the coordinates in \a rect.

    The function assumes the geometry object contains a single triangle strip
    of QSGGeometry::ColoredPoint2D vertices
 */
void QSGGeometry::updateColoredRectGeometry(QSGGeometry *g, const QRectF &rect)
{
    ColoredPoint2D *v = g->vertexDataAsColoredPoint2D();
    v[0].x = rect.left();
    v[0].y = rect.top();

    v[1].x = rect.left();
    v[1].y = rect.bottom();

    v[2].x = rect.right();
    v[2].y = rect.top();

    v[3].x = rect.right();
    v[3].y = rect.bottom();
}


/*!
    \enum QSGGeometry::DataPattern

    The DataPattern enum is used to specify the use pattern for the
    vertex and index data in a geometry object.

    \value AlwaysUploadPattern The data is always uploaded. This means
    that the user does not need to explicitly mark index and vertex
    data as dirty after changing it. This is the default.

    \value DynamicPattern The data is modified repeatedly and drawn
    many times.  This is a hint that may provide better
    performance. When set the user must make sure to mark the data as
    dirty after changing it.

    \value StaticPattern The data is modified once and drawn many
    times. This is a hint that may provide better performance. When
    set the user must make sure to mark the data as dirty after
    changing it.

    \value StreamPattern The data is modified for almost every time it
    is drawn. This is a hint that may provide better performance. When
    set, the user must make sure to mark the data as dirty after
    changing it.
 */


/*!
    \fn QSGGeometry::DataPattern QSGGeometry::indexDataPattern() const

    Returns the usage pattern for indices in this geometry. The default
    pattern is AlwaysUploadPattern.
 */

/*!
    Sets the usage pattern for indices to \a p.

    The default is AlwaysUploadPattern. When set to anything other than
    the default, the user must call markIndexDataDirty() after changing
    the index data, in addition to calling QSGNode::markDirty() with
    QSGNode::DirtyGeometry.
 */

void QSGGeometry::setIndexDataPattern(DataPattern p)
{
    m_index_usage_pattern = p;
}




/*!
    \fn QSGGeometry::DataPattern QSGGeometry::vertexDataPattern() const

    Returns the usage pattern for vertices in this geometry. The default
    pattern is AlwaysUploadPattern.
 */

/*!
    Sets the usage pattern for vertices to \a p.

    The default is AlwaysUploadPattern. When set to anything other than
    the default, the user must call markVertexDataDirty() after changing
    the vertex data, in addition to calling QSGNode::markDirty() with
    QSGNode::DirtyGeometry.
 */

void QSGGeometry::setVertexDataPattern(DataPattern p)
{
    m_vertex_usage_pattern = p;
}




/*!
    Mark that the vertices in this geometry has changed and must be uploaded
    again.

    This function only has an effect when the usage pattern for vertices is
    StaticData and the renderer that renders this geometry uploads the geometry
    into Vertex Buffer Objects (VBOs).
 */
void QSGGeometry::markIndexDataDirty()
{
    m_dirty_index_data = true;
}



/*!
    Mark that the vertices in this geometry has changed and must be uploaded
    again.

    This function only has an effect when the usage pattern for vertices is
    StaticData and the renderer that renders this geometry uploads the geometry
    into Vertex Buffer Objects (VBOs).
 */
void QSGGeometry::markVertexDataDirty()
{
    m_dirty_vertex_data = true;
}


QT_END_NAMESPACE
