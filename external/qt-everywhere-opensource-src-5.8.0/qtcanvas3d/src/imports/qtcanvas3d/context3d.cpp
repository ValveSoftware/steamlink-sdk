/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "canvasglstatedump_p.h"
#include "activeinfo3d_p.h"
#include "canvas3d_p.h"
#include "context3d_p.h"
#include "texture3d_p.h"
#include "shader3d_p.h"
#include "program3d_p.h"
#include "buffer3d_p.h"
#include "framebuffer3d_p.h"
#include "renderbuffer3d_p.h"
#include "uniformlocation_p.h"
#include "teximage3d_p.h"
#include "arrayutils_p.h"
#include "shaderprecisionformat_p.h"
#include "enumtostringmap_p.h"
#include "canvas3dcommon_p.h"
#include "renderjob_p.h"
#include "compressedtextures3tc_p.h"
#include "compressedtexturepvrtc_p.h"

#include <QtGui/QOpenGLShader>
#include <QtGui/QOpenGLContext>
#include <QtQml/private/qv4typedarray_p.h>
#include <QtQml/private/qv4arraybuffer_p.h>
#include <QtQml/private/qjsvalue_p.h>
#include <QtCore/private/qbytedata_p.h>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

const int maxUniformAttributeNameLen = 512;

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

/*!
 * \qmltype Context3D
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Provides the 3D rendering API and context.
 *
 * An uncreatable QML type that provides a WebGL-like API that can be used to draw 3D graphics to
 * the Canvas3D element. You can get it by calling the \l{Canvas3D::getContext}{Canvas3D.getContext}
 * method.
 *
 * \sa Canvas3D
 */
CanvasContext::CanvasContext(QQmlEngine *engine, bool isES2, int maxVertexAttribs,
                             int contextVersion, const QSet<QByteArray> &extensions,
                             CanvasGlCommandQueue *commandQueue,
                             bool isCombinedDepthStencilSupported, QObject *parent) :
    CanvasAbstractObject(0, parent),
    m_engine(engine),
    m_v4engine(QQmlEnginePrivate::getV4Engine(engine)),
    m_unpackFlipYEnabled(false),
    m_unpackPremultiplyAlphaEnabled(false),
    m_unpackAlignmentValue(4),
    m_devicePixelRatio(1.0),
    m_currentProgram(0),
    m_currentArrayBuffer(0),
    m_currentElementArrayBuffer(0),
    m_currentTexture2D(0),
    m_currentTextureCubeMap(0),
    m_currentFramebuffer(0),
    m_currentRenderbuffer(0),
    m_extensions(extensions),
    m_error(CANVAS_NO_ERRORS),
    m_map(EnumToStringMap::newInstance()),
    m_canvas(0),
    m_maxVertexAttribs(maxVertexAttribs),
    m_contextVersion(contextVersion),
    m_isOpenGLES2(isES2),
    m_isCombinedDepthStencilSupported(isCombinedDepthStencilSupported),
    m_commandQueue(0),
    m_contextLost(false),
    m_contextLostErrorReported(false),
    m_stateDumpExt(0),
    m_textureProviderExt(0),
    m_standardDerivatives(0),
    m_compressedTextureS3TC(0),
    m_compressedTexturePVRTC(0)
{
    setCommandQueue(commandQueue);
}

CanvasContext::~CanvasContext()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;
    EnumToStringMap::deleteInstance();

    // Cleanup quick item textures to avoid crash when parent gets deleted before children
    const QList<CanvasTexture *> quickItemTextures = m_quickItemToTextureMap.values();
    for (CanvasTexture *texture : quickItemTextures)
        texture->del();
}

void CanvasContext::setCanvas(Canvas *canvas)
{
    if (m_canvas != canvas) {
        if (m_canvas) {
            disconnect(m_canvas, &QQuickItem::widthChanged, this, 0);
            disconnect(m_canvas, &QQuickItem::heightChanged, this, 0);
        }

        m_canvas = canvas;
        emit canvasChanged(canvas);

        connect(m_canvas, &QQuickItem::widthChanged,
                this, &CanvasContext::drawingBufferWidthChanged);
        connect(m_canvas, &QQuickItem::heightChanged,
                this, &CanvasContext::drawingBufferHeightChanged);
    }
}

/*!
 * \qmlproperty Canvas3D Context3D::canvas
 * Holds the read only reference to the Canvas3D for this Context3D.
 */
Canvas *CanvasContext::canvas()
{
    return m_canvas;
}

/*!
 * \qmlproperty int Context3D::drawingBufferWidth
 * Holds the current read-only logical pixel width of the drawing buffer.
 * To get the width in physical pixels you need to multiply this with the \c devicePixelRatio.
 */
uint CanvasContext::drawingBufferWidth()
{
    uint width = 0;
    if (m_canvas)
        width = m_canvas->pixelSize().width() / m_devicePixelRatio;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): " << width;
    return width;
}

/*!
 * \qmlproperty int Context3D::drawingBufferHeight
 * Holds the current read-only logical pixel height of the drawing buffer.
 * To get the height in physical pixels you need to multiply this with the \c devicePixelRatio.
 */
uint CanvasContext::drawingBufferHeight()
{
    uint height = 0;
    if (m_canvas)
        height = m_canvas->pixelSize().height() / m_devicePixelRatio;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): " << height;
    return height;
}

QString CanvasContext::glEnumToString(glEnums value) const
{
    return m_map->lookUp(value);
}

void CanvasContext::setContextAttributes(const CanvasContextAttributes &attribs)
{
    m_contextAttributes.setFrom(attribs);
}

float CanvasContext::devicePixelRatio()
{
    return m_devicePixelRatio;
}

void CanvasContext::setDevicePixelRatio(float ratio)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "(" << ratio << ")";
    m_devicePixelRatio = ratio;
}

/*!
 * \qmlmethod Canvas3DShaderPrecisionFormat Context3D::getShaderPrecisionFormat(glEnums shadertype, glEnums precisiontype)
 * Return a new Canvas3DShaderPrecisionFormat describing the range and precision for the specified shader
 * numeric format.
 * \a shadertype Type of the shader, either \c Context3D.FRAGMENT_SHADER or
 * \c{Context3D.VERTEX_SHADER}.
 * \a precisiontype Can be \c{Context3D.LOW_FLOAT}, \c{Context3D.MEDIUM_FLOAT},
 * \c{Context3D.HIGH_FLOAT}, \c{Context3D.LOW_INT}, \c{Context3D.MEDIUM_INT} or
 * \c{Context3D.HIGH_INT}.
 * This command is handled synchronously.
 *
 * \sa Canvas3DShaderPrecisionFormat
 */
QJSValue CanvasContext::getShaderPrecisionFormat(glEnums shadertype,
                                                 glEnums precisiontype)
{
    QString str = QString(__FUNCTION__);
    str += QStringLiteral("(shaderType:")
            + glEnumToString(shadertype)
            + QStringLiteral(", precisionType:")
            + glEnumToString(precisiontype)
            + QStringLiteral(")");

    qCDebug(canvas3drendering).nospace() << "Context3D::" << str;

    GLint retval[3];
    // Default values from OpenGL ES2 spec
    switch (precisiontype) {
    case LOW_INT:
    case MEDIUM_INT:
    case HIGH_INT:
        // 32-bit twos-complement integer format
        retval[0] = 31;
        retval[1] = 30;
        retval[2] = 0;
        break;
    case LOW_FLOAT:
    case MEDIUM_FLOAT:
    case HIGH_FLOAT:
        // IEEE single-precision floating-point format
        retval[0] = 127;
        retval[1] = 127;
        retval[2] = 23;
        break;
    default:
        retval[0] = 1;
        retval[1] = 1;
        retval[2] = 1;
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }

    if (!checkContextLost()) {
        // On desktop envs glGetShaderPrecisionFormat is part of OpenGL 4.x, so it is not necessarily
        // available. Let's just return the default values if not ES2.
        if (m_isOpenGLES2) {
            GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetShaderPrecisionFormat,
                                      GLint(shadertype), GLint(precisiontype));
            syncCommand.returnValue = retval;
            scheduleSyncCommand(&syncCommand);
        }
    }

    CanvasShaderPrecisionFormat *format = new CanvasShaderPrecisionFormat();
    format->setRangeMin(int(retval[0]));
    format->setRangeMax(int(retval[1]));
    format->setPrecision(int(retval[2]));
    return m_engine->newQObject(format);
}

/*!
 * \qmlmethod bool Context3D::isContextLost()
 * Returns \c{true} if the context is currently lost.
 *
 * \sa {Canvas3D::contextLost}{Canvas3D.contextLost}
 */
bool CanvasContext::isContextLost()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): " << m_contextLost;
    return m_contextLost;
}

/*!
 * \qmlmethod Canvas3DContextAttributes Context3D::getContextAttributes()
 * Returns a copy of the actual context parameters that are used in the current context.
 */
QJSValue CanvasContext::getContextAttributes()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "()";

    if (checkContextLost()) {
        return QJSValue(QJSValue::NullValue);
    } else {
        CanvasContextAttributes *attributes = new CanvasContextAttributes();
        attributes->setAlpha(m_contextAttributes.alpha());
        attributes->setDepth(m_contextAttributes.depth());
        attributes->setStencil(m_contextAttributes.stencil());
        attributes->setAntialias(m_contextAttributes.antialias());
        attributes->setPremultipliedAlpha(m_contextAttributes.premultipliedAlpha());
        attributes->setPreserveDrawingBuffer(m_contextAttributes.preserveDrawingBuffer());
        attributes->setPreferLowPowerToHighPerformance(
                    m_contextAttributes.preferLowPowerToHighPerformance());
        attributes->setFailIfMajorPerformanceCaveat(
                    m_contextAttributes.failIfMajorPerformanceCaveat());

        return m_engine->newQObject(attributes);
    }
}

/*!
 * \qmlmethod void Context3D::flush()
 * Indicates to graphics driver that previously sent commands must complete within finite time.
 */
void CanvasContext::flush()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "()";

    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glFlush);
}

/*!
 * \qmlmethod void Context3D::finish()
 * Forces all previous 3D rendering commands to complete.
 * This command is handled synchronously.
 */
void CanvasContext::finish()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "()";

    if (checkContextLost())
        return;

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glFinish);
    scheduleSyncCommand(&syncCommand);
}

/*!
 * \qmlmethod Canvas3DTexture Context3D::createTexture()
 * Create a Canvas3DTexture object and initialize a name for it as by calling \c{glGenTextures()}.
 */
QJSValue CanvasContext::createTexture()
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    CanvasTexture *texture = new CanvasTexture(m_commandQueue, this);
    QJSValue value = m_engine->newQObject(texture);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();
    addObjectToValidList(texture);
    return value;
}

/*!
 * \qmlmethod void Context3D::deleteTexture(Canvas3DTexture texture3D)
 * Deletes the given texture as if by calling \c{glDeleteTextures()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a texture3D is the Canvas3DTexture to be deleted.
 */
void CanvasContext::deleteTexture(QJSValue texture3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(texture:" << texture3D.toString()
                                         << ")";

    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (texture) {
        if (!checkValidity(texture, __FUNCTION__))
            return;
        texture->del();
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID texture handle:"
                                               << texture3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::scissor(int x, int y, int width, int height)
 * Defines a rectangle that constrains the drawing.
 * \a x is theleft edge of the rectangle.
 * \a y is the bottom edge of the rectangle.
 * \a width is the width of the rectangle.
 * \a height is the height of the rectangle.
 */
void CanvasContext::scissor(int x, int y, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glScissor,
                                 GLint(x), GLint(y), GLint(width), GLint(height));
}

/*!
 * \qmlmethod void Context3D::activeTexture(glEnums texture)
 * Sets the given texture unit as active. The number of texture units is implementation dependent,
 * but must be at least 8. Initially \c Context3D.TEXTURE0 is active.
 * \a texture must be one of \c Context3D.TEXTUREi values where \c i ranges from \c 0 to
 * \c{(Context3D.MAX_COMBINED_TEXTURE_IMAGE_UNITS-1)}.
 */
void CanvasContext::activeTexture(glEnums texture)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(texture:" << glEnumToString(texture)
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glActiveTexture, GLint(texture));
}

/*!
 * \qmlmethod void Context3D::bindTexture(glEnums target, Canvas3DTexture texture3D)
 * Bind a Canvas3DTexture to a texturing target.
 * \a target is the target of the active texture unit to which the Canvas3DTexture will be bound.
 * Must be either \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a texture3D is the Canvas3DTexture to be bound.
 */
void CanvasContext::bindTexture(glEnums target, QJSValue texture3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", texture:" << texture3D.toString()
                                         << ")";

    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (target == TEXTURE_2D) {
        m_currentTexture2D = texture;
    } else if (target == TEXTURE_CUBE_MAP) {
        m_currentTextureCubeMap = texture;
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Only TEXTURE_2D and TEXTURE_CUBE_MAP targets are supported.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (texture && checkValidity(texture, __FUNCTION__)) {
        if (target == TEXTURE_2D)
            m_currentTexture2D->bind(target);
        else if (target == TEXTURE_CUBE_MAP)
            m_currentTextureCubeMap->bind(target);
    } else {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindTexture, GLint(target), GLint(0));
    }
}

bool CanvasContext::isValidTextureBound(glEnums target, const QString &funcName, bool singleLayer)
{
    if (checkContextLost())
        return false;

    switch (target) {
    case TEXTURE_2D:
        if (!m_currentTexture2D) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "No current TEXTURE_2D bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        } else if (!m_currentTexture2D->isAlive()) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "Currently bound TEXTURE_2D is deleted";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
        break;
    case TEXTURE_CUBE_MAP:
    case TEXTURE_CUBE_MAP_POSITIVE_X:
    case TEXTURE_CUBE_MAP_NEGATIVE_X:
    case TEXTURE_CUBE_MAP_POSITIVE_Y:
    case TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case TEXTURE_CUBE_MAP_POSITIVE_Z:
    case TEXTURE_CUBE_MAP_NEGATIVE_Z:
        // Some functions only operate on single layer while some on the whole cube texture.
        // Both types have different set of acceptable TEXTURE_CUBE_MAP targets.
        if ((target == TEXTURE_CUBE_MAP) == singleLayer) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_ENUM:"
                                                   << "Invalid texture target;"
                                                   << glEnumToString(target);
            m_error |= CANVAS_INVALID_ENUM;
            return false;
        }
        if (!m_currentTextureCubeMap) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "No current TEXTURE_CUBE_MAP bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        } else if (!m_currentTextureCubeMap->isAlive()) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "Currently bound TEXTURE_CUBE_MAP is deleted";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                               << ":INVALID_ENUM:"
                                               << "Only TEXTURE_2D and TEXTURE_CUBE_MAP targets supported.";
        m_error |= CANVAS_INVALID_ENUM;
        return false;
    }

    return true;
}

bool CanvasContext::checkValidity(CanvasAbstractObject *obj, const char *function)
{
    if (obj) {
        if (obj->invalidated()) {
            m_error |= CANVAS_INVALID_OPERATION;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << function
                                                   << ":INVALID_OPERATION:"
                                                   << "Object is invalid";
            return false;
        } else if (obj->parent() != this) {
            m_error |= CANVAS_INVALID_OPERATION;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << function
                                                   << ":INVALID_OPERATION:"
                                                   << "Object from wrong context";
            return false;
        }
        return true;
    }

    m_error |= CANVAS_INVALID_OPERATION;
    qCWarning(canvas3drendering).nospace() << "Context3D::" << function
                                           << ":INVALID_OPERATION:"
                                           << "Null object";
    return false;
}

/*!
 * Transposes matrices. \a dim is the dimensions of the square matrices in \a src.
 * A newly allocated array containing transposed matrices is returned. \a count specifies how many
 * matrices are handled.
 * Required for uniformMatrix*fv functions in ES2.
 */
float *CanvasContext::transposeMatrix(int dim, int count, float *src)
{
    float *dest = new float[dim * dim * count];

    for (int k = 0; k < count; k++) {
        const int offset = k * dim * dim;
        for (int i = 0; i < dim; i++) {
            for (int j = 0; j < dim; j++)
                dest[offset + (i * dim) + j] = src[offset + (j * dim) + i];
        }
    }

    return dest;
}

/*!
 * Set matrix uniform values.
 */
void CanvasContext::uniformMatrixNfv(int dim, const QJSValue &location3D, bool transpose,
                                     const QJSValue &array)
{
    if (canvas3drendering().isDebugEnabled()) {
        QString command(QStringLiteral("uniformMatrix"));
        command.append(QString::number(dim));
        command.append(QStringLiteral("fv"));
        qCDebug(canvas3drendering).nospace().noquote() << "Context3D::" << command
                                                       << ", uniformLocation:" << location3D.toString()
                                                       << ", transpose:" << transpose
                                                       << ", array:" << array.toString()
                                                       << ")";
    }

    if (!isOfType(location3D, "QtCanvas3D::CanvasUniformLocation")) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasUniformLocation *locationObj =
            static_cast<CanvasUniformLocation *>(location3D.toQObject());

    if (!checkValidity(locationObj, __FUNCTION__))
        return;

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniformMatrixNfva(dim, locationObj, transpose, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    float *uniformData = reinterpret_cast<float * >(
                getTypedArrayAsRawDataPtr(array, arrayLen, QV4::Heap::TypedArray::Float32Array));

    if (!m_currentProgram || !uniformData || !locationObj)
        return;

    int numMatrices = arrayLen / (dim * dim * 4);

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "numMatrices:" << numMatrices;

    float *transposedMatrix = 0;
    if (m_isOpenGLES2 && transpose) {
        transpose = false;
        transposedMatrix = transposeMatrix(dim, numMatrices, uniformData);
        uniformData = transposedMatrix;
    }

    CanvasGlCommandQueue::GlCommandId id(CanvasGlCommandQueue::internalNoCommand);
    switch (dim) {
    case 2:
        id = CanvasGlCommandQueue::glUniformMatrix2fv;
        break;
    case 3:
        id = CanvasGlCommandQueue::glUniformMatrix3fv;
        break;
    case 4:
        id = CanvasGlCommandQueue::glUniformMatrix4fv;
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(uniformData), arrayLen);
    GlCommand &command = m_commandQueue->queueCommand(id, locationObj->id(), GLint(numMatrices),
                                                      GLint(transpose));
    command.data = dataArray;

    delete[] transposedMatrix;
}

/*!
 * Set matrix uniform values from JS array.
 */
void CanvasContext::uniformMatrixNfva(int dim, CanvasUniformLocation *uniformLocation,
                                     bool transpose, const QVariantList &array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;

    if (!m_currentProgram || !uniformLocation)
        return;

    int location3D = uniformLocation->id();
    int size = array.count();
    float *uniformData = new float[size];
    float *arrayPtr = uniformData;
    int numMatrices = size / (dim * dim);

    ArrayUtils::fillFloatArrayFromVariantList(array, arrayPtr);

    float *transposedMatrix = 0;
    if (m_isOpenGLES2 && transpose) {
        transpose = false;
        transposedMatrix = transposeMatrix(dim, numMatrices, arrayPtr);
        arrayPtr = transposedMatrix;
    }

    CanvasGlCommandQueue::GlCommandId id(CanvasGlCommandQueue::internalNoCommand);
    switch (dim) {
    case 2:
        id = CanvasGlCommandQueue::glUniformMatrix2fv;
        break;
    case 3:
        id = CanvasGlCommandQueue::glUniformMatrix3fv;
        break;
    case 4:
        id = CanvasGlCommandQueue::glUniformMatrix4fv;
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(arrayPtr), size * 4);
    GlCommand &command = m_commandQueue->queueCommand(id, location3D, GLint(numMatrices),
                                                      GLint(transpose));
    command.data = dataArray;

    delete[] uniformData;
    delete[] transposedMatrix;
}

/*!
 * \qmlmethod void Context3D::generateMipmap(glEnums target)
 * Generates a complete set of mipmaps for a texture object of the currently active texture unit.
 * \a target defines the texture target to which the texture object is bound whose mipmaps will be
 * generated. Must be either \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 */
void CanvasContext::generateMipmap(glEnums target)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__, false))
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glGenerateMipmap, GLint(target));
}

/*!
 * \qmlmethod bool Context3D::isTexture(Object anyObject)
 * Returns true if the given object is a valid Canvas3DTexture object.
 * \a anyObject is the object that is to be verified as a valid texture.
 * This command is handled synchronously.
 */
bool CanvasContext::isTexture(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasTexture *texture = getAsTexture3D(anyObject);
    if (texture && checkValidity(texture, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsTexture, texture->textureId());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasTexture *CanvasContext::getAsTexture3D(const QJSValue &anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasTexture"))
        return 0;

    CanvasTexture *texture = static_cast<CanvasTexture *>(anyObject.toQObject());
    if (!texture->isAlive())
        return 0;

    return texture;
}

/*!
 * \qmlmethod void Context3D::compressedTexImage2D(glEnums target, int level, glEnums internalformat, int width, int height, int border, TypedArray pixels)
 * Specify a 2D compressed texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the compressed texture.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 * \a pixels specifies the TypedArray containing the compressed image data.
 */
void CanvasContext::compressedTexImage2D(glEnums target, int level, glEnums internalformat,
                                         int width, int height, int border,
                                         QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    int byteLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(pixels, byteLen, QV4::Heap::TypedArray::UInt8Array);

    if (srcData) {
        // Driver implementation will handle checking of texture
        // properties for specific compression methods
        QByteArray *commandData = new QByteArray(reinterpret_cast<const char *>(srcData), byteLen);
        GlCommand &command =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glCompressedTexImage2D,
                                             GLint(target), GLint(level),
                                             GLint(internalformat), GLint(width),
                                             GLint(height), GLint(border));
        command.data = commandData;
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels must be TypedArray";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod void Context3D::compressedTexSubImage2D(glEnums target, int level, int xoffset, int yoffset, int width, int height, glEnums format, TypedArray pixels)
 * Specify a 2D compressed texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a xoffset Specifies a texel offset in the x direction within the texture array.
 * \a yoffset Specifies a texel offset in the y direction within the texture array.
 * \a width Width of the texture subimage.
 * \a height Height of the texture subimage.
 * \a pixels specifies the TypedArray containing the compressed image data.
 * \a format Format of the texel data given in \a pixels, must match the value
 * of \c internalFormat parameter given when the texture was created.
 * \a pixels TypedArray containing the compressed image data. If pixels is \c null.
 */
void CanvasContext::compressedTexSubImage2D(glEnums target, int level,
                                            int xoffset, int yoffset,
                                            int width, int height,
                                            glEnums format,
                                            QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", format:" << glEnumToString(format)
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    int byteLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(pixels, byteLen, QV4::Heap::TypedArray::UInt8Array);

    if (srcData) {
        // Driver implementation will handle checking of texture
        // properties for specific compression methods
        QByteArray *commandData = new QByteArray(reinterpret_cast<const char *>(srcData), byteLen);
        GlCommand &command =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glCompressedTexSubImage2D,
                                             GLint(target), GLint(level),
                                             GLint(xoffset), GLint(yoffset),
                                             GLint(width), GLint(height), GLint(format));
        command.data = commandData;
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels must be TypedArray";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod void Context3D::copyTexImage2D(glEnums target, int level, glEnums internalformat, int x, int y, int width, int height, int border)
 * Copies pixels into currently bound 2D texture.
 * \a target specifies the target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the texture. Must be \c{Context3D.ALPHA},
 * \c{Context3D.LUMINANCE}, \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a x specifies the window coordinate of the left edge of the rectangular region of pixels to be
 * copied.
 * \a y specifies the window coordinate of the lower edge of the rectangular region of pixels to be
 * copied.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 */
void CanvasContext::copyTexImage2D(glEnums target, int level, glEnums internalformat,
                                   int x, int y, int width, int height,
                                   int border)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glCopyTexImage2D,
                                 GLint(target), GLint(level),
                                 GLint(internalformat), GLint(x), GLint(y),
                                 GLint(width), GLint(height), GLint(border));
}

/*!
 * \qmlmethod void Context3D::copyTexSubImage2D(glEnums target, int level, int xoffset, int yoffset, int x, int y, int width, int height)
 * Copies to into a currently bound 2D texture subimage.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a xoffset specifies the texel offset in the x direction within the texture array.
 * \a yoffset specifies the texel offset in the y direction within the texture array.
 * \a x specifies the window coordinate of the left edge of the rectangular region of pixels to be
 * copied.
 * \a y specifies the window coordinate of the lower edge of the rectangular region of pixels to be
 * copied.
 * \a width specifies the width of the texture subimage.
 * \a height specifies the height of the texture subimage.
 */
void CanvasContext::copyTexSubImage2D(glEnums target, int level,
                                      int xoffset, int yoffset,
                                      int x, int y,
                                      int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glCopyTexSubImage2D,
                                 GLint(target), GLint(level),
                                 GLint(xoffset), GLint(yoffset),
                                 GLint(x), GLint(y),
                                 GLint(width), GLint(height));
}

bool CanvasContext::checkTextureFormats(glEnums internalFormat, glEnums format)
{
    // Internal format and format must match and be valid
    switch (format) {
    case ALPHA:
    case RGB:
    case RGBA:
    case LUMINANCE:
    case LUMINANCE_ALPHA:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::texImage2D()"
                                               << ":INVALID_ENUM:"
                                               << "format parameter is invalid";
        m_error |= CANVAS_INVALID_ENUM;
        return false;
    }

    if (format != internalFormat) {
        qCWarning(canvas3drendering).nospace() << "Context3D::texImage2D()"
                                               << ":INVALID_OPERATION:"
                                               << "internalFormat doesn't match format";
        m_error |= CANVAS_INVALID_OPERATION;
        return false;
    }

    return true;
}

bool CanvasContext::checkContextLost()
{
    if (m_contextLost) {
        qCWarning(canvas3drendering).nospace() << "Context3D::checkContextValid()"
                                               << ":CONTEXT LOST:"
                                               << "Context has been lost";
        return true;
    }
    return false;
}

/*!
 * \qmlmethod void Context3D::texImage2D(glEnums target, int level, glEnums internalformat, int width, int height, int border, glEnums format, glEnums type, TypedArray pixels)
 * Specify a 2D texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the texture. Must be \c{Context3D.ALPHA},
 * \c{Context3D.LUMINANCE}, \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 * \a format specifies the format of the texel data given in \a pixels, must match the value
 * of \a internalFormat.
 * \a type specifies the data type of the data given in \a pixels, must match the TypedArray type
 * of \a pixels. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a pixels specifies the TypedArray containing the image data. If pixels is \c{null}, a buffer
 * of sufficient size initialized to 0 is passed.
 */
void CanvasContext::texImage2D(glEnums target, int level, glEnums internalformat,
                               int width, int height, int border,
                               glEnums format, glEnums type,
                               QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", pixels:" << pixels.toString()
                                         << ")";
    if (!isValidTextureBound(target, __FUNCTION__) || !checkTextureFormats(internalformat, format))
        return;

    int bytesPerPixel = 0;
    uchar *srcData = 0;
    QByteArray *dataArray = 0;

    bool deleteTempPixels = false;
    if (pixels.isNull()) {
        deleteTempPixels = true;
        int size = getSufficientSize(type, width, height);
        srcData = new uchar[size];
        memset(srcData, 0, size);
    }

    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }

        if (bytesPerPixel == 0) {
            m_error |= CANVAS_INVALID_ENUM;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:Invalid format supplied "
                                                   << glEnumToString(format);
            if (deleteTempPixels)
                delete[] srcData;
            return;
        }

        if (!srcData)
            srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint8Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        dataArray = unpackPixels(srcData, false, bytesPerPixel, width, height);

        GlCommand &texImageCommand =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexImage2D,
                                             GLint(target), GLint(level),
                                             GLint(internalformat), GLint(width),
                                             GLint(height), GLint(border),
                                             GLint(format), GLint(type));
        texImageCommand.data = dataArray;
    }
        break;
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_5_5_5_1: {
        if (!srcData)
            srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt16Array);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint16Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        dataArray = unpackPixels(srcData, false, 2, width, height);

        GlCommand &texImageCommand =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexImage2D,
                                             GLint(target), GLint(level),
                                             GLint(internalformat), GLint(width),
                                             GLint(height), GLint(border),
                                             GLint(format), GLint(type));
        texImageCommand.data = dataArray;
    }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::"
                                               << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }

    if (deleteTempPixels)
        delete[] srcData;
}

uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue, int &byteLength,
                                                QV4::Heap::TypedArray::Type type)
{
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::TypedArray> typedArray(scope,
                                            QJSValuePrivate::convertedToValue(m_v4engine, jsValue));

    if (!typedArray)
        return 0;

    QV4::Heap::TypedArray::Type arrayType = typedArray->arrayType();
    if (type < QV4::Heap::TypedArray::NTypes && arrayType != type)
        return 0;

    uchar *dataPtr = reinterpret_cast<uchar *>(typedArray->arrayData()->data());
    dataPtr += typedArray->d()->byteOffset;
    byteLength = typedArray->byteLength();
    return dataPtr;
}

uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue,
                                                QV4::Heap::TypedArray::Type type)
{
    int dummy;
    return getTypedArrayAsRawDataPtr(jsValue, dummy, type);
}

uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue, int &byteLength)
{
    return getTypedArrayAsRawDataPtr(jsValue, byteLength, QV4::Heap::TypedArray::NTypes);
}

uchar *CanvasContext::getArrayBufferAsRawDataPtr(const QJSValue &jsValue, int &byteLength)
{
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::ArrayBuffer> arrayBuffer(scope,
                                              QJSValuePrivate::convertedToValue(m_v4engine, jsValue));
    if (!arrayBuffer)
        return 0;

    uchar *dataPtr = reinterpret_cast<uchar *>(arrayBuffer->data());
    byteLength = arrayBuffer->byteLength();
    return dataPtr;
}

/*!
 * \qmlmethod void Context3D::texSubImage2D(glEnums target, int level, int xoffset, int yoffset, int width, int height, glEnums format, glEnums type, TypedArray pixels)
 * Specify a 2D texture subimage.
 * \a target Target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a xoffset Specifies a texel offset in the x direction within the texture array.
 * \a yoffset Specifies a texel offset in the y direction within the texture array.
 * \a width Width of the texture subimage.
 * \a height Height of the texture subimage.
 * \a format Format of the texel data given in \a pixels, must match the value
 * of \c internalFormat parameter given when the texture was created.
 * \a type Data type of the data given in \a pixels, must match the TypedArray type
 * of \a pixels. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a pixels TypedArray containing the image data.
 */
void CanvasContext::texSubImage2D(glEnums target, int level,
                                  int xoffset, int yoffset,
                                  int width, int height,
                                  glEnums format, glEnums type,
                                  QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    if (pixels.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels was null";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    int bytesPerPixel = 0;
    uchar *srcData = 0;
    QByteArray *dataArray = 0;

    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }

        if (bytesPerPixel == 0) {
            m_error |= CANVAS_INVALID_ENUM;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:Invalid format "
                                                   << glEnumToString(format);
            return;
        }

        srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);
        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint8Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        dataArray = unpackPixels(srcData, false, bytesPerPixel, width, height);

        GlCommand &texImageCommand =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexSubImage2D,
                                             GLint(target), GLint(level),
                                             GLint(xoffset), GLint(yoffset),
                                             GLint(width), GLint(height),
                                             GLint(format), GLint(type));
        texImageCommand.data = dataArray;
    }
        break;
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_5_5_5_1: {
        srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt16Array);
        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint16Array, "
                                                   << "received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        dataArray = unpackPixels(srcData, false, 2, width, height);

        GlCommand &texImageCommand =
                m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexSubImage2D,
                                             GLint(target), GLint(level),
                                             GLint(xoffset), GLint(yoffset),
                                             GLint(width), GLint(height),
                                             GLint(format), GLint(type));
        texImageCommand.data = dataArray;
    }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }
}

/*!
 * If \a useSrcDataAsDst is true, just modifies the original array and returns a null pointer.
 */
QByteArray *CanvasContext::unpackPixels(uchar *srcData, bool useSrcDataAsDst,
                                        int bytesPerPixel, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(srcData:" << srcData
                                         << ", useSrcDataAsDst:" << useSrcDataAsDst
                                         << ", bytesPerPixel:" << bytesPerPixel
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    int bytesPerRow = width * bytesPerPixel;

    // Align bytesPerRow to UNPACK_ALIGNMENT setting
    if ( m_unpackAlignmentValue > 1)
        bytesPerRow = bytesPerRow + (m_unpackAlignmentValue - 1) - (bytesPerRow - 1) % m_unpackAlignmentValue;

    int totalBytes = bytesPerRow * height;

    QByteArray *unpackedData = 0;
    if (!m_unpackFlipYEnabled || srcData == 0 || width == 0 || height == 0 || bytesPerPixel == 0) {
        // No processing is needed, do a straight copy
        if (!useSrcDataAsDst)
            unpackedData = new QByteArray(reinterpret_cast<char *>(srcData), totalBytes);
    } else {
        if (useSrcDataAsDst) {
            uchar *row = new uchar[bytesPerRow];
            for (int y = 0; y < height; y++) {
                memcpy(row,
                       srcData + y * bytesPerRow,
                       bytesPerRow);
                memcpy(srcData + y * bytesPerRow,
                       srcData + (height - y - 1) * bytesPerRow,
                       bytesPerRow);
                memcpy(srcData + (height - y - 1) * bytesPerRow,
                       row,
                       bytesPerRow);
            }
            delete[] row;
        } else {
            unpackedData = new QByteArray(totalBytes, Qt::Uninitialized);
            uchar *ptr = reinterpret_cast<uchar*>(unpackedData->data());
            for (int y = 0; y < height; y++) {
                memcpy(ptr + (height - y - 1) * bytesPerRow,
                       srcData + y * bytesPerRow,
                       bytesPerRow);
            }
        }
    }

    return unpackedData;
}

/*!
 * \qmlmethod void Context3D::texImage2D(glEnums target, int level, glEnums internalformat, glEnums format, glEnums type, TextureImage texImage)
 * Uploads the given TextureImage element to the currently bound Canvas3DTexture.
 * \a target Target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a internalformat Internal format of the texture, conceptually the given image is first
 * converted to this format, then uploaded. Must be \c{Context3D.ALPHA}, \c{Context3D.LUMINANCE},
 * \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a format Format of the texture, must match the value of \a internalFormat.
 * \a type Type of the data, conceptually the given image is first converted to this type, then
 * uploaded. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a texImage A complete \c{TextureImage} loaded using the \c{TextureImageFactory}.
 */
void CanvasContext::texImage2D(glEnums target, int level, glEnums internalformat,
                               glEnums format, glEnums type, QJSValue texImage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", texImage:" << texImage.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__) || !checkTextureFormats(internalformat, format))
        return;

    CanvasTextureImage *image = getAsTextureImage(texImage);
    if (!image) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Invalid texImage " << texImage.toString();
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *pixels = 0;
    int bytesPerPixel = 0;
    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }
        pixels = image->convertToFormat(type, m_unpackFlipYEnabled, m_unpackPremultiplyAlphaEnabled);
        break;
    }
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        bytesPerPixel = 2;
        pixels = image->convertToFormat(type, m_unpackFlipYEnabled, m_unpackPremultiplyAlphaEnabled);
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels == 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":Conversion of pixels to format failed.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (target == TEXTURE_2D) {
        if (m_currentTexture2D && !m_currentTexture2D->hasSpecificName())
            m_currentTexture2D->setName("ImageTexture_"+image->name());
    } else {
        if (m_currentTextureCubeMap && !m_currentTextureCubeMap->hasSpecificName())
            m_currentTextureCubeMap->setName("ImageTexture_"+image->name());
    }

    int totalBytes = image->width() * image->height() * bytesPerPixel;
    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(pixels), totalBytes);
    GlCommand &texImageCommand =
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexImage2D,
                                         GLint(target), GLint(level),
                                         GLint(internalformat),
                                         GLint(image->width()), GLint(image->height()),
                                         GLint(0), GLint(format), GLint(type));
    texImageCommand.data = dataArray;
}

CanvasTextureImage* CanvasContext::getAsTextureImage(const QJSValue &anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasTextureImage"))
        return 0;

    CanvasTextureImage *texImage = static_cast<CanvasTextureImage *>(anyObject.toQObject());
    return texImage;
}


/*!
 * \qmlmethod void Context3D::texSubImage2D(glEnums target, int level, int xoffset, int yoffset, glEnums format, glEnums type, TextureImage texImage)
 * Uploads the given TextureImage element to the currently bound Canvas3DTexture.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a internalformat Internal format of the texture, conceptually the given image is first
 * converted to this format, then uploaded. Must be \c{Context3D.ALPHA}, \c{Context3D.LUMINANCE},
 * \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a format Format of the texture, must match the value of \a internalFormat.
 * \a type Type of the data, conceptually the given image is first converted to this type, then
 * uploaded. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a texImage A complete \c{TextureImage} loaded using the \c{TextureImageFactory}.
 */
void CanvasContext::texSubImage2D(glEnums target, int level,
                                  int xoffset, int yoffset,
                                  glEnums format, glEnums type, QJSValue texImage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", texImage:" << texImage.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    CanvasTextureImage *image = getAsTextureImage(texImage);
    if (!image) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:invalid texImage "
                                               << texImage.toString();
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *pixels = 0;
    int bytesPerPixel = 0;
    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }
        pixels = image->convertToFormat(type,
                                        m_unpackFlipYEnabled,
                                        m_unpackPremultiplyAlphaEnabled);
        break;
    }
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        bytesPerPixel = 2;
        pixels = image->convertToFormat(type,
                                        m_unpackFlipYEnabled,
                                        m_unpackPremultiplyAlphaEnabled);
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels == 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":Conversion of pixels to format failed.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    int totalBytes = image->width() * image->height() * bytesPerPixel;
    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(pixels), totalBytes);
    GlCommand &texImageCommand =
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexSubImage2D,
                                         GLint(target), GLint(level),
                                         GLint(xoffset), GLint(yoffset),
                                         GLint(image->width()), GLint(image->height()),
                                         GLint(format), GLint(type));
    texImageCommand.data = dataArray;
}

/*!
 * \qmlmethod void Context3D::texParameterf(glEnums target, glEnums pname, float param)
 * Sets texture parameters.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a pname specifies the symbolic name of a texture parameter. pname can be
 * \c{Context3D.TEXTURE_MIN_FILTER}, \c{Context3D.TEXTURE_MAG_FILTER}, \c{Context3D.TEXTURE_WRAP_S} or
 * \c{Context3D.TEXTURE_WRAP_T}.
 * \a param specifies the new float value to be set to \a pname
 */
void CanvasContext::texParameterf(glEnums target, glEnums pname, float param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( target:" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ", param:" << param
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__, false))
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexParameterf,
                                 GLint(target), GLint(pname), GLfloat(param));
}

/*!
 * \qmlmethod void Context3D::texParameteri(glEnums target, glEnums pname, float param)
 * Sets texture parameters.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a pname specifies the symbolic name of a texture parameter. pname can be
 * \c{Context3D.TEXTURE_MIN_FILTER}, \c{Context3D.TEXTURE_MAG_FILTER}, \c{Context3D.TEXTURE_WRAP_S} or
 * \c{Context3D.TEXTURE_WRAP_T}.
 * \a param specifies the new int value to be set to \a pname
 */
void CanvasContext::texParameteri(glEnums target, glEnums pname, int param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ", param:" << glEnumToString(glEnums(param))
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__, false))
        return;

    switch (pname) {
    case TEXTURE_MAG_FILTER:
    case TEXTURE_MIN_FILTER:
    case TEXTURE_WRAP_S:
    case TEXTURE_WRAP_T: {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glTexParameteri,
                                     GLint(target), GLint(pname), GLint(param));
        break;
    }
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:invalid pname "
                                               << glEnumToString(pname)
                                               << " must be one of: TEXTURE_MAG_FILTER, "
                                               << "TEXTURE_MIN_FILTER, TEXTURE_WRAP_S"
                                               << " or TEXTURE_WRAP_T";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }
}

int CanvasContext::getSufficientSize(glEnums internalFormat, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( internalFormat:" << glEnumToString(internalFormat)
                                         << " , width:" << width
                                         << ", height:" << height
                                         << ")";
    int bytesPerPixel = 0;
    switch (internalFormat) {
    case UNSIGNED_BYTE:
        bytesPerPixel = 4;
        break;
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        bytesPerPixel = 2;
        break;
    default:
        break;
    }

    width = (width > 0) ? width : 0;
    height = (height > 0) ? height : 0;

    return width * height * bytesPerPixel;
}

/*!
 * \qmlmethod Canvas3DFrameBuffer Context3D::createFramebuffer()
 * Returns a created Canvas3DFrameBuffer object that is initialized with a framebuffer object name as
 * if by calling \c{glGenFramebuffers()}.
 */
QJSValue CanvasContext::createFramebuffer()
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    CanvasFrameBuffer *framebuffer = new CanvasFrameBuffer(m_commandQueue, this);
    QJSValue value = m_engine->newQObject(framebuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << ":" << value.toString();
    addObjectToValidList(framebuffer);
    return value;
}

/*!
 * \qmlmethod void Context3D::bindFramebuffer(glEnums target, Canvas3DFrameBuffer buffer)
 * Binds the given \a buffer object to the given \a target.
 * \a target must be \c{Context3D.FRAMEBUFFER}.
 */
void CanvasContext::bindFramebuffer(glEnums target, QJSValue buffer)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", framebuffer:" << buffer.toString() << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_ENUM:"
                                               << " bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasFrameBuffer *framebuffer = getAsFramebuffer(buffer);

    GLint bindId = 0;
    if (framebuffer && checkValidity(framebuffer, __FUNCTION__)) {
        m_currentFramebuffer = framebuffer;
        bindId = framebuffer->id();
    } else {
        m_currentFramebuffer = 0;
    }

    if (!checkContextLost())
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindFramebuffer, bindId);
}

/*!
 * \qmlmethod Context3D::glEnums Context3D::checkFramebufferStatus(glEnums target)
 * Returns the completeness status of the framebuffer object.
 * \a target must be \c{Context3D.FRAMEBUFFER}.
 * This command is handled synchronously.
 */
CanvasContext::glEnums CanvasContext::checkFramebufferStatus(glEnums target)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ")";
    if (checkContextLost())
        return FRAMEBUFFER_UNSUPPORTED;

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return FRAMEBUFFER_UNSUPPORTED;
    }

    GLenum value(0);
    GlSyncCommand syncCommand(CanvasGlCommandQueue::glCheckFramebufferStatus,
                              GLint(GL_FRAMEBUFFER));
    syncCommand.returnValue = &value;
    scheduleSyncCommand(&syncCommand);
    return glEnums(value);
}

/*!
 * \qmlmethod void Context3D::framebufferRenderbuffer(glEnums target, glEnums attachment, glEnums renderbuffertarget, Canvas3DRenderBuffer renderbuffer3D)
 * Attaches the given \a renderbuffer3D object to the \a attachment point of the current framebuffer
 * object.
 * \a target must be \c{Context3D.FRAMEBUFFER}. \a renderbuffertarget must
 * be \c{Context3D.RENDERBUFFER}.
 */
void CanvasContext::framebufferRenderbuffer(glEnums target, glEnums attachment,
                                            glEnums renderbuffertarget,
                                            QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << "attachment:" << glEnumToString(attachment)
                                         << "renderbuffertarget:"
                                         << glEnumToString(renderbuffertarget)
                                         << ", renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM:bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (!m_currentFramebuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:no framebuffer bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (attachment != COLOR_ATTACHMENT0
            && attachment != DEPTH_ATTACHMENT
            && attachment != STENCIL_ATTACHMENT
            && attachment != DEPTH_STENCIL_ATTACHMENT) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:attachment must be one of "
                                               << "COLOR_ATTACHMENT0, DEPTH_ATTACHMENT, "
                                               << "STENCIL_ATTACHMENT or DEPTH_STENCIL_ATTACHMENT";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    if (renderbuffer) {
        if (renderbuffertarget != RENDERBUFFER) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << "(): INVALID_OPERATION renderbuffertarget must be"
                                                   << " RENDERBUFFER for non null renderbuffers";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        if (!checkValidity(renderbuffer, __FUNCTION__))
            return;
    }

    GLint renderbufferId = renderbuffer ? renderbuffer->id() : 0;

    if (attachment == DEPTH_STENCIL_ATTACHMENT) {
        GLint secondaryId = m_isCombinedDepthStencilSupported
                ? renderbufferId
                : (renderbuffer ? renderbuffer->secondaryId() : 0);
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glFramebufferRenderbuffer,
                                     GLint(GL_FRAMEBUFFER), GLint(DEPTH_ATTACHMENT),
                                     GLint(GL_RENDERBUFFER),
                                     GLint(renderbufferId));
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glFramebufferRenderbuffer,
                                     GLint(GL_FRAMEBUFFER), GLint(STENCIL_ATTACHMENT),
                                     GLint(GL_RENDERBUFFER),
                                     GLint(secondaryId));
    } else {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glFramebufferRenderbuffer,
                                     GLint(GL_FRAMEBUFFER), GLint(attachment),
                                     GLint(GL_RENDERBUFFER),
                                     GLint(renderbufferId));
    }
}

/*!
 * \qmlmethod void Context3D::framebufferTexture2D(glEnums target, glEnums attachment, glEnums textarget, Canvas3DTexture texture3D, int level)
 * Attaches the given \a renderbuffer object to the \a attachment point of the current framebuffer
 * object.
 * \a target must be \c{Context3D.FRAMEBUFFER}. \a renderbuffertarget must
 * be \c{Context3D.RENDERBUFFER}.
 */
void CanvasContext::framebufferTexture2D(glEnums target, glEnums attachment, glEnums textarget,
                                         QJSValue texture3D, int level)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", attachment:" << glEnumToString(attachment)
                                         << ", textarget:" << glEnumToString(textarget)
                                         << ", texture:" << texture3D.toString()
                                         << ", level:" << level
                                         << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_ENUM:"
                                               << " bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (!m_currentFramebuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:"
                                               << " no current framebuffer bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (attachment != COLOR_ATTACHMENT0 && attachment != DEPTH_ATTACHMENT
            && attachment != STENCIL_ATTACHMENT) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION attachment must be one of "
                                               << "COLOR_ATTACHMENT0, DEPTH_ATTACHMENT"
                                               << " or STENCIL_ATTACHMENT";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (texture) {
        if (!checkValidity(texture, __FUNCTION__))
            return;
        if (textarget != TEXTURE_2D
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_X
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_Y
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_Z
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_X
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_Y
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_Z) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << "(): textarget must be one of TEXTURE_2D, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_X, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_Y, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_Z, "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_X, "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_Y or "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_Z";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        if (level != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << "(): INVALID_VALUE level must be 0";
            m_error |= CANVAS_INVALID_VALUE;
            return;
        }
    }

    if (!checkContextLost()) {
        GLint textureId = texture ? texture->textureId() : 0;
        m_currentFramebuffer->setTexture(texture);
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glFramebufferTexture2D,
                                     GLint(target), GLint(attachment),
                                     GLint(textarget), textureId, GLint(level));
    }
}

/*!
 * \qmlmethod void Context3D::isFramebuffer(Object anyObject)
 * Returns true if the given object is a valid Canvas3DFrameBuffer object.
 * \a anyObject is the object that is to be verified as a valid framebuffer.
 * This command is handled synchronously.
 */
bool CanvasContext::isFramebuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( anyObject:" << anyObject.toString()
                                         << ")";

    CanvasFrameBuffer *fbo = getAsFramebuffer(anyObject);
    if (fbo && checkValidity(fbo, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsFramebuffer, fbo->id());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasFrameBuffer *CanvasContext::getAsFramebuffer(const QJSValue &anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasFrameBuffer"))
        return 0;

    CanvasFrameBuffer *fbo = static_cast<CanvasFrameBuffer *>(anyObject.toQObject());

    if (!fbo->isAlive())
        return 0;

    return fbo;
}

/*!
 * \qmlmethod void Context3D::deleteFramebuffer(Canvas3DFrameBuffer buffer)
 * Deletes the given framebuffer as if by calling \c{glDeleteFramebuffers()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a buffer is the Canvas3DFrameBuffer to be deleted.
 */
void CanvasContext::deleteFramebuffer(QJSValue buffer)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( buffer:" << buffer.toString()
                                         << ")";

    CanvasFrameBuffer *fbo = getAsFramebuffer(buffer);
    if (fbo) {
        if (!checkValidity(fbo, __FUNCTION__))
            return;
        fbo->del();
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_VALUE buffer handle";
    }
}

/*!
 * \qmlmethod Canvas3DRenderBuffer Context3D::createRenderbuffer()
 * Returns a created Canvas3DRenderBuffer object that is initialized with a renderbuffer object name
 * as if by calling \c{glGenRenderbuffers()}.
 */
QJSValue CanvasContext::createRenderbuffer()
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    CanvasRenderBuffer *renderbuffer =
            new CanvasRenderBuffer(m_commandQueue, !m_isCombinedDepthStencilSupported, this);
    QJSValue value = m_engine->newQObject(renderbuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();
    addObjectToValidList(renderbuffer);
    return value;
}

/*!
 * \qmlmethod void Context3D::bindRenderbuffer(glEnums target, Canvas3DRenderBuffer renderbuffer)
 * Binds the given \a renderbuffer3D object to the given \a target.
 * \a target must be \c{Context3D.RENDERBUFFER}.
 */
void CanvasContext::bindRenderbuffer(glEnums target, QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    if (target != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM target must be RENDERBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    GLint bindId = 0;
    if (renderbuffer && checkValidity(renderbuffer, __FUNCTION__)) {
        m_currentRenderbuffer = renderbuffer;
        bindId = renderbuffer->id();
    } else {
        m_currentRenderbuffer = 0;
    }
    if (!checkContextLost()) {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindRenderbuffer,
                                     GLint(GL_RENDERBUFFER), bindId);
    }
}

/*!
 * \qmlmethod void Context3D::renderbufferStorage(glEnums target, glEnums internalformat, int width, int height)
 * Create and initialize a data store for the \c renderbuffer object.
 * \a target must be \c Context3D.RENDERBUFFER.
 * \a internalformat specifies the color-renderable, depth-renderable or stencil-renderable format
 * of the renderbuffer. Must be one of \c{Context3D.RGBA4}, \c{Context3D.RGB565}, \c{Context3D.RGB5_A1},
 * \c{Context3D.DEPTH_COMPONENT16}, \c{Context3D.STENCIL_INDEX8}, or \c{Context3D.DEPTH_STENCIL}.
 * \a width specifies the renderbuffer width in pixels.
 * \a height specifies the renderbuffer height in pixels.
 */
void CanvasContext::renderbufferStorage(glEnums target, glEnums internalformat,
                                        int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    if (checkContextLost())
        return;

    if (target != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM target must be RENDERBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (!m_currentRenderbuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_OPERATION no renderbuffer bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (internalformat == DEPTH_STENCIL) {
        if (m_isCombinedDepthStencilSupported) {
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glRenderbufferStorage,
                                         GLint(target), GLint(GL_DEPTH24_STENCIL8),
                                         GLint(width), GLint(height));
        } else {
            // Some platforms do not support combined DEPTH_STENCIL buffer natively, so create
            // two separate render buffers for them. Depth buffer is the primary buffer
            // and the stencil buffer is the secondary buffer.
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glRenderbufferStorage,
                                         GLint(target), GLint(GL_DEPTH_COMPONENT16),
                                         GLint(width), GLint(height));
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindRenderbuffer,
                                         GLint(target), m_currentRenderbuffer->secondaryId());
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glRenderbufferStorage,
                                         GLint(target), GLint(GL_STENCIL_INDEX8),
                                         GLint(width), GLint(height));
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindRenderbuffer,
                                         GLint(target), m_currentRenderbuffer->id());
        }
    } else {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glRenderbufferStorage,
                                     GLint(target), GLint(internalformat),
                                     GLint(width), GLint(height));
    }
}

/*!
 * \qmlmethod bool Context3D::isRenderbuffer(Object anyObject)
 * Returns true if the given object is a valid Canvas3DRenderBuffer object.
 * \a anyObject is the object that is to be verified as a valid renderbuffer.
 * This command is handled synchronously.
 */
bool CanvasContext::isRenderbuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasRenderBuffer *rbo = getAsRenderbuffer3D(anyObject);
    if (rbo && checkValidity(rbo, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsRenderbuffer, rbo->id());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasRenderBuffer *CanvasContext::getAsRenderbuffer3D(const QJSValue &anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasRenderBuffer"))
        return 0;

    CanvasRenderBuffer *rbo = static_cast<CanvasRenderBuffer *>(anyObject.toQObject());
    if (!rbo->isAlive())
        return 0;

    return rbo;
}

/*!
 * \qmlmethod void Context3D::deleteRenderbuffer(Canvas3DRenderBuffer renderbuffer3D)
 * Deletes the given renderbuffer as if by calling \c{glDeleteRenderbuffers()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a renderbuffer3D is the Canvas3DRenderBuffer to be deleted.
 */
void CanvasContext::deleteRenderbuffer(QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    if (renderbuffer) {
        if (!checkValidity(renderbuffer, __FUNCTION__))
            return;
        renderbuffer->del();
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_VALUE renderbuffer handle";
    }
}

/*!
 * \qmlmethod void Context3D::sampleCoverage(float value, bool invert)
 * Sets the multisample coverage parameters.
 * \a value specifies the floating-point sample coverage value. The value is clamped to the range
 * \c{[0, 1]} with initial value of \c{1.0}.
 * \a invert specifies if coverage masks should be inverted.
 */
void CanvasContext::sampleCoverage(float value, bool invert)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(value:" << value
                                         << ", invert:" << invert
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glSampleCoverage,
                                 GLint(invert), GLfloat(value));
}

/*!
 * \qmlmethod Canvas3DProgram Context3D::createProgram()
 * Returns a created Canvas3DProgram object that is initialized with a program object name as if by
 * calling \c{glCreateProgram()}.
 */
QJSValue CanvasContext::createProgram()
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    CanvasProgram *program = new CanvasProgram(m_commandQueue, this);
    QJSValue value = m_engine->newQObject(program);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();
    addObjectToValidList(program);
    return value;
}

/*!
 * \qmlmethod bool Context3D::isProgram(Object anyObject)
 * Returns true if the given object is a valid Canvas3DProgram object.
 * \a anyObject is the object that is to be verified as a valid program.
 * This command is handled synchronously.
 */
bool CanvasContext::isProgram(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(anyObject);

    if (program && checkValidity(program, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsProgram, program->id());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasProgram *CanvasContext::getAsProgram3D(const QJSValue &anyObject, bool deadOrAlive) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasProgram"))
        return 0;

    CanvasProgram *program = static_cast<CanvasProgram *>(anyObject.toQObject());
    if (!deadOrAlive && !program->isAlive())
        return 0;

    return program;
}

/*!
 * \qmlmethod void Context3D::deleteProgram(Canvas3DProgram program3D)
 * Deletes the given program as if by calling \c{glDeleteProgram()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a program3D is the Canvas3DProgram to be deleted.
 */
void CanvasContext::deleteProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D, true);

    if (program) {
        if (!checkValidity(program, __FUNCTION__))
            return;
        program->del();
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE program handle:"
                                               << program3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::attachShader(Canvas3DProgram program3D, Canvas3DShader shader3D)
 * Attaches the given \a shader3D object to the given \a program3D object.
 * Calling this method repeatedly on the same object has no side effects.
 */
void CanvasContext::attachShader(QJSValue program3D, QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", shader:" << shader3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasShader *shader = getAsShader3D(shader3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid program handle "
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid shader handle "
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkValidity(program, __FUNCTION__) || !checkValidity(shader, __FUNCTION__))
        return;

    program->attach(shader);
}

/*!
 * \qmlmethod list<Canvas3DShader> Context3D::getAttachedShaders(Canvas3DProgram program3D)
 * Returns the list of shaders currently attached to the given \a program3D.
 */
QJSValue CanvasContext::getAttachedShaders(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    int index = 0;

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program) {
        m_error |= CANVAS_INVALID_VALUE;
        return QJSValue(QJSValue::NullValue);
    }

    if (!checkValidity(program, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    QList<CanvasShader *> shaders = program->attachedShaders();

    QJSValue shaderList = m_engine->newArray(shaders.count());

    for (QList<CanvasShader *>::const_iterator iter = shaders.constBegin();
         iter != shaders.constEnd(); ++iter) {
        CanvasShader *shader = *iter;
        shaderList.setProperty(index++, m_engine->newQObject((CanvasShader *)shader));
    }

    return shaderList;
}


/*!
 * \qmlmethod void Context3D::detachShader(Canvas3DProgram program, Canvas3DShader shader)
 * Detaches given shader object from given program object.
 * \a program3D specifies the program object from which to detach the shader.
 * \a shader3D specifies the shader object to detach.
 */
void CanvasContext::detachShader(QJSValue program3D, QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", shader:" << shader3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasShader *shader = getAsShader3D(shader3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid program handle "
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid shader handle "
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkValidity(program, __FUNCTION__) || !checkValidity(shader, __FUNCTION__))
        return;

    program->detach(shader);
}

/*!
 * \qmlmethod void Context3D::linkProgram(Canvas3DProgram program3D)
 * Links the given program object.
 * \a program3D specifies the program to be linked.
 */
void CanvasContext::linkProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program || !checkValidity(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    program->link();
}

/*!
 * \qmlmethod void Context3D::lineWidth(float width)
 * Specifies the width of rasterized lines.
 * \a width specifies the width to be used when rasterizing lines. Initial value is \c{1.0}.
 */
void CanvasContext::lineWidth(float width)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(width:" << width
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glLineWidth, GLfloat(width));
}

/*!
 * \qmlmethod void Context3D::polygonOffset(float factor, float units)
 * Sets scale and units used to calculate depth values.
 * \a factor specifies the scale factor that is used to create a variable depth offset for each
 * polygon. Initial value is \c{0.0}.
 * \a units gets multiplied by an implementation-specific value to create a constant depth offset.
 * Initial value is \c{0.0}.
 */
void CanvasContext::polygonOffset(float factor, float units)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(factor:" << factor
                                         << ", units:" << units
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glPolygonOffset,
                                 GLfloat(factor), GLfloat(units));
}

/*!
 * \qmlmethod void Context3D::pixelStorei(glEnums pname, int param)
 * Set the pixel storage modes.
 * \a pname specifies the name of the parameter to be set. \c {Context3D.PACK_ALIGNMENT} affects the
 * packing of pixel data into memory. \c {Context3D.UNPACK_ALIGNMENT} affects the unpacking of pixel
 * data from memory. \c {Context3D.UNPACK_FLIP_Y_WEBGL} is initially \c false, but once set, in any
 * subsequent calls to \l texImage2D or \l texSubImage2D, the source data is flipped along the
 * vertical axis. \c {Context3D.UNPACK_PREMULTIPLY_ALPHA_WEBGL} is initially \c false, but once set,
 * in any subsequent calls to \l texImage2D or \l texSubImage2D, the alpha channel of the source
 * data, is multiplied into the color channels during the data transfer. Initial value is \c false
 * and any non-zero value is interpreted as \c true.
 *
 * \a param specifies the value that \a pname is set to.
 */
void CanvasContext::pixelStorei(glEnums pname, int param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(pname:" << glEnumToString(pname)
                                         << ", param:" << param
                                         << ")";

    if (checkContextLost())
        return;

    switch (pname) {
    case UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipYEnabled = (param != 0);
        break;
    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlphaEnabled = (param != 0);
        break;
    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
        // Intentionally ignored
        break;
    case PACK_ALIGNMENT:
        if ( param == 1 || param == 2 || param == 4 || param == 8 ) {
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glPixelStorei,
                                         GLint(pname), GLint(param));
        } else {
            m_error |= CANVAS_INVALID_VALUE;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_VALUE:"
                                                   << "Invalid pack alignment: " << param;
        }
        break;
    case UNPACK_ALIGNMENT:
        if ( param == 1 || param == 2 || param == 4 || param == 8 ) {
            m_unpackAlignmentValue = param;
            m_commandQueue->queueCommand(CanvasGlCommandQueue::glPixelStorei,
                                         GLint(pname), GLint(param));
        } else {
            m_error |= CANVAS_INVALID_VALUE;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_VALUE:"
                                                   << "Invalid unpack alignment: " << param;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Invalid pname.";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }
}

/*!
 * \qmlmethod void Context3D::hint(glEnums target, glEnums mode)
 * Set implementation-specific hints.
 * \a target \c Context3D.GENERATE_MIPMAP_HINT is accepted.
 * \a mode \c{Context3D.FASTEST}, \c{Context3D.NICEST}, and \c{Context3D.DONT_CARE} are accepted.
 */
void CanvasContext::hint(glEnums target, glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ",mode:" << glEnumToString(mode) << ")";

    if (checkContextLost())
        return;

    switch (target) {
    case FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
        if (!m_standardDerivatives) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:"
                                                   << "OES_standard_derivatives extension needed for "
                                                   << "FRAGMENT_SHADER_DERIVATIVE_HINT_OES";
            m_error |= CANVAS_INVALID_ENUM;
            return;
        }
        break;
    case GENERATE_MIPMAP_HINT:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Invalid target.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    switch (mode) {
    case FASTEST:
    case NICEST:
    case DONT_CARE:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Invalid mode.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glHint, GLint(target), GLint(mode));
}

/*!
 * \qmlmethod void Context3D::enable(glEnums cap)
 * Enable server side GL capabilities.
 * \a cap specifies a constant indicating a GL capability.
 */
void CanvasContext::enable(glEnums cap)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(cap:" << glEnumToString(cap)
                                         << ")";

    if (isCapabilityValid(cap))
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glEnable, GLint(cap));
}

/*!
 * \qmlmethod bool Context3D::isEnabled(glEnums cap)
 * Returns whether a capability is enabled.
 * \a cap specifies a constant indicating a GL capability.
 * This command is handled synchronously.
 */
bool CanvasContext::isEnabled(glEnums cap)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(cap:" << glEnumToString(cap)
                                         << ")";
    GLboolean boolValue = false;
    if (isCapabilityValid(cap)) {
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsEnabled, GLint(cap));
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
    }
    return boolValue;
}

/*!
 * \qmlmethod void Context3D::disable(glEnums cap)
 * Disable server side GL capabilities.
 * \a cap specifies a constant indicating a GL capability.
 */
void CanvasContext::disable(glEnums cap)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(cap:" << glEnumToString(cap)
                                         << ")";

    if (isCapabilityValid(cap))
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glDisable, GLint(cap));
}

/*!
 * \qmlmethod void Context3D::blendColor(float red, float green, float blue, float alpha)
 * Set the blend color.
 * \a red, \a green, \a blue and \a alpha specify the components of \c{Context3D.BLEND_COLOR}.
 */
void CanvasContext::blendColor(float red, float green, float blue, float alpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(red:" << red
                                          << ", green:" << green
                                          << ", blue:" << blue
                                          << ", alpha:" << alpha
                                          << ")";

    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glBlendColor,
                                 GLfloat(red), GLfloat(green),
                                 GLfloat(blue), GLfloat(alpha));
}

bool CanvasContext::checkBlendMode(glEnums mode)
{
    if (checkContextLost())
        return false;

    switch (mode) {
    case FUNC_ADD:
    case FUNC_SUBTRACT:
    case FUNC_REVERSE_SUBTRACT:
        return true;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Mode must be one of following: FUNC_ADD, "
                                               << "FUNC_SUBTRACT, or FUNC_REVERSE_SUBTRACT.";
        m_error |= CANVAS_INVALID_ENUM;
        return false;
    }
}

/*!
 * \qmlmethod void Context3D::blendEquation(glEnums mode)
 * Sets the equation used for both the RGB blend equation and the alpha blend equation.
 * \a mode specifies how source and destination colors are to be combined. Must be
 * \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT} or \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 */
void CanvasContext::blendEquation(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ")";

    if (checkBlendMode(mode))
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBlendEquation, GLint(mode));
}

/*!
 * \qmlmethod void Context3D::blendEquationSeparate(glEnums modeRGB, glEnums modeAlpha)
 * Set the RGB blend equation and the alpha blend equation separately.
 * \a modeRGB specifies how the RGB components of the source and destination colors are to be
 * combined. Must be \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT} or
 * \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 * \a modeAlpha specifies how the alpha component of the source and destination colors are to be
 * combined. Must be \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT}, or
 * \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 */
void CanvasContext::blendEquationSeparate(glEnums modeRGB, glEnums modeAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(modeRGB:" << glEnumToString(modeRGB)
                                         << ", modeAlpha:" << glEnumToString(modeAlpha)
                                         << ")";

    if (checkBlendMode(modeRGB) && checkBlendMode(modeAlpha)) {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBlendEquationSeparate,
                                     GLint(modeRGB), GLint(modeAlpha));
    }
}

/*!
 * \qmlmethod void Context3D::blendFunc(glEnums sfactor, glEnums dfactor)
 * Sets the pixel arithmetic.
 * \a sfactor specifies how the RGBA source blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dfactor Specifies how the RGBA destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 */
void CanvasContext::blendFunc(glEnums sfactor, glEnums dfactor)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(sfactor:" << glEnumToString(sfactor)
                                         << ", dfactor:" << glEnumToString(dfactor)
                                         << ")";

    if (checkContextLost())
        return;

    if (((sfactor == CONSTANT_COLOR || sfactor == ONE_MINUS_CONSTANT_COLOR)
         && (dfactor == CONSTANT_ALPHA || dfactor == ONE_MINUS_CONSTANT_ALPHA))
            || ((dfactor == CONSTANT_COLOR || dfactor == ONE_MINUS_CONSTANT_COLOR)
                && (sfactor == CONSTANT_ALPHA || sfactor == ONE_MINUS_CONSTANT_ALPHA))) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_OPERATION illegal combination";
        return;
    }

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glBlendFunc,
                                 GLint(sfactor), GLint(dfactor));
}

/*!
 * \qmlmethod void Context3D::blendFuncSeparate(glEnums srcRGB, glEnums dstRGB, glEnums srcAlpha, glEnums dstAlpha)
 * Sets the pixel arithmetic for RGB and alpha components separately.
 * \a srcRGB specifies how the RGB source blending factors are computed. Must be \c{Context3D.ZERO},
 * \c{Context3D.ONE}, \c{Context3D.SRC_COLOR}, \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR},
 * \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA}, \c{Context3D.ONE_MINUS_SRC_ALPHA},
 * \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA}, \c{Context3D.CONSTANT_COLOR},
 * \c{Context3D.ONE_MINUS_CONSTANT_COLOR}, \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dstRGB Specifies how the RGB destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR}, \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 * \a srcAlpha specifies how the alpha source blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dstAlpha Specifies how the alpha destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 */
void CanvasContext::blendFuncSeparate(glEnums srcRGB, glEnums dstRGB, glEnums srcAlpha,
                                      glEnums dstAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(srcRGB:" << glEnumToString(srcRGB)
                                         << ", dstRGB:" << glEnumToString(dstRGB)
                                         << ", srcAlpha:" << glEnumToString(srcAlpha)
                                         << ", dstAlpha:" << glEnumToString(dstAlpha)
                                         << ")";

    if (checkContextLost())
        return;

    if (((srcRGB == CONSTANT_COLOR || srcRGB == ONE_MINUS_CONSTANT_COLOR )
         && (dstRGB == CONSTANT_ALPHA || dstRGB == ONE_MINUS_CONSTANT_ALPHA ))
            || ((dstRGB == CONSTANT_COLOR || dstRGB == ONE_MINUS_CONSTANT_COLOR )
                && (srcRGB == CONSTANT_ALPHA || srcRGB == ONE_MINUS_CONSTANT_ALPHA ))) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_OPERATION illegal combination";
        return;
    }

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glBlendFuncSeparate,
                                 GLint(srcRGB), GLint(dstRGB),
                                 GLint(srcAlpha), GLint(dstAlpha));
}

/*!
 * \qmlmethod variant Context3D::getProgramParameter(Canvas3DProgram program3D, glEnums paramName)
 * Return the value for the passed \a paramName given the passed \a program3D. The type returned is
 * the natural type for the requested paramName.
 * \a paramName must be \c{Context3D.DELETE_STATUS}, \c{Context3D.LINK_STATUS},
 * \c{Context3D.VALIDATE_STATUS}, \c{Context3D.ATTACHED_SHADERS}, \c{Context3D.ACTIVE_ATTRIBUTES} or
 * \c{Context3D.ACTIVE_UNIFORMS}.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getProgramParameter(QJSValue program3D, glEnums paramName)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", paramName:" << glEnumToString(paramName)
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program || !checkValidity(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetProgramiv,
                              GLint(program->id()), GLint(paramName));
    GLint value = 0;
    syncCommand.returnValue = &value;

    switch(paramName) {
    case DELETE_STATUS:
        // Intentional flow through
    case LINK_STATUS:
        // Intentional flow through
    case VALIDATE_STATUS: {
        scheduleSyncCommand(&syncCommand);
        if (syncCommand.glError) {
            return QJSValue(QJSValue::NullValue);
        } else {
            const bool boolValue = (value == GL_TRUE);
            qCDebug(canvas3drendering).nospace() << "    getProgramParameter returns " << boolValue;
            return QJSValue(boolValue);
        }
    }
    case ATTACHED_SHADERS:
        // Intentional flow through
    case ACTIVE_ATTRIBUTES:
        // Intentional flow through
    case ACTIVE_UNIFORMS: {
        scheduleSyncCommand(&syncCommand);
        if (syncCommand.glError) {
            return QJSValue(QJSValue::NullValue);
        } else {
            qCDebug(canvas3drendering).nospace() << "    getProgramParameter returns " << value;
            return QJSValue(value);
        }
    }
    default: {
        m_error |= CANVAS_INVALID_ENUM;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM illegal parameter name ";
        return QJSValue(QJSValue::NullValue);
    }
    }
}

/*!
 * \qmlmethod Canvas3DShader Context3D::createShader(glEnums type)
 * Creates a shader of \a type. Must be either \c Context3D.VERTEX_SHADER or
 * \c{Context3D.FRAGMENT_SHADER}.
 */
QJSValue CanvasContext::createShader(glEnums type)
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    switch (type) {
    case VERTEX_SHADER: // Intentional fall through
    case FRAGMENT_SHADER: {
        qCDebug(canvas3drendering).nospace() << "Context3D::createShader("
                                             << glEnumToString(type) << ")";
        CanvasShader *shader = new CanvasShader(GLenum(type), m_commandQueue, this);
        addObjectToValidList(shader);
        return m_engine->newQObject(shader);
    }
    default: {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:unknown shader type:"
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }
    }
}

/*!
 * \qmlmethod bool Context3D::isShader(Object anyObject)
 * Returns true if the given object is a valid Canvas3DShader object.
 * \a anyObject is the object that is to be verified as a valid shader.
 * This command is handled synchronously.
 */
bool CanvasContext::isShader(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasShader *shader3D = getAsShader3D(anyObject);
    if (shader3D && checkValidity(shader3D, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsShader, shader3D->id());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasShader *CanvasContext::getAsShader3D(const QJSValue &shader3D, bool deadOrAlive) const
{
    if (!isOfType(shader3D, "QtCanvas3D::CanvasShader"))
        return 0;

    CanvasShader *shader = static_cast<CanvasShader *>(shader3D.toQObject());
    if (!deadOrAlive && !shader->isAlive())
        return 0;

    return shader;
}

/*!
 * \qmlmethod void Context3D::deleteShader(Canvas3DShader shader)
 * Deletes the given shader as if by calling \c{glDeleteShader()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a shader is the Canvas3DShader to be deleted.
 */
void CanvasContext::deleteShader(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::"
                                         << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";

    CanvasShader *shader = getAsShader3D(shader3D, true);

    if (shader) {
        if (!checkValidity(shader, __FUNCTION__))
            return;
        shader->del();
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Invalid shader handle:" << shader3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::shaderSource(Canvas3DShader shader, string shaderSource)
 * Replaces the shader source code in the given shader object.
 * \a shader specifies the shader object whose source code is to be replaced.
 * \a shaderSource specifies the source code to be loaded in to the shader.
 */
void CanvasContext::shaderSource(QJSValue shader3D, const QString &shaderSource)
{
    QString modSource = "#version 120 \n#define precision \n"+ shaderSource;

    if (m_isOpenGLES2)
        modSource = shaderSource;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ", shaderSource"
                                         << ")" << endl << modSource << endl;

    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return;
    }
    if (!checkValidity(shader, __FUNCTION__))
        return;

    shader->setSourceCode(modSource);
}


/*!
 * \qmlmethod string Context3D::getShaderSource(Canvas3DShader shader)
 * Returns the source code string from the \a shader object.
 */
QJSValue CanvasContext::getShaderSource(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";

    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkValidity(shader, __FUNCTION__))
        return false;

    return QJSValue(shader->sourceCode());
}

/*!
 * \qmlmethod void Context3D::compileShader(Canvas3DShader shader)
 * Compiles the given \a shader object.
 */
void CanvasContext::compileShader(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return;
    }
    if (!checkValidity(shader, __FUNCTION__))
        return;

    shader->compileShader();
}

CanvasUniformLocation *CanvasContext::getAsUniformLocation3D(const QJSValue &anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasUniformLocation"))
        return 0;

    CanvasUniformLocation *uniformLocation =
            static_cast<CanvasUniformLocation *>(anyObject.toQObject());

    return uniformLocation;
}

/*!
 * \qmlmethod void Context3D::uniform1i(Canvas3DUniformLocation location3D, int x)
 * Sets the single integer value given in \a x to the given uniform \a location3D.
 */
void CanvasContext::uniform1i(QJSValue location3D, int x)
{
    uniformNi(1, location3D, x);
}

/*!
 * \qmlmethod void Context3D::uniform1iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform1iv(QJSValue location3D, QJSValue array)
{
    uniformNxv(1, false, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform1f(Canvas3DUniformLocation location3D, float x)
 * Sets the single float value given in \a x to the given uniform \a location3D.
 */
void CanvasContext::uniform1f(QJSValue location3D, float x)
{
    uniformNf(1, location3D, x);
}

/*!
 * \qmlmethod void Context3D::uniform1fvt(Canvas3DUniformLocation location3D, Object array)
 * Sets the float array given in \a array to the given uniform \a location3D. \a array must be
 * a JavaScript \c Array object or a \c Float32Array object.
 */
void CanvasContext::uniform1fv(QJSValue location3D, QJSValue array)
{
    uniformNxv(1, true, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform2f(Canvas3DUniformLocation location3D, float x, float y)
 * Sets the two float values given in \a x and \a y to the given uniform \a location3D.
 */
void CanvasContext::uniform2f(QJSValue location3D, float x, float y)
{
    uniformNf(2, location3D, x, y);
}

/*!
 * \qmlmethod void Context3D::uniform2fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform2fv(QJSValue location3D, QJSValue array)
{
    uniformNxv(2, true, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform2i(Canvas3DUniformLocation location3D, int x, int y)
 * Sets the two integer values given in \a x and \a y to the given uniform \a location3D.
 */
void CanvasContext::uniform2i(QJSValue location3D, int x, int y)
{
    uniformNi(2, location3D, x, y);
}

/*!
 * \qmlmethod void Context3D::uniform2iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform2iv(QJSValue location3D, QJSValue array)
{
    uniformNxv(2, false, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform3f(Canvas3DUniformLocation location3D, float x, float y, float z)
 * Sets the three float values given in \a x , \a y and \a z to the given uniform \a location3D.
 */
void CanvasContext::uniform3f(QJSValue location3D, float x, float y, float z)
{
    uniformNf(3, location3D, x, y, z);
}

/*!
 * \qmlmethod void Context3D::uniform3fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform3fv(QJSValue location3D, QJSValue array)
{
    uniformNxv(3, true, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform3i(Canvas3DUniformLocation location3D, int x, int y, int z)
 * Sets the three integer values given in \a x , \a y and \a z to the given uniform \a location3D.
 */
void CanvasContext::uniform3i(QJSValue location3D, int x, int y, int z)
{
    uniformNi(3, location3D, x, y, z);
}

/*!
 * \qmlmethod void Context3D::uniform3iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform3iv(QJSValue location3D, QJSValue array)
{
    uniformNxv(3, false, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform4f(Canvas3DUniformLocation location3D, float x, float y, float z, float w)
 * Sets the four float values given in \a x , \a y , \a z and \a w to the given uniform \a location3D.
 */
void CanvasContext::uniform4f(QJSValue location3D, float x, float y, float z, float w)
{
    uniformNf(4, location3D, x, y, z, w);
}

/*!
 * \qmlmethod void Context3D::uniform4fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform4fv(QJSValue location3D, QJSValue array)
{
    uniformNxv(4, true, location3D, array);
}

/*!
 * \qmlmethod void Context3D::uniform4i(Canvas3DUniformLocation location3D, int x, int y, int z, int w)
 * Sets the four integer values given in \a x , \a y , \a z and \a w to the given uniform
 * \a location3D.
 */
void CanvasContext::uniform4i(QJSValue location3D, int x, int y, int z, int w)
{
    uniformNi(4, location3D, x, y, z, w);
}

/*!
 * \qmlmethod void Context3D::uniform4iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
void CanvasContext::uniform4iv(QJSValue location3D, QJSValue array)
{
    uniformNxv(4, false, location3D, array);
}

void CanvasContext::vertexAttribNfv(int dim, unsigned int indx, const QJSValue &array)
{
    if (canvas3drendering().isDebugEnabled()) {
        QString command(QStringLiteral("vertexAttrib"));
        command.append(QString::number(dim));
        command.append(QStringLiteral("fv"));
        qCDebug(canvas3drendering).nospace().noquote() << "Context3D::" << command
                                                       << ", indx:" << indx
                                                       << ", array:" << array.toString()
                                                       << ")";
    }

    if (checkContextLost())
        return;

    CanvasGlCommandQueue::GlCommandId id(CanvasGlCommandQueue::internalNoCommand);
    switch (dim) {
    case 1:
        id = CanvasGlCommandQueue::glVertexAttrib1fv;
        break;
    case 2:
        id = CanvasGlCommandQueue::glVertexAttrib2fv;
        break;
    case 3:
        id = CanvasGlCommandQueue::glVertexAttrib3fv;
        break;
    case 4:
        id = CanvasGlCommandQueue::glVertexAttrib4fv;
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        vertexAttribNfva(id, indx, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *attribData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                  QV4::Heap::TypedArray::Float32Array);

    if (!attribData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(attribData), arrayLen);

    GlCommand &command = m_commandQueue->queueCommand(id, GLint(indx));
    command.data = dataArray;
}

void CanvasContext::vertexAttribNfva(CanvasGlCommandQueue::GlCommandId id, unsigned int indx,
                                     const QVariantList &values)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;

    QByteArray *dataArray = new QByteArray(values.count() * sizeof(float), Qt::Uninitialized);

    ArrayUtils::fillFloatArrayFromVariantList(values, reinterpret_cast<float *>(dataArray->data()));

    GlCommand &command = m_commandQueue->queueCommand(id, GLint(indx));
    command.data = dataArray;
}

void CanvasContext::uniformNf(int dim, const QJSValue &location, float x, float y, float z, float w)
{
    if (canvas3drendering().isDebugEnabled()) {
        QString command(QStringLiteral("uniform"));
        command.append(QString::number(dim));
        command.append(QStringLiteral("f"));
        qCDebug(canvas3drendering).nospace().noquote() << "Context3D::" << command
                                                       << "(location3D:" << location.toString()
                                                       << ", x:" << x
                                                       << ", y:" << y
                                                       << ", z:" << z
                                                       << ", w:" << w
                                                       << ")";
    }

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location);

    if (!locationObj || !checkValidity(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    switch (dim) {
    case 1:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform1f,
                                     locationObj->id(),
                                     GLfloat(x));
        break;
    case 2:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform2f,
                                     locationObj->id(),
                                     GLfloat(x), GLfloat(y));
        break;
    case 3:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform3f,
                                     locationObj->id(),
                                     GLfloat(x), GLfloat(y), GLfloat(z));
        break;
    case 4:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform4f,
                                     locationObj->id(),
                                     GLfloat(x), GLfloat(y), GLfloat(z), GLfloat(w));
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }
}

void CanvasContext::uniformNi(int dim, const QJSValue &location, int x, int y, int z, int w)
{
    if (canvas3drendering().isDebugEnabled()) {
        QString command(QStringLiteral("uniform"));
        command.append(QString::number(dim));
        command.append(QStringLiteral("i"));
        qCDebug(canvas3drendering).nospace().noquote() << "Context3D::" << command
                                                       << "(location3D:" << location.toString()
                                                       << ", x:" << x
                                                       << ", y:" << y
                                                       << ", z:" << z
                                                       << ", w:" << w
                                                       << ")";
    }

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location);

    if (!locationObj || !checkValidity(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    switch (dim) {
    case 1:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform1i,
                                     locationObj->id(), GLint(x));
        break;
    case 2:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform2i,
                                     locationObj->id(), GLint(x), GLint(y));
        break;
    case 3:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform3i,
                                     locationObj->id(), GLint(x), GLint(y), GLint(z));
        break;
    case 4:
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glUniform4i,
                                     locationObj->id(), GLint(x), GLint(y), GLint(z), GLint(w));
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }
}

void CanvasContext::uniformNxva(int dim, bool typeFloat, CanvasGlCommandQueue::GlCommandId id,
                                CanvasUniformLocation *location, const QVariantList &array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;
    QByteArray *dataArray = new QByteArray(array.length() * 4, Qt::Uninitialized);

    if (typeFloat)
        ArrayUtils::fillFloatArrayFromVariantList(array, reinterpret_cast<float *>(dataArray->data()));
    else
        ArrayUtils::fillIntArrayFromVariantList(array, reinterpret_cast<int *>(dataArray->data()));

    GlCommand &uniformCommand = m_commandQueue->queueCommand(id, location->id(),
                                                             GLint(array.length() / dim));
    uniformCommand.data = dataArray;
}

void CanvasContext::uniformNxv(int dim, bool typeFloat, const QJSValue &location, const QJSValue &array)
{
    if (canvas3drendering().isDebugEnabled()) {
        QString command(QStringLiteral("uniform"));
        command.append(QString::number(dim));
        if (typeFloat)
            command.append(QStringLiteral("f"));
        else
            command.append(QStringLiteral("i"));
        command.append(QStringLiteral("v"));
        qCDebug(canvas3drendering).nospace().noquote() << "Context3D::" << command
                                                       << "(location3D:" << location.toString()
                                                       << ", array:" << array.toString()
                                                       << ")";
    }

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location);
    if (!locationObj || !checkValidity(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasGlCommandQueue::GlCommandId id(CanvasGlCommandQueue::internalNoCommand);
    switch (dim) {
    case 1:
        id = typeFloat ? CanvasGlCommandQueue::glUniform1fv : CanvasGlCommandQueue::glUniform1iv;
        break;
    case 2:
        id = typeFloat ? CanvasGlCommandQueue::glUniform2fv : CanvasGlCommandQueue::glUniform2iv;
        break;
    case 3:
        id = typeFloat ? CanvasGlCommandQueue::glUniform3fv : CanvasGlCommandQueue::glUniform3iv;
        break;
    case 4:
        id = typeFloat ? CanvasGlCommandQueue::glUniform4fv : CanvasGlCommandQueue::glUniform4iv;
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniformNxva(dim, typeFloat, id, locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   typeFloat ? QV4::Heap::TypedArray::Float32Array
                                                             : QV4::Heap::TypedArray::Int32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    QByteArray *dataArray = new QByteArray(reinterpret_cast<char *>(uniformData), arrayLen);

    arrayLen /= (4 * dim); // get value count
    GlCommand &uniformCommand = m_commandQueue->queueCommand(id, locationObj->id(),
                                                             GLint(arrayLen));
    uniformCommand.data = dataArray;
}

/*!
 * Checks if capability passed to enable, isEnabled, or disable function is valid.
 */
bool CanvasContext::isCapabilityValid(CanvasContext::glEnums cap)
{
    bool capValid = false;
    if (!checkContextLost()) {
        switch (GLint(cap)) {
        case CULL_FACE:
        case BLEND:
        case DITHER:
        case STENCIL_TEST:
        case DEPTH_TEST:
        case SCISSOR_TEST:
        case POLYGON_OFFSET_FILL:
        case SAMPLE_ALPHA_TO_COVERAGE:
        case SAMPLE_COVERAGE:
            capValid = true;
            break;
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:"
                                                   << "Tried to enable, disable, or query an invalid capability:"
                                                   << glEnumToString(cap);
            m_error |= CANVAS_INVALID_ENUM;
            break;
        }
    }
    return capValid;
}

/*!
 * \qmlmethod void Context3D::vertexAttrib1f(int indx, float x)
 * Sets the single float value given in \a x to the generic vertex attribute index specified
 * by \a indx.
 */
void CanvasContext::vertexAttrib1f(unsigned int indx, float x)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glVertexAttrib1f, GLint(indx), GLfloat(x));
}

/*!
 * \qmlmethod void Context3D::vertexAttrib1fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib1fv(unsigned int indx, QJSValue array)
{
    vertexAttribNfv(1, indx, array);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib2f(int indx, float x, float y)
 * Sets the two float values given in \a x and \a y to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib2f(unsigned int indx, float x, float y)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glVertexAttrib2f,
                                 GLint(indx), GLfloat(x), GLfloat(y));
}

/*!
 * \qmlmethod void Context3D::vertexAttrib2fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib2fv(unsigned int indx, QJSValue array)
{
    vertexAttribNfv(2, indx, array);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib3f(int indx, float x, float y, float z)
 * Sets the three float values given in \a x , \a y and \a z to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib3f(unsigned int indx, float x, float y, float z)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glVertexAttrib3f,
                                 GLint(indx), GLfloat(x), GLfloat(y), GLfloat(z));
}

/*!
 * \qmlmethod void Context3D::vertexAttrib3fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib3fv(unsigned int indx, QJSValue array)
{
    vertexAttribNfv(3, indx, array);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib4f(int indx, float x, float y, float z, float w)
 * Sets the four float values given in \a x , \a y , \a z and \a w to the generic vertex attribute
 * index specified by \a indx.
 */
void CanvasContext::vertexAttrib4f(unsigned int indx, float x, float y, float z, float w)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ", w:" << w
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glVertexAttrib4f,
                                 GLint(indx), GLfloat(x), GLfloat(y), GLfloat(z), GLfloat(w));
}

/*!
 * \qmlmethod void Context3D::vertexAttrib4fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
void CanvasContext::vertexAttrib4fv(unsigned int indx, QJSValue array)
{
    vertexAttribNfv(4, indx, array);
}

/*!
 * \qmlmethod int Context3D::getShaderParameter(Canvas3DShader shader, glEnums pname)
 * Returns the value of the passed \a pname for the given \a shader.
 * \a pname must be one of \c{Context3D.SHADER_TYPE}, \c Context3D.DELETE_STATUS and
 * \c{Context3D.COMPILE_STATUS}.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getShaderParameter(QJSValue shader3D, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ", pname:"<< glEnumToString(pname)
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               <<"Invalid shader handle:" << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkValidity(shader, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetShaderiv,
                              GLint(shader->id()), GLint(pname));
    GLint value = 0;
    syncCommand.returnValue = &value;

    switch (pname) {
    case SHADER_TYPE: {
        scheduleSyncCommand(&syncCommand);
        if (syncCommand.glError) {
            return QJSValue(QJSValue::NullValue);
        } else {
            qCDebug(canvas3drendering).nospace() << "    getShaderParameter returns " << value;
            return QJSValue(glEnums(value));
        }
    }
    case DELETE_STATUS:
        // Intentional fall-through
    case COMPILE_STATUS: {
        scheduleSyncCommand(&syncCommand);
        if (syncCommand.glError) {
            return QJSValue(QJSValue::NullValue);
        } else {
            qCDebug(canvas3drendering).nospace() << "    getShaderParameter returns " << bool(value);
            return QJSValue(bool(value));
        }
    }
    default: {
        qCWarning(canvas3drendering).nospace() << "getShaderParameter():UNSUPPORTED parameter name "
                                               << glEnumToString(pname);
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }
    }
}

/*!
 * \qmlmethod Canvas3DBuffer Context3D::createBuffer()
 * Creates a Canvas3DBuffer object and initializes it with a buffer object name as if \c glGenBuffers()
 * was called.
 */
QJSValue CanvasContext::createBuffer()
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    CanvasBuffer *newBuffer = new CanvasBuffer(m_commandQueue, this);
    m_idToCanvasBufferMap.insert(newBuffer->id(), newBuffer);

    QJSValue value = m_engine->newQObject(newBuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << ":" << value.toString() << " = " << newBuffer;
    addObjectToValidList(newBuffer);
    return value;
}

/*!
 * \qmlmethod Canvas3DUniformLocation Context3D::getUniformLocation(Canvas3DProgram program3D, string name)
 * Returns Canvas3DUniformLocation object that represents the location3D of a specific uniform variable
 * with the given \a name within the given \a program3D object.
 * Returns \c null if name doesn't correspond to a uniform variable.
 */
QJSValue CanvasContext::getUniformLocation(QJSValue program3D, const QString &name)
{
    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(program3D:" << program3D.toString()
                                             << ", name:" << name
                                             << "):-1";
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING:Invalid Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkValidity(program, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    CanvasUniformLocation *location3D = new CanvasUniformLocation(m_commandQueue, this);
    location3D->setName(name);
    QJSValue value = m_engine->newQObject(location3D);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", name:" << value.toString()
                                         << "):" << location3D;
    addObjectToValidList(location3D);

    GlCommand &command = m_commandQueue->queueCommand(CanvasGlCommandQueue::glGetUniformLocation,
                                                      location3D->id(), program->id());
    QByteArray *commandData = new QByteArray(name.toLatin1());
    command.data = commandData;

    return value;
}

/*!
 * \qmlmethod int Context3D::getAttribLocation(Canvas3DProgram program3D, string name)
 * Returns location3D of the given attribute variable \a name in the given \a program3D.
 * This command is handled synchronously.
 */
int CanvasContext::getAttribLocation(QJSValue program3D, const QString &name)
{
    if (checkContextLost())
        return -1;

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(program3D:" << program3D.toString()
                                             << ", name:" << name
                                             << "):-1";
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return -1;
    } else {
        if (!checkValidity(program, __FUNCTION__))
            return -1;
        GLint attribLoc = -1;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetAttribLocation, program->id());
        syncCommand.data = new QByteArray(name.toLatin1());
        syncCommand.returnValue = &attribLoc;

        scheduleSyncCommand(&syncCommand);
        if (syncCommand.glError) {
            return -1;
        } else {
            qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                 << "(program3D:" << program3D.toString()
                                                 << ", name:" << name
                                                 << "):" << attribLoc;
            return int(attribLoc);
        }
    }
}

/*!
 * \qmlmethod void Context3D::bindAttribLocation(Canvas3DProgram program3D, int index, string name)
 * Binds the attribute \a index with the attribute variable \a name in the given \a program3D.
 */
void CanvasContext::bindAttribLocation(QJSValue program3D, int index, const QString &name)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index
                                         << ", name:" << name
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (!checkValidity(program, __FUNCTION__))
        return;

    program->bindAttributeLocation(index, name);
}

/*!
 * \qmlmethod void Context3D::enableVertexAttribArray(int index)
 * Enables the vertex attribute array at \a index.
 */
void CanvasContext::enableVertexAttribArray(int index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index:" << index
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glEnableVertexAttribArray, GLint(index));
}

/*!
 * \qmlmethod void Context3D::disableVertexAttribArray(int index)
 * Disables the vertex attribute array at \a index.
 */
void CanvasContext::disableVertexAttribArray(int index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index:" << index
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDisableVertexAttribArray, GLint(index));
}

/*!
 * \qmlmethod void Context3D::uniformMatrix2fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 2x2 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
void CanvasContext::uniformMatrix2fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(2, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::uniformMatrix3fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 3x3 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
void CanvasContext::uniformMatrix3fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(3, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::uniformMatrix4fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 4x4 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
void CanvasContext::uniformMatrix4fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(4, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::vertexAttribPointer(int indx, int size, glEnums type, bool normalized, int stride, long offset)
 * Sets the currently bound array buffer to the vertex attribute at the index passed at \a indx.
 * \a size is the number of components per attribute. \a stride specifies the byte offset between
 * consecutive vertex attributes. \a offset specifies the byte offset to the first vertex attribute
 * in the array. If int values should be normalized, set \a normalized to \c{true}.
 *
 * \a type specifies the element type and can be one of:
 * \list
 * \li \c{Context3D.BYTE}
 * \li \c{Context3D.UNSIGNED_BYTE}
 * \li \c{Context3D.SHORT}
 * \li \c{Context3D.UNSIGNED_SHORT}
 * \li \c{Context3D.FLOAT}
 * \endlist
 */
void CanvasContext::vertexAttribPointer(int indx, int size, glEnums type,
                                        bool normalized, int stride, long offset)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", size: " << size
                                         << ", type:" << glEnumToString(type)
                                         << ", normalized:" << normalized
                                         << ", stride:" << stride
                                         << ", offset:" << offset
                                         << ")";
    if (checkContextLost())
        return;

    if (!m_currentArrayBuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << " No ARRAY_BUFFER currently bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (offset < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Offset must be positive, was "
                                               << offset;
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (stride > 255) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "stride must be less than 255, was "
                                               << stride;
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    // Verify offset follows the rules of the spec
    switch (type) {
    case BYTE:
    case UNSIGNED_BYTE:
        break;
    case SHORT:
    case UNSIGNED_SHORT:
        if (offset % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "offset with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        if (stride % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "stride with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    case FLOAT:
        if (offset % 4 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "offset with FLOAT type "
                                                   << "must be multiple of 4";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        if (stride % 4 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "stride with FLOAT type must "
                                                   << "be multiple of 4";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Invalid type enumeration of "
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }


    m_commandQueue->queueCommand(CanvasGlCommandQueue::glVertexAttribPointer,
                                 GLint(indx), GLint(size),
                                 GLint(type), GLint(normalized),
                                 GLint(stride), GLint(offset));
}

bool CanvasContext::checkBufferTarget(glEnums target) {
    switch (target) {
    case ARRAY_BUFFER:
        if (!m_currentArrayBuffer) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "called with no ARRAY_BUFFER bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
        break;
    case ELEMENT_ARRAY_BUFFER:
        if (!m_currentElementArrayBuffer) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "called with no ELEMENT_ARRAY_BUFFER bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Target must be either ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return false;
    }
    return true;
}

bool CanvasContext::checkBufferUsage(CanvasContext::glEnums usage)
{
    switch (usage) {
    case STREAM_DRAW:
    case STATIC_DRAW:
    case DYNAMIC_DRAW:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Usage must be one of STREAM_DRAW, STATIC_DRAW, "
                                               << "or DYNAMIC_DRAW.";
        m_error |= CANVAS_INVALID_ENUM;
        return false;
    }
    return true;
}

/*!
 * \qmlmethod void Context3D::bufferData(glEnums target, long size, glEnums usage)
 * Sets the size of the \a target buffer to \a size. Target buffer must be either
 * \c{Context3D.ARRAY_BUFFER} or \c{Context3D.ELEMENT_ARRAY_BUFFER}. \a usage sets the usage pattern
 * of the data, and must be one of \c{Context3D.STREAM_DRAW}, \c{Context3D.STATIC_DRAW}, or
 * \c{Context3D.DYNAMIC_DRAW}.
 */
void CanvasContext::bufferData(glEnums target, long size, glEnums usage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", size:" << size
                                         << ", usage:" << glEnumToString(usage)
                                         << ")";

    if (checkBufferTarget(target) && checkBufferUsage(usage)) {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBufferData,
                                     GLint(target), GLint(size), GLint(usage));
    }
}

/*!
 * \qmlmethod void Context3D::bufferData(glEnums target, value data, glEnums usage)
 * Writes the \a data held in \c{ArrayBufferView} or \c{ArrayBuffer} to the \a target buffer.
 * Target buffer must be either \c{Context3D.ARRAY_BUFFER } or \c{Context3D.ELEMENT_ARRAY_BUFFER}.
 * \a usage sets the usage pattern of the data, and must be one of \c{Context3D.STREAM_DRAW},
 * \c{Context3D.STATIC_DRAW}, or \c{Context3D.DYNAMIC_DRAW}.
 */
void CanvasContext::bufferData(glEnums target, QJSValue data, glEnums usage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", data:" << data.toString()
                                         << ", usage:" << glEnumToString(usage)
                                         << ")";

    if (data.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE:Called with null data";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (checkBufferTarget(target) && checkBufferUsage(usage)) {
        int arrayLen = 0;
        uchar *srcData = getTypedArrayAsRawDataPtr(data, arrayLen);

        if (!srcData)
            srcData = getArrayBufferAsRawDataPtr(data, arrayLen);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_VALUE:data must be either"
                                                   << " TypedArray or ArrayBuffer";
            m_error |= CANVAS_INVALID_VALUE;
            return;
        }

        QByteArray *commandData = new QByteArray(reinterpret_cast<const char *>(srcData), arrayLen);
        GlCommand &command = m_commandQueue->queueCommand(CanvasGlCommandQueue::glBufferData,
                                                          GLint(target), GLint(commandData->size()), GLint(usage));
        command.data = commandData;
    }
}

/*!
 * \qmlmethod void Context3D::bufferSubData(glEnums target, int offset, value data)
 * Writes the \a data held in \c{ArrayBufferView} or \c{ArrayBuffer} starting from \a offset to the
 * \a target buffer. Target buffer must be either \c Context3D.ARRAY_BUFFER or
 * \c{Context3D.ELEMENT_ARRAY_BUFFER}.
 */
void CanvasContext::bufferSubData(glEnums target, int offset, QJSValue data)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", offset:"<< offset
                                         << ", data:" << data.toString()
                                         << ")";

    if (data.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE:Called with null data";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (checkBufferTarget(target)) {
        int arrayLen = 0;
        uchar *srcData = getTypedArrayAsRawDataPtr(data, arrayLen);

        if (!srcData)
            srcData = getArrayBufferAsRawDataPtr(data, arrayLen);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_VALUE:data must be either"
                                                   << " TypedArray or ArrayBuffer";
            m_error |= CANVAS_INVALID_VALUE;
            return;
        }

        QByteArray *commandData = new QByteArray(reinterpret_cast<const char *>(srcData), arrayLen);
        GlCommand &command = m_commandQueue->queueCommand(CanvasGlCommandQueue::glBufferSubData,
                                                          GLint(target), GLint(offset));
        command.data = commandData;
    }
}

/*!
 * \qmlmethod value Context3D::getBufferParameter(glEnums target, glEnums pname)
 * Returns the value for the passed \a pname of the \a target. Target
 * must be either \c Context3D.ARRAY_BUFFER or \c{Context3D.ELEMENT_ARRAY_BUFFER}. pname must be
 * either \c Context3D.BUFFER_SIZE or \c{Context3D.BUFFER_USAGE}.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getBufferParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", pname" << glEnumToString(pname)
                                         << ")";

    if (checkBufferTarget(target)) {
        switch (pname) {
        case BUFFER_SIZE: // Intentional flow through
        case BUFFER_USAGE: {
            GLint value(0);
            GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetBufferParameteriv,
                                      GLint(target), GLint(pname));
            syncCommand.returnValue = &value;
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(value);
        }
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:"
                                                   << "Pname must be either BUFFER_SIZE or BUFFER_USAGE.";
            m_error |= CANVAS_INVALID_ENUM;
            break;
        }
    }
    return QJSValue(QJSValue::NullValue);
}

/*!
 * \qmlmethod bool Context3D::isBuffer(Object anyObject)
 * Returns \c true if the given \a anyObect is a valid Canvas3DBuffer object,
 * \c false otherwise.
 * This command is handled synchronously.
 */
bool CanvasContext::isBuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasBuffer *buffer = getAsBuffer3D(anyObject);
    if (buffer && checkValidity(buffer, __FUNCTION__)) {
        GLboolean boolValue;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glIsBuffer, buffer->id());
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return boolValue;
    } else {
        return false;
    }
}

CanvasBuffer *CanvasContext::getAsBuffer3D(const QJSValue &anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasBuffer"))
        return 0;

    CanvasBuffer *buffer = static_cast<CanvasBuffer *>(anyObject.toQObject());

    if (!buffer->isAlive())
        return 0;

    return buffer;
}

/*!
 * \qmlmethod void Context3D::deleteBuffer(Canvas3DBuffer buffer3D)
 * Deletes the \a buffer3D. Has no effect if the Canvas3DBuffer object has been deleted already.
 */
void CanvasContext::deleteBuffer(QJSValue buffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(buffer:" << buffer3D.toString()
                                         << ")";
    CanvasBuffer *bufferObj = getAsBuffer3D(buffer3D);
    if (!bufferObj) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": WARNING invalid buffer target"
                                               << buffer3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (!checkValidity(bufferObj, __FUNCTION__))
        return;

    m_idToCanvasBufferMap.remove(bufferObj->id());
    bufferObj->del();
}

/*!
 * \qmlmethod glEnums Context3D::getError()
 * Returns the error value, if any.
 * This command is handled synchronously.
 */
CanvasContext::glEnums CanvasContext::getError()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;

    glEnums retVal = NO_ERROR;
    if (m_contextLost) {
        if (!m_contextLostErrorReported) {
            m_contextLostErrorReported = true;
            retVal = CONTEXT_LOST_WEBGL;
        }
    } else {
        // Fetch GL errors synchronously
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetError);
        int canvasError = CANVAS_NO_ERRORS;
        syncCommand.returnValue = &canvasError;
        scheduleSyncCommand(&syncCommand);

        m_error |= canvasError;

        if (m_error != CANVAS_NO_ERRORS) {
            // Return set error flags one by one and clear the flags.
            // Note that stack overflow/underflow flags are never returned.
            if ((m_error & CANVAS_INVALID_ENUM) != 0) {
                retVal = INVALID_ENUM;
                m_error &= ~(CANVAS_INVALID_ENUM);
            } else if ((m_error & CANVAS_INVALID_VALUE) != 0) {
                retVal = INVALID_VALUE;
                m_error &= ~(CANVAS_INVALID_VALUE);
            }else if ((m_error & CANVAS_INVALID_OPERATION) != 0) {
                retVal = INVALID_OPERATION;
                m_error &= ~(CANVAS_INVALID_OPERATION);
            } else if ((m_error & CANVAS_OUT_OF_MEMORY) != 0) {
                retVal = OUT_OF_MEMORY;
                m_error &= ~(CANVAS_OUT_OF_MEMORY);
            } else if ((m_error & CANVAS_INVALID_FRAMEBUFFER_OPERATION) != 0) {
                retVal = INVALID_FRAMEBUFFER_OPERATION;
                m_error &= ~(CANVAS_INVALID_FRAMEBUFFER_OPERATION);
            }
        }
    }

    return retVal;
}

/*!
 * \qmlmethod variant Context3D::getParameter(glEnums pname)
 * Returns the value for the given \a pname.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getParameter(glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    GLint value = 0;
    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetIntegerv, GLint(pname));
    syncCommand.returnValue = &value;

    switch (pname) {
    // GLint values
    // Intentional flow through
    case MAX_COMBINED_TEXTURE_IMAGE_UNITS:
    case MAX_FRAGMENT_UNIFORM_VECTORS:
    case MAX_RENDERBUFFER_SIZE:
    case MAX_VARYING_VECTORS:
    case MAX_VERTEX_ATTRIBS:
    case MAX_VERTEX_TEXTURE_IMAGE_UNITS:
    case PACK_ALIGNMENT:
    case SAMPLE_BUFFERS:
    case SAMPLES:
    case STENCIL_BACK_REF:
    case STENCIL_CLEAR_VALUE:
    case STENCIL_REF:
    case SUBPIXEL_BITS:
    case UNPACK_ALIGNMENT:
    case RED_BITS:
    case GREEN_BITS:
    case BLUE_BITS:
    case ALPHA_BITS:
    case DEPTH_BITS:
    case STENCIL_BITS:
    case MAX_TEXTURE_IMAGE_UNITS:
    case MAX_TEXTURE_SIZE:
    case MAX_CUBE_MAP_TEXTURE_SIZE:
    {
        scheduleSyncCommand(&syncCommand);
        return QJSValue(int(value));
    }
        // GLuint values
        // Intentional flow through
    case STENCIL_BACK_VALUE_MASK:
    case STENCIL_BACK_WRITEMASK:
    case STENCIL_VALUE_MASK:
    case STENCIL_WRITEMASK:
    {
        scheduleSyncCommand(&syncCommand);
        return QJSValue(uint(value));
    }

    case FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
        if (m_standardDerivatives) {
            scheduleSyncCommand(&syncCommand);
            return QJSValue(value);
        }
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);

    case MAX_VERTEX_UNIFORM_VECTORS: {
#if !defined(QT_OPENGL_ES_2)
        if (!m_isOpenGLES2)
            syncCommand.i1 = GLint(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
#endif
        scheduleSyncCommand(&syncCommand);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << value;
        return QJSValue(value);
    }

        // GLboolean values
        // Intentional flow through
    case BLEND:
    case CULL_FACE:
    case DEPTH_TEST:
    case DEPTH_WRITEMASK:
    case DITHER:
    case POLYGON_OFFSET_FILL:
    case SAMPLE_COVERAGE_INVERT:
    case SCISSOR_TEST:
    case STENCIL_TEST: {
        GLboolean boolValue;
        syncCommand.id = CanvasGlCommandQueue::glGetBooleanv;
        syncCommand.returnValue = &boolValue;
        scheduleSyncCommand(&syncCommand);
        return QJSValue(bool(boolValue));
    }
    case UNPACK_FLIP_Y_WEBGL:
        return QJSValue(m_unpackFlipYEnabled);
    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return QJSValue(m_unpackPremultiplyAlphaEnabled);

        // GLenum values
        // Intentional flow through
    case ACTIVE_TEXTURE:
    case BLEND_DST_ALPHA:
    case BLEND_DST_RGB:
    case BLEND_EQUATION_ALPHA:
    case BLEND_EQUATION_RGB:
    case BLEND_SRC_ALPHA:
    case BLEND_SRC_RGB:
    case CULL_FACE_MODE:
    case DEPTH_FUNC:
    case FRONT_FACE:
    case GENERATE_MIPMAP_HINT:
    case STENCIL_BACK_FAIL:
    case STENCIL_BACK_FUNC:
    case STENCIL_BACK_PASS_DEPTH_FAIL:
    case STENCIL_BACK_PASS_DEPTH_PASS:
    case STENCIL_FAIL:
    case STENCIL_FUNC:
    case STENCIL_PASS_DEPTH_FAIL:
    case STENCIL_PASS_DEPTH_PASS: {
        scheduleSyncCommand(&syncCommand);
        return QJSValue(glEnums(value));
    }

    case IMPLEMENTATION_COLOR_READ_FORMAT:
        return QJSValue(QJSValue::UndefinedValue);
    case IMPLEMENTATION_COLOR_READ_TYPE:
        return QJSValue(QJSValue::UndefinedValue);
    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return QJSValue(BROWSER_DEFAULT_WEBGL);

        // Float32Array (with 2 elements)
        // Intentional flow through
    case ALIASED_LINE_WIDTH_RANGE:
    case ALIASED_POINT_SIZE_RANGE:
    case DEPTH_RANGE: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->newArrayBuffer(sizeof(float) * 2));

        syncCommand.id = CanvasGlCommandQueue::glGetFloatv;
        syncCommand.returnValue = buffer->data();
        scheduleSyncCommand(&syncCommand);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Float32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        constructor->construct(scope, callData);
        return QJSValue(m_v4engine, scope.result.asReturnedValue());
    }

        // Float32Array (with 4 values)
        // Intentional flow through
    case BLEND_COLOR:
    case COLOR_CLEAR_VALUE: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->newArrayBuffer(sizeof(float) * 4));

        syncCommand.id = CanvasGlCommandQueue::glGetFloatv;
        syncCommand.returnValue = buffer->data();
        scheduleSyncCommand(&syncCommand);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Float32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        constructor->construct(scope, callData);
        return QJSValue(m_v4engine, scope.result.asReturnedValue());
    }

        // Int32Array (with 2 elements)
    case MAX_VIEWPORT_DIMS: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->newArrayBuffer(sizeof(int) * 2));

        syncCommand.returnValue = buffer->data();
        scheduleSyncCommand(&syncCommand);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Int32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        constructor->construct(scope, callData);
        return QJSValue(m_v4engine, scope.result.asReturnedValue());
    }
        // Int32Array (with 4 elements)
        // Intentional flow through
    case SCISSOR_BOX:
    case VIEWPORT: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->newArrayBuffer(sizeof(int) * 4));

        syncCommand.returnValue = buffer->data();
        scheduleSyncCommand(&syncCommand);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Int32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        constructor->construct(scope, callData);
        return QJSValue(m_v4engine, scope.result.asReturnedValue());
    }

        // sequence<GLboolean> (with 4 values)
        // Intentional flow through
    case COLOR_WRITEMASK: {
        GLboolean values[4];
        syncCommand.id = CanvasGlCommandQueue::glGetBooleanv;
        syncCommand.returnValue = values;
        scheduleSyncCommand(&syncCommand);

        QJSValue arrayValue = m_engine->newArray(4);
        arrayValue.setProperty(0, bool(values[0]));
        arrayValue.setProperty(1, bool(values[1]));
        arrayValue.setProperty(2, bool(values[2]));
        arrayValue.setProperty(3, bool(values[3]));
        return arrayValue;
    }

        // GLfloat values
        // Intentional flow through
    case DEPTH_CLEAR_VALUE:
    case LINE_WIDTH:
    case POLYGON_OFFSET_FACTOR:
    case POLYGON_OFFSET_UNITS:
    case SAMPLE_COVERAGE_VALUE: {
        GLfloat floatValue;
        syncCommand.id = CanvasGlCommandQueue::glGetFloatv;
        syncCommand.returnValue = &floatValue;
        scheduleSyncCommand(&syncCommand);
        return QJSValue(float(floatValue));
    }
        // DomString values
    case VENDOR:
        return QJSValue(QStringLiteral("The Qt Company"));
    case RENDERER:
        return QJSValue(QStringLiteral("Qt Canvas3D Renderer"));
    case SHADING_LANGUAGE_VERSION: // Intentional flow through
    case VERSION: {
        syncCommand.id = CanvasGlCommandQueue::glGetString;
        scheduleSyncCommand(&syncCommand);
        const char *text = reinterpret_cast<char *>(syncCommand.returnValue);
        QString qtext = QString::fromLatin1(text);
        if (pname == SHADING_LANGUAGE_VERSION)
            qtext.prepend(QStringLiteral("WebGL GLSL ES 1.0 - Qt Canvas3D (OpenGL: "));
        else // VERSION
            qtext.prepend(QStringLiteral("WebGL 1.0 - Qt Canvas3D (OpenGL: "));
        qtext.append(QStringLiteral(")"));
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << qtext;
        return QJSValue(qtext);
    }
    case UNMASKED_VENDOR_WEBGL:
        // Intentional flow through
    case UNMASKED_RENDERER_WEBGL: {
        syncCommand.i1 = GLint(GL_VENDOR);
        syncCommand.id = CanvasGlCommandQueue::glGetString;
        scheduleSyncCommand(&syncCommand);
        const char *text = reinterpret_cast<char *>(syncCommand.returnValue);

        QString qtext = QString::fromLatin1((const char *)text);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << qtext;
        return QJSValue(qtext);
    }
    case COMPRESSED_TEXTURE_FORMATS: {
        syncCommand.i1 = GLint(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
        scheduleSyncCommand(&syncCommand);
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->newArrayBuffer(sizeof(int) * value));
        if (value > 0) {
            syncCommand.i1 = GLint(pname);
            syncCommand.returnValue = buffer->data();
            scheduleSyncCommand(&syncCommand);
        }
        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::UInt32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        constructor->construct(scope, callData);
        return QJSValue(m_v4engine, scope.result.asReturnedValue());
    }
    case FRAMEBUFFER_BINDING: {
        return m_engine->newQObject(m_currentFramebuffer);
    }
    case RENDERBUFFER_BINDING: {
        return m_engine->newQObject(m_currentRenderbuffer);
    }
    case CURRENT_PROGRAM: {
        return m_engine->newQObject(m_currentProgram);
    }
    case ELEMENT_ARRAY_BUFFER_BINDING: {
        return m_engine->newQObject(m_currentElementArrayBuffer);
    }
    case ARRAY_BUFFER_BINDING: {
        return m_engine->newQObject(m_currentArrayBuffer);
    }
    case TEXTURE_BINDING_2D: {
        return m_engine->newQObject(m_currentTexture2D);
    }
    case TEXTURE_BINDING_CUBE_MAP: {
        return m_engine->newQObject(m_currentTextureCubeMap);
    }
    default: {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): UNIMPLEMENTED PARAMETER NAME"
                                               << glEnumToString(pname);
        return QJSValue(QJSValue::NullValue);
    }
    }

    return QJSValue(QJSValue::NullValue);
}

/*!
 * \qmlmethod string Context3D::getShaderInfoLog(Canvas3DShader shader)
 * Returns the info log string of the given \a shader.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getShaderInfoLog(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader3D:" << shader3D.toString()
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING: invalid shader handle:"
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkValidity(shader, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    QString log;

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetShaderInfoLog, shader->id());
    syncCommand.returnValue = &log;
    scheduleSyncCommand(&syncCommand);
    if (syncCommand.glError)
        return QJSValue(QJSValue::NullValue);
    else
        return QJSValue(log);
}

/*!
 * \qmlmethod string Context3D::getProgramInfoLog(Canvas3DProgram program3D)
 * Returns the info log string of the given \a program3D.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getProgramInfoLog(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";
    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING: invalid program handle:"
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkValidity(program, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    QString log;

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetProgramInfoLog, program->id());
    syncCommand.returnValue = &log;
    scheduleSyncCommand(&syncCommand);
    if (syncCommand.glError)
        return QJSValue(QJSValue::NullValue);
    else
        return QJSValue(log);
}

/*!
 * \qmlmethod void Context3D::bindBuffer(glEnums target, Canvas3DBuffer buffer3D)
 * Binds the given \a buffer3D to the given \a target. Target must be either
 * \c Context3D.ARRAY_BUFFER or \c{Context3D.ELEMENT_ARRAY_BUFFER}. If the \a buffer3D given is
 * \c{null}, the current buffer bound to the target is unbound.
 */
void CanvasContext::bindBuffer(glEnums target, QJSValue buffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", buffer:" << buffer3D.toString()
                                         << ")";

    if (target != ARRAY_BUFFER && target != ELEMENT_ARRAY_BUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:target must be either "
                                               << "ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasBuffer *buffer = getAsBuffer3D(buffer3D);
    if (buffer && checkValidity(buffer, __FUNCTION__)) {
        if (target == ARRAY_BUFFER) {
            if (buffer->target() == CanvasBuffer::UNINITIALIZED)
                buffer->setTarget(CanvasBuffer::ARRAY_BUFFER);

            if (buffer->target() != CanvasBuffer::ARRAY_BUFFER) {
                qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                       << ":INVALID_OPERATION:can't rebind "
                                                       << "ELEMENT_ARRAY_BUFFER as ARRAY_BUFFER";
                m_error |= CANVAS_INVALID_OPERATION;
                return;
            }
            m_currentArrayBuffer = buffer;
        } else {
            if (buffer->target() == CanvasBuffer::UNINITIALIZED)
                buffer->setTarget(CanvasBuffer::ELEMENT_ARRAY_BUFFER);

            if (buffer->target() != CanvasBuffer::ELEMENT_ARRAY_BUFFER) {
                qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                       << ":INVALID_OPERATION:can't rebind "
                                                       << "ARRAY_BUFFER as ELEMENT_ARRAY_BUFFER";
                m_error |= CANVAS_INVALID_OPERATION;
                return;
            }
            m_currentElementArrayBuffer = buffer;
        }
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindBuffer, GLint(target),
                                     buffer->id());
    } else {
        m_commandQueue->queueCommand(CanvasGlCommandQueue::glBindBuffer, GLint(target), GLint(0));
    }
}

/*!
 * \qmlmethod void Context3D::validateProgram(Canvas3DProgram program3D)
 * Validates the given \a program3D. The validation status is stored into the state of the shader
 * program container in \a program3D and can be queried using \c{Context3D.VALIDATE_STATUS}
 * as \c{paramName} for \c{Context3D.getProgramParameter()} method. Additional information may
 * be stored into the program's information log.
 *
 * \sa getProgramInfoLog
 */
void CanvasContext::validateProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString() << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (checkValidity(program, __FUNCTION__))
        program->validateProgram();
}

/*!
 * \qmlmethod void Context3D::useProgram(Canvas3DProgram program)
 * Installs the given \a program3D as a part of the current rendering state.
 */
void CanvasContext::useProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString() << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    m_currentProgram = program;

    if (!program) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkValidity(program, __FUNCTION__))
        return;
    program->useProgram();
}

/*!
 * \qmlmethod void Context3D::clear(glEnums flags)
 * Clears the given \a flags.
 */
void CanvasContext::clear(glEnums flags)
{
    if (!canvas3drendering().isDebugEnabled()) {
        QString flagStr;
        if (flags & COLOR_BUFFER_BIT)
            flagStr.append(" COLOR_BUFFER_BIT ");

        if (flags & DEPTH_BUFFER_BIT)
            flagStr.append(" DEPTH_BUFFER_BIT ");

        if (flags & STENCIL_BUFFER_BIT)
            flagStr.append(" STENCIL_BUFFER_BIT ");

        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(flags:" << flagStr << ")";
    }

    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glClear, GLint(flags));

    // Set clear flags if the clear targets default framebuffer
    if (!m_currentFramebuffer)
        m_commandQueue->removeFromClearMask(GLbitfield(flags));
}

/*!
 * \qmlmethod void Context3D::cullFace(glEnums mode)
 * Sets the culling to \a mode. Must be one of \c{Context3D.FRONT}, \c{Context3D.BACK},
 * or \c{Context3D.FRONT_AND_BACK}. Defaults to \c{Context3D.BACK}.
 */
void CanvasContext::cullFace(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode) << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glCullFace, GLint(mode));
}

/*!
 * \qmlmethod void Context3D::frontFace(glEnums mode)
 * Sets the front face drawing to \a mode. Must be either \c Context3D.CW
 * or \c{Context3D.CCW}. Defaults to \c{Context3D.CCW}.
 */
void CanvasContext::frontFace(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode) << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glFrontFace, GLint(mode));
}

/*!
 * \qmlmethod void Context3D::depthMask(bool flag)
 * Enables or disables the depth mask based on \a flag. Defaults to \c{true}.
 */
void CanvasContext::depthMask(bool flag)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(flag:" << flag << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDepthMask, GLint(flag));
}

/*!
 * \qmlmethod void Context3D::depthFunc(glEnums func)
 * Sets the depth function to \a func. Must be one of \c{Context3D.NEVER}, \c{Context3D.LESS},
 * \c{Context3D.EQUAL}, \c{Context3D.LEQUAL}, \c{Context3D.GREATER}, \c{Context3D.NOTEQUAL},
 * \c{Context3D.GEQUAL}, or \c{Context3D.ALWAYS}. Defaults to \c{Context3D.LESS}.
 */
void CanvasContext::depthFunc(glEnums func)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(func:" << glEnumToString(func) << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDepthFunc, GLint(func));
}

/*!
 * \qmlmethod void Context3D::depthRange(float zNear, float zFar)
 * Sets the depth range between \a zNear and \a zFar. Values are clamped to \c{[0, 1]}. \a zNear
 * must be less or equal to \a zFar. zNear Range defaults to \c{[0, 1]}.
 */
void CanvasContext::depthRange(float zNear, float zFar)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(zNear:" << zNear
                                         << ", zFar:" << zFar <<  ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDepthRangef,
                                 GLfloat(zNear), GLfloat(zFar));
}

/*!
 * \qmlmethod void Context3D::clearStencil(int stencil)
 * Sets the clear value for the stencil buffer to \a stencil. Defaults to \c{0}.
 */
void CanvasContext::clearStencil(int stencil)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(stencil:" << stencil << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glClearStencil, GLint(stencil));
}

/*!
 * \qmlmethod void Context3D::colorMask(bool maskRed, bool maskGreen, bool maskBlue, bool maskAlpha)
 * Enables or disables the writing of colors to the frame buffer based on \a maskRed, \a maskGreen,
 * \a maskBlue and \a maskAlpha. All default to \c{true}.
 */
void CanvasContext::colorMask(bool maskRed, bool maskGreen, bool maskBlue, bool maskAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(maskRed:" << maskRed
                                         << ", maskGreen:" << maskGreen
                                         << ", maskBlue:" << maskBlue
                                         << ", maskAlpha:" << maskAlpha  <<  ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glColorMask,
                                 GLint(maskRed), GLint(maskGreen),
                                 GLint(maskBlue), GLint(maskAlpha));
}

/*!
 * \qmlmethod void Context3D::clearDepth(float depth)
 * Sets the clear value for the depth buffer to \a depth. Must be between \c{[0, 1]}. Defaults
 * to \c{1}.
 */
void CanvasContext::clearDepth(float depth)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(depth:" << depth << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glClearDepthf, GLfloat(depth));
}

/*!
 * \qmlmethod void Context3D::clearColor(float red, float green, float blue, float alpha)
 * Sets the clear values for the color buffers with \a red, \a green, \a blue and \a alpha. Values
 * must be between \c{[0, 1]}. All default to \c{0}.
 */
void CanvasContext::clearColor(float red, float green, float blue, float alpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(red:" << red
                                          << ", green:" << green
                                          << ", blue:" << blue
                                          << ", alpha:" << alpha << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glClearColor,
                                 GLfloat(red), GLfloat(green),
                                 GLfloat(blue), GLfloat(alpha));
}

/*!
 * \qmlmethod Context3D::viewport(int x, int y, int width, int height)
 * Defines the affine transformation from normalized x and y device coordinates to window
 * coordinates within the drawing buffer.
 * \a x defines the left edge of the viewport.
 * \a y defines the bottom edge of the viewport.
 * \a width defines the width of the viewport.
 * \a height defines the height of the viewport.
 */
void CanvasContext::viewport(int x, int y, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(x:" << x
                                          << ", y:" << y
                                          << ", width:" << width
                                          << ", height:" << height << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glViewport, GLint(x), GLint(y),
                                 GLint(width), GLint(height));
}

/*!
 * \qmlmethod void Context3D::drawArrays(glEnums mode, int first, int count)
 * Renders the geometric primitives held in the currently bound array buffer starting from \a first
 * up to \a count using \a mode for drawing. Mode can be one of:
 * \list
 * \li \c{Context3D.POINTS}
 * \li \c{Context3D.LINES}
 * \li \c{Context3D.LINE_LOOP}
 * \li \c{Context3D.LINE_STRIP}
 * \li \c{Context3D.TRIANGLES}
 * \li \c{Context3D.TRIANGLE_STRIP}
 * \li \c{Context3D.TRIANGLE_FAN}
 * \endlist
 */
void CanvasContext::drawArrays(glEnums mode, int first, int count)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ", first:" << first
                                         << ", count:" << count << ")";
    if (checkContextLost())
        return;

    if (first < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE: first is negative.";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (count < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE: count is negative.";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDrawArrays,
                                 GLint(mode), GLint(first), GLint(count));
}

/*!
 * \qmlmethod void Context3D::drawElements(glEnums mode, int count, glEnums type, long offset)
 * Renders the number of geometric elements given in \a count held in the currently bound element
 * array buffer using \a mode for drawing. Mode can be one of:
 * \list
 * \li \c{Context3D.POINTS}
 * \li \c{Context3D.LINES}
 * \li \c{Context3D.LINE_LOOP}
 * \li \c{Context3D.LINE_STRIP}
 * \li \c{Context3D.TRIANGLES}
 * \li \c{Context3D.TRIANGLE_STRIP}
 * \li \c{Context3D.TRIANGLE_FAN}
 * \endlist
 *
 * \a type specifies the element type and can be one of:
 * \list
 * \li \c Context3D.UNSIGNED_BYTE
 * \li \c{Context3D.UNSIGNED_SHORT}
 * \endlist
 *
 * \a offset specifies the location3D where indices are stored.
 */
void CanvasContext::drawElements(glEnums mode, int count, glEnums type, long offset)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ", count:" << count
                                         << ", type:" << glEnumToString(type)
                                         << ", offset:" << offset << ")";
    if (!m_currentElementArrayBuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION: "
                                               << "No ELEMENT_ARRAY_BUFFER currently bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Verify offset follows the rules of the spec
    switch (type) {
    case UNSIGNED_SHORT:
        if (offset % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION: "
                                                   << "Offset with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
    case UNSIGNED_BYTE:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM: "
                                               << "Invalid type enumeration of "
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (count < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE: count is negative.";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glDrawElements,
                                 GLint(mode), GLint(count),
                                 GLint(type), GLint(offset));
}

/*!
 * \qmlmethod void Context3D::readPixels(int x, int y, long width, long height, glEnums format, glEnums type, ArrayBufferView pixels)
 * Returns the pixel data in the rectangle specified by \a x, \a y, \a width and \a height of the
 * frame buffer in \a pixels using \a format (must be \c{Context3D.RGBA}) and \a type
 * (must be \c{Context3D.UNSIGNED_BYTE}).
 * This command is handled synchronously.
 */
void CanvasContext::readPixels(int x, int y, long width, long height, glEnums format, glEnums type,
                               QJSValue pixels)
{
    if (checkContextLost())
        return;

    if (format != RGBA) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:format must be RGBA.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (type != UNSIGNED_BYTE) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:type must be UNSIGNED_BYTE.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels was null.";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *bufferPtr = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);
    if (!bufferPtr) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:pixels must be Uint8Array.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Zero out the buffer (WebGL conformance requires pixels outside the framebuffer to be 0)
    memset(bufferPtr, 0, width * height * 4);

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glReadPixels, GLint(x), GLint(y),
                              GLint(width), GLint(height), GLint(format), GLint(type));
    syncCommand.returnValue = bufferPtr;
    scheduleSyncCommand(&syncCommand);
}

/*!
 * \qmlmethod Canvas3DActiveInfo Context3D::getActiveAttrib(Canvas3DProgram program3D, uint index)
 * Returns information about the given active attribute variable defined by \a index for the given
 * \a program3D.
 * This command is handled synchronously.
 * \sa Canvas3DActiveInfo
 */
CanvasActiveInfo *CanvasContext::getActiveAttrib(QJSValue program3D, uint index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program || !checkValidity(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return 0;
    }

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetActiveAttrib,
                              program->id(), GLint(index), GLint(maxUniformAttributeNameLen));
    // glGetActiveAttrib returns three integers and one string, reserve space for all in retval.
    GLint retval[3 + (maxUniformAttributeNameLen / sizeof(GLint))];
    memset(retval, 0, sizeof(retval));
    syncCommand.returnValue = retval;

    scheduleSyncCommand(&syncCommand);
    if (syncCommand.glError) {
        return 0;
    } else {
        QString strName(reinterpret_cast<char *>(&retval[3]));
        return new CanvasActiveInfo(retval[1], CanvasContext::glEnums(retval[2]), strName);
    }
}

/*!
 * \qmlmethod Canvas3DActiveInfo Context3D::getActiveUniform(Canvas3DProgram program3D, uint index)
 * Returns information about the given active uniform variable defined by \a index for the given
 * \a program3D.
 * This command is handled synchronously.
 * \sa Canvas3DActiveInfo
 */
CanvasActiveInfo *CanvasContext::getActiveUniform(QJSValue program3D, uint index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program || !checkValidity(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return 0;
    }

    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetActiveUniform,
                              program->id(), GLint(index), GLint(maxUniformAttributeNameLen));
    // glGetActiveUniform returns three integers and one string, reserve space for all in retval.
    GLint retval[3 + (maxUniformAttributeNameLen / sizeof(GLint))];
    memset(retval, 0, sizeof(retval));
    syncCommand.returnValue = retval;

    scheduleSyncCommand(&syncCommand);
    if (syncCommand.glError) {
        return 0;
    } else {
        QString strName(reinterpret_cast<char *>(&retval[3]));
        return new CanvasActiveInfo(retval[1], CanvasContext::glEnums(retval[2]), strName);
    }
}

/*!
 * \qmlmethod void Context3D::stencilFunc(glEnums func, int ref, uint mask)
 * Sets front and back function \a func and reference value \a ref for stencil testing.
 * Also sets the \a mask value that is ANDed with both reference and stored stencil value when
 * the test is done. \a func is initially set to \c{Context3D.ALWAYS} and can be one of:
 * \list
 * \li \c{Context3D.NEVER}
 * \li \c{Context3D.LESS}
 * \li \c{Context3D.LEQUAL}
 * \li \c{Context3D.GREATER}
 * \li \c{Context3D.GEQUAL}
 * \li \c{Context3D.EQUAL}
 * \li \c{Context3D.NOTEQUAL}
 * \li \c{Context3D.ALWAYS}
 * \endlist
 */
void CanvasContext::stencilFunc(glEnums func, int ref, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(func:" <<  glEnumToString(func)
                                         << ", ref:" << ref
                                         << ", mask:" << mask
                                         << ")";
    if (checkContextLost())
        return;

    // Clamp ref
    if (ref < 0)
        ref = 0;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilFunc,
                                 GLint(func), GLint(ref), GLint(mask));
}

/*!
 * \qmlmethod void Context3D::stencilFuncSeparate(glEnums face, glEnums func, int ref, uint mask)
 * Sets front and/or back (defined by \a face) function \a func and reference value \a ref for
 * stencil testing. Also sets the \a mask value that is ANDed with both reference and stored stencil
 * value when the test is done. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 * \a func is initially set to \c{Context3D.ALWAYS} and can be one of:
 * \list
 * \li \c{Context3D.NEVER}
 * \li \c{Context3D.LESS}
 * \li \c{Context3D.LEQUAL}
 * \li \c{Context3D.GREATER}
 * \li \c{Context3D.GEQUAL}
 * \li \c{Context3D.EQUAL}
 * \li \c{Context3D.NOTEQUAL}
 * \li \c{Context3D.ALWAYS}
 * \endlist
 */
void CanvasContext::stencilFuncSeparate(glEnums face, glEnums func, int ref, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", func:" <<  glEnumToString(func)
                                         << ", ref:" << ref
                                         << ", mask:" << mask
                                         << ")";
    if (checkContextLost())
        return;

    // Clamp ref
    if (ref < 0)
        ref = 0;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilFuncSeparate,
                                 GLint(face), GLint(func), GLint(ref), GLint(mask));
}

/*!
 * \qmlmethod void Context3D::stencilMask(uint mask)
 * Controls the front and back writing of individual bits in the stencil planes. \a mask defines
 * the bit mask to enable and disable writing of individual bits in the stencil planes.
 */
void CanvasContext::stencilMask(uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mask:" << mask
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilMask, GLint(mask));
}

/*!
 * \qmlmethod void Context3D::stencilMaskSeparate(glEnums face, uint mask)
 * Controls the front and/or back writing (defined by \a face) of individual bits in the stencil
 * planes. \a mask defines the bit mask to enable and disable writing of individual bits in the
 * stencil planes. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 */
void CanvasContext::stencilMaskSeparate(glEnums face, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", mask:" << mask
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilMaskSeparate,
                                 GLint(face), GLint(mask));
}

/*!
 * \qmlmethod void Context3D::stencilOp(glEnums sfail, glEnums zfail, glEnums zpass)
 * Sets the front and back stencil test actions for failing the test \a zfail and passing the test
 * \a zpass. \a sfail, \a zfail and \a zpass are initially set to \c{Context3D.KEEP} and can be one
 * of:
 * \list
 * \li \c{Context3D.KEEP}
 * \li \c{Context3D.ZERO}
 * \li \c{Context3D.GL_REPLACE}
 * \li \c{Context3D.GL_INCR}
 * \li \c{Context3D.GL_INCR_WRAP}
 * \li \c{Context3D.GL_DECR}
 * \li \c{Context3D.GL_DECR_WRAP}
 * \li \c{Context3D.GL_INVERT}
 * \endlist
 */
void CanvasContext::stencilOp(glEnums sfail, glEnums zfail, glEnums zpass)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(sfail:" <<  glEnumToString(sfail)
                                         << ", zfail:" <<  glEnumToString(zfail)
                                         << ", zpass:" << glEnumToString(zpass)
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilOp,
                                 GLint(sfail), GLint(zfail), GLint(zpass));
}

/*!
 * \qmlmethod void Context3D::stencilOpSeparate(glEnums face, glEnums fail, glEnums zfail, glEnums zpass)
 * Sets the front and/or back (defined by \a face) stencil test actions for failing the test
 * \a zfail and passing the test \a zpass. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 *
 * \a sfail, \a zfail and \a zpass are initially set to \c{Context3D.KEEP} and can be one of:
 * \list
 * \li \c{Context3D.KEEP}
 * \li \c{Context3D.ZERO}
 * \li \c{Context3D.GL_REPLACE}
 * \li \c{Context3D.GL_INCR}
 * \li \c{Context3D.GL_INCR_WRAP}
 * \li \c{Context3D.GL_DECR}
 * \li \c{Context3D.GL_DECR_WRAP}
 * \li \c{Context3D.GL_INVERT}
 * \endlist
 */
void CanvasContext::stencilOpSeparate(glEnums face, glEnums fail, glEnums zfail, glEnums zpass)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", fail:" <<  glEnumToString(fail)
                                         << ", zfail:" <<  glEnumToString(zfail)
                                         << ", zpass:" << glEnumToString(zpass)
                                         << ")";
    if (checkContextLost())
        return;

    m_commandQueue->queueCommand(CanvasGlCommandQueue::glStencilOpSeparate,
                                 GLint(face), GLint(fail), GLint(zfail), GLint(zpass));
}

/*!
 * \qmlmethod int Context3D::getFramebufferAttachmentParameter(glEnums target, glEnums attachment, glEnums pname)
 * Returns information specified by \a pname about given \a attachment of a framebuffer object
 * bound to the given \a target.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getFramebufferAttachmentParameter(glEnums target, glEnums attachment,
                                                          glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", attachment:" << glEnumToString(attachment)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Target parameter must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);;
    }

    switch (attachment) {
    case COLOR_ATTACHMENT0:
    case DEPTH_ATTACHMENT:
    case STENCIL_ATTACHMENT:
    case DEPTH_STENCIL_ATTACHMENT:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "attachment parameter is invalid";
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);;
    }

    GLint parameter;
    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetFramebufferAttachmentParameteriv,
                              GLint(target), GLint(attachment), GLint(pname));
    syncCommand.returnValue = &parameter;
    scheduleSyncCommand(&syncCommand);

    if (syncCommand.glError) {
        return QJSValue(QJSValue::NullValue);
    } else {
        switch (pname) {
        case FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return QJSValue(glEnums(parameter));
        case FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: {
            QJSValue retval;
            // Check FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, and choose the type based on it
            syncCommand.i3 = GLint(FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                return QJSValue(QJSValue::NullValue);
            } else {
                if (parameter == TEXTURE)
                    retval = m_engine->newQObject(m_currentFramebuffer->texture());
                else
                    retval = m_engine->newQObject(m_currentRenderbuffer);
                return retval;
            }
        }
        case FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            return QJSValue(parameter);
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:invalid pname "
                                                   << glEnumToString(pname);
            m_error |= CANVAS_INVALID_ENUM;
            return QJSValue(QJSValue::NullValue);
        }
    }
}

/*!
 * \qmlmethod int Context3D::getRenderbufferParameter(glEnums target, glEnums pname)
 * Returns information specified by \a pname of a renderbuffer object
 * bound to the given \a target.
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getRenderbufferParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    if (target != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Target parameter must be RENDERBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);;
    }

    GLint parameter;
    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetRenderbufferParameteriv,
                              GLint(target), GLint(pname));
    syncCommand.returnValue = &parameter;
    scheduleSyncCommand(&syncCommand);

    if (syncCommand.glError) {
        return QJSValue(QJSValue::NullValue);
    } else {
        switch (pname) {
        case RENDERBUFFER_INTERNAL_FORMAT:
            return QJSValue(glEnums(parameter));
        case RENDERBUFFER_WIDTH:
        case RENDERBUFFER_HEIGHT:
        case RENDERBUFFER_RED_SIZE:
        case RENDERBUFFER_GREEN_SIZE:
        case RENDERBUFFER_BLUE_SIZE:
        case RENDERBUFFER_ALPHA_SIZE:
        case RENDERBUFFER_DEPTH_SIZE:
        case RENDERBUFFER_STENCIL_SIZE:
            return QJSValue(parameter);
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:invalid pname "
                                                   << glEnumToString(pname);
            m_error |= CANVAS_INVALID_ENUM;
            return QJSValue(QJSValue::NullValue);
        }
    }
}

/*!
 * \qmlmethod variant Context3D::getTexParameter(glEnums target, glEnums pname)
 * Returns parameter specified by the \a pname of a texture object
 * bound to the given \a target. \a pname must be one of:
 * \list
 * \li \c{Context3D.TEXTURE_MAG_FILTER}
 * \li \c{Context3D.TEXTURE_MIN_FILTER}
 * \li \c{Context3D.TEXTURE_WRAP_S}
 * \li \c{Context3D.TEXTURE_WRAP_T}
 * \endlist
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getTexParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    GLint parameter = 0;
    if (isValidTextureBound(target, __FUNCTION__, false)) {
        switch (pname) {
        case TEXTURE_MAG_FILTER:
        case TEXTURE_MIN_FILTER:
        case TEXTURE_WRAP_S:
        case TEXTURE_WRAP_T: {
            GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetTexParameteriv,
                                      GLint(target), GLint(pname));
            syncCommand.returnValue = &parameter;
            scheduleSyncCommand(&syncCommand);

            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(parameter);
        }
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:invalid pname "
                                                   << glEnumToString(pname)
                                                   << " must be one of: TEXTURE_MAG_FILTER, "
                                                   << "TEXTURE_MIN_FILTER, TEXTURE_WRAP_S"
                                                   << " or TEXTURE_WRAP_T";
            m_error |= CANVAS_INVALID_ENUM;
            return QJSValue(QJSValue::NullValue);
        }
    }
    return QJSValue(QJSValue::NullValue);
}


/*!
 * \qmlmethod int Context3D::getVertexAttribOffset(int index, glEnums pname)
 * Returns the offset of the specified generic vertex attribute pointer \a index. \a pname must be
 * \c{Context3D.VERTEX_ATTRIB_ARRAY_POINTER}
 * \list
 * \li \c{Context3D.TEXTURE_MAG_FILTER}
 * \li \c{Context3D.TEXTURE_MIN_FILTER}
 * \li \c{Context3D.TEXTURE_WRAP_S}
 * \li \c{Context3D.TEXTURE_WRAP_T}
 * \endlist
 * This command is handled synchronously.
 */
uint CanvasContext::getVertexAttribOffset(uint index, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index" << index
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return 0;

    if (pname != VERTEX_ATTRIB_ARRAY_POINTER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:pname must be "
                                               << "VERTEX_ATTRIB_ARRAY_POINTER";
        m_error |= CANVAS_INVALID_ENUM;
        return 0;
    }

    if (index >= m_maxVertexAttribs) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:index must be smaller than "
                                               << m_maxVertexAttribs;
        m_error |= CANVAS_INVALID_VALUE;
        return 0;
    }

    GLuint offset = 0;
    GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetVertexAttribPointerv,
                              GLint(index), GLint(pname));
    syncCommand.returnValue = &offset;
    scheduleSyncCommand(&syncCommand);

    return uint(offset);
}

/*!
 * \qmlmethod variant Context3D::getVertexAttrib(int index, glEnums pname)
 * Returns the requested parameter \a pname of the specified generic vertex attribute pointer
 * \a index. The type returned is dependent on the requested \a pname, as shown in the table:
 * \table
 * \header
 *   \li pname
 *   \li Returned Type
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING}
 *   \li \c{Canvas3DBuffer}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_ENABLED}
 *   \li \c{boolean}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_SIZE}
 *   \li \c{int}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_STRIDE}
 *   \li \c{int}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_TYPE}
 *   \li \c{glEnums}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_NORMALIZED}
 *   \li \c{boolean}
 * \row
 *   \li \c{Context3D.CURRENT_VERTEX_ATTRIB}
 *   \li \c{Float32Array} (with 4 elements)
 *  \endtable
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getVertexAttrib(uint index, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index" << index
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    if (index >= MAX_VERTEX_ATTRIBS) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:index must be smaller than "
                                               << "MAX_VERTEX_ATTRIBS = " << MAX_VERTEX_ATTRIBS;
        m_error |= CANVAS_INVALID_VALUE;
    } else {
        GLint value = 0;
        GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetVertexAttribiv,
                                  GLint(index), GLint(pname));
        syncCommand.returnValue = &value;

        switch (pname) {
        case VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError || value == 0 || !m_idToCanvasBufferMap.contains(value))
                return QJSValue(QJSValue::NullValue);

            return m_engine->newQObject(m_idToCanvasBufferMap.value(value));
        }
        case VERTEX_ATTRIB_ARRAY_ENABLED:
            // Intentional flow through
        case VERTEX_ATTRIB_ARRAY_NORMALIZED: {
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(bool(value));
        }
        case VERTEX_ATTRIB_ARRAY_SIZE:
            // Intentional flow through
        case VERTEX_ATTRIB_ARRAY_STRIDE:
        case VERTEX_ATTRIB_ARRAY_TYPE: {
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(value);
        }
        case CURRENT_VERTEX_ATTRIB: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->newArrayBuffer(sizeof(float) * 4));

            syncCommand.id = CanvasGlCommandQueue::glGetVertexAttribfv;
            syncCommand.returnValue = buffer->data();
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                return QJSValue(QJSValue::NullValue);
            } else {
                QV4::ScopedFunctionObject constructor(scope,
                                                      m_v4engine->typedArrayCtors[
                                                      QV4::Heap::TypedArray::Float32Array]);
                QV4::ScopedCallData callData(scope, 1);
                callData->args[0] = buffer;
                constructor->construct(scope, callData);
                return QJSValue(m_v4engine, scope.result.asReturnedValue());
            }
        }
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:pname " << pname;
            m_error |= CANVAS_INVALID_ENUM;
        }
    }

    return QJSValue(QJSValue::NullValue);
}

/*!
 * Implements CanvasTextureProvider::createTextureFromSource() extension functionality
 */
QJSValue CanvasContext::createTextureFromSource(QQuickItem *item)
{
    if (checkContextLost())
        return QJSValue(QJSValue::NullValue);

    // First check if we have a CanvasTexture already for this item
    CanvasTexture *texture = m_quickItemToTextureMap.value(item, 0);
    if (!texture) {
        texture = new CanvasTexture(m_commandQueue, this, item);
        addObjectToValidList(texture);
    }

    m_quickItemToTextureMap.insert(item, texture);

    QJSValue value = m_engine->newQObject(texture);

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(quickItem:" << item
                                         << "):" << value.toString();

    // We attempt to add item as texture again even if it is already created to make sure the
    // provider is cached. This allows user to fix the texture after e.g. disabling and enabling
    // a layer, which destroys and recreates the texture provider.
    m_commandQueue->addQuickItemAsTexture(item, texture->textureId());

    return value;
}

QMap<QQuickItem *, CanvasTexture *> &CanvasContext::quickItemToTextureMap()
{
    return m_quickItemToTextureMap;
}

/*!
 * \qmlmethod variant Context3D::getUniform(Canvas3DProgram program, Canvas3DUniformLocation location3D)
 * Returns the uniform value at the given \a location3D in the \a program.
 * The type returned is dependent on the uniform type, as shown in the table:
 * \table
 * \header
 *   \li Uniform Type
 *   \li Returned Type
 * \row
 *   \li boolean
 *   \li boolean
 * \row
 *   \li int
 *   \li int
 * \row
 *   \li float
 *   \li float
 * \row
 *   \li vec2
 *   \li Float32Array (with 2 elements)
 * \row
 *   \li vec3
 *   \li Float32Array (with 3 elements)
 * \row
 *   \li vec4
 *   \li Float32Array (with 4 elements)
 * \row
 *   \li ivec2
 *   \li Int32Array (with 2 elements)
 * \row
 *   \li ivec3
 *   \li Int32Array (with 3 elements)
 * \row
 *   \li ivec4
 *   \li Int32Array (with 4 elements)
 * \row
 *   \li bvec2
 *   \li sequence<boolean> (with 2 elements)
 * \row
 *   \li bvec3
 *   \li sequence<boolean> (with 3 elements)
 * \row
 *   \li bvec4
 *   \li sequence<boolean> (with 4 elements)
 * \row
 *   \li mat2
 *   \li Float32Array (with 4 elements)
 * \row
 *   \li mat3
 *   \li Float32Array (with 9 elements)
 * \row
 *   \li mat4
 *   \li Float32Array (with 16 elements)
 * \row
 *   \li sampler2D
 *   \li int
 * \row
 *   \li samplerCube
 *   \li int
 *  \endtable
 * This command is handled synchronously.
 */
QJSValue CanvasContext::getUniform(QJSValue program3D, QJSValue location3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program" << program3D.toString()
                                         << ", location3D:" << location3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasUniformLocation *location = getAsUniformLocation3D(location3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:No program was specified";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);
    } else if (!location) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:No location3D was specified";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);
    } else if (!checkValidity(program, __FUNCTION__) || !checkValidity(location, __FUNCTION__)) {
        return QJSValue(QJSValue::UndefinedValue);
    }

    location->resolveType(program->id(), this);
    const GLint locationId = location->id();
    const GLint type = location->type();

    if (type < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:Uniform type could not be determined";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);
    } else {
        int numValues = 4;

        GlSyncCommand syncCommand(CanvasGlCommandQueue::glGetUniformiv, program->id(),
                                  locationId);
        switch (type) {
        case SAMPLER_2D:
            // Intentional flow through
        case SAMPLER_CUBE:
            // Intentional flow through
        case INT: {
            GLint value = 0;
            syncCommand.returnValue = &value;
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(value);
        }
        case FLOAT: {
            GLfloat value = 0;
            syncCommand.id = CanvasGlCommandQueue::glGetUniformfv;
            syncCommand.returnValue = &value;
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(value);
        }
        case BOOL: {
            GLint value = 0;
            syncCommand.returnValue = &value;
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError)
                return QJSValue(QJSValue::NullValue);
            else
                return QJSValue(bool(value));
        }
        case INT_VEC2:
            numValues--;
            // Intentional flow through
        case INT_VEC3:
            numValues--;
            // Intentional flow through
        case INT_VEC4: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->newArrayBuffer(sizeof(int) * numValues));

            syncCommand.returnValue = buffer->data();
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                return QJSValue(QJSValue::NullValue);
            } else {
                QV4::ScopedFunctionObject constructor(scope,
                                                      m_v4engine->typedArrayCtors[
                                                      QV4::Heap::TypedArray::Int32Array]);
                QV4::ScopedCallData callData(scope, 1);
                callData->args[0] = buffer;
                constructor->construct(scope, callData);
                return QJSValue(m_v4engine, scope.result.asReturnedValue());
            }
        }
        case FLOAT_VEC2:
            numValues--;
            // Intentional flow through
        case FLOAT_VEC3:
            numValues--;
            // Intentional flow through
        case FLOAT_VEC4: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->newArrayBuffer(sizeof(float) * numValues));

            syncCommand.id = CanvasGlCommandQueue::glGetUniformfv;
            syncCommand.returnValue = buffer->data();
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                return QJSValue(QJSValue::NullValue);
            } else {
                QV4::ScopedFunctionObject constructor(scope,
                                                      m_v4engine->typedArrayCtors[
                                                      QV4::Heap::TypedArray::Float32Array]);
                QV4::ScopedCallData callData(scope, 1);
                callData->args[0] = buffer;
                constructor->construct(scope, callData);
                return QJSValue(m_v4engine, scope.result.asReturnedValue());
            }
        }
        case BOOL_VEC2:
            numValues--;
            // Intentional flow through
        case BOOL_VEC3:
            numValues--;
            // Intentional flow through
        case BOOL_VEC4: {
            GLint *value = new GLint[numValues];
            QJSValue array = m_engine->newArray(numValues);

            syncCommand.returnValue = value;
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                delete[] value;
                return QJSValue(QJSValue::NullValue);
            } else {
                for (int i = 0; i < numValues; i++)
                    array.setProperty(i, bool(value[i]));
                delete[] value;
                return array;
            }
        }
        case FLOAT_MAT2:
            numValues--;
            // Intentional flow through
        case FLOAT_MAT3:
            numValues--;
            // Intentional flow through
        case FLOAT_MAT4: {
            numValues = numValues * numValues;
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->newArrayBuffer(sizeof(float) * numValues));

            syncCommand.id = CanvasGlCommandQueue::glGetUniformfv;
            syncCommand.returnValue = buffer->data();
            scheduleSyncCommand(&syncCommand);
            if (syncCommand.glError) {
                return QJSValue(QJSValue::NullValue);
            } else {
                QV4::ScopedFunctionObject constructor(scope,
                                                      m_v4engine->typedArrayCtors[
                                                      QV4::Heap::TypedArray::Float32Array]);
                QV4::ScopedCallData callData(scope, 1);
                callData->args[0] = buffer;
                constructor->construct(scope, callData);
                return QJSValue(m_v4engine, scope.result.asReturnedValue());
            }
        }
        default:
            break;
        }
    }
    return QJSValue(QJSValue::UndefinedValue);
}

/*!
 * \qmlmethod list<variant> Context3D::getSupportedExtensions()
 * Returns an array of the extension strings supported by this implementation
 */
QVariantList CanvasContext::getSupportedExtensions()
{
    qCDebug(canvas3drendering).nospace() << Q_FUNC_INFO;

    QVariantList list;

    if (!checkContextLost()) {
        list.append(QVariant::fromValue(QStringLiteral("QTCANVAS3D_gl_state_dump")));

        if (!m_isOpenGLES2 ||
                (m_contextVersion >= 3
                 || m_extensions.contains("GL_OES_standard_derivatives"))) {
            list.append(QVariant::fromValue(QStringLiteral("OES_standard_derivatives")));
        }

        if (m_extensions.contains("GL_EXT_texture_compression_s3tc"))
            list.append(QVariant::fromValue(QStringLiteral("WEBGL_compressed_texture_s3tc")));

        if (m_extensions.contains("GL_IMG_texture_compression_pvrtc"))
            list.append(QVariant::fromValue(QStringLiteral("WEBGL_compressed_texture_pvrtc")));
    }

    return list;
}

bool CanvasContext::isOfType(const QJSValue &value, const char *classname) const
{
    if (!value.isQObject()) {
        return false;
    }

    QObject *obj = value.toQObject();

    if (!obj)
        return false;

    if (!obj->inherits(classname)) {
        return false;
    }

    return true;
}

/*!
 * Schedules a synchronous render job for a command that returns values from OpenGL side.
 */
void CanvasContext::scheduleSyncCommand(GlSyncCommand *command)
{
    if (m_canvas->window() && m_canvas->renderer()) {
        QOpenGLContext *ctx = m_canvas->window()->openglContext();
        if (ctx) {
            bool jobDeleted = false;
            if (ctx->thread() != QThread::currentThread()) {
                // In case of threaded renderer, we block the main thread until the job is done
                CanvasRenderJob *syncJob = new CanvasRenderJob(command, &m_renderJobMutex,
                                                               &m_renderJobCondition,
                                                               m_canvas->renderer(),
                                                               &jobDeleted);
                m_renderJobMutex.lock();
                m_canvas->window()->scheduleRenderJob(syncJob, QQuickWindow::NoStage);
                // scheduleRenderJob will delete the job if the window is not exposed
                if (!jobDeleted)
                    m_renderJobCondition.wait(&m_renderJobMutex);
                m_renderJobMutex.unlock();

            } else {
                // In case of non-threaded renderer, scheduling a job executes it immediately,
                // so we don't need synchronization.
                CanvasRenderJob *syncJob = new CanvasRenderJob(command, 0, 0, m_canvas->renderer(),
                                                               &jobDeleted);
                m_canvas->window()->scheduleRenderJob(syncJob, QQuickWindow::NoStage);
            }
        }
    }

    // Clean up the temporary command data, if any.
    if (command)
        command->deleteData();
}

/*!
 * Schedules a blocking job to clear the queue.
 */
void CanvasContext::handleFullCommandQueue()
{
    // Use no command to simply force the execution of the pending queue
    scheduleSyncCommand(0);
}

void CanvasContext::handleTextureIdResolved(QQuickItem *item)
{
    CanvasTexture *texture = m_quickItemToTextureMap.value(item, 0);
    if (texture && texture->isAlive() && m_textureProviderExt)
        emit m_textureProviderExt->textureReady(item);
}

/*!
 * \qmlmethod variant Context3D::getExtension(string name)
 * \return object if given \a name matches a supported extension.
 * Otherwise returns \c{null}. The returned object may contain constants and/or functions provided
 * by the extension, but at minimum a unique object is returned.
 * Case-insensitive \a name of the extension to be returned.
 */
QVariant CanvasContext::getExtension(const QString &name)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(name:" << name
                                         << ")";
    if (!checkContextLost()) {
        QString upperCaseName = name.toUpper();

        if (upperCaseName == QStringLiteral("QTCANVAS3D_GL_STATE_DUMP")) {
            if (!m_stateDumpExt)
                m_stateDumpExt = new CanvasGLStateDump(this, m_isOpenGLES2, this);
            return QVariant::fromValue(m_stateDumpExt);
        } else if (upperCaseName == QStringLiteral("QTCANVAS3D_TEXTURE_PROVIDER")) {
            if (!m_textureProviderExt)
                m_textureProviderExt = new CanvasTextureProvider(this, this);
            return QVariant::fromValue(m_textureProviderExt);
        } else if (upperCaseName == QStringLiteral("OES_STANDARD_DERIVATIVES") &&
                   m_extensions.contains("GL_OES_standard_derivatives")) {
            if (!m_standardDerivatives)
                m_standardDerivatives = new QObject(this);
            return QVariant::fromValue(m_standardDerivatives);
        } else if (upperCaseName == QStringLiteral("WEBGL_COMPRESSED_TEXTURE_S3TC") &&
                   m_extensions.contains("GL_EXT_texture_compression_s3tc")) {
            if (!m_compressedTextureS3TC)
                m_compressedTextureS3TC = new CompressedTextureS3TC(this);
            return QVariant::fromValue(m_compressedTextureS3TC);
        } else if (upperCaseName == QStringLiteral("WEBGL_COMPRESSED_TEXTURE_PVRTC") &&
                   m_extensions.contains("GL_IMG_texture_compression_pvrtc")) {
            if (!m_compressedTexturePVRTC)
                m_compressedTexturePVRTC = new CompressedTexturePVRTC(this);
            return QVariant::fromValue(m_compressedTexturePVRTC);
        }
    }

    return QVariant(QVariant::Int);
}

void CanvasContext::setContextLostState(bool lost)
{
    if (lost != m_contextLost) {
        m_contextLost = lost;

        // Clear errors on lost state change
        m_error = CANVAS_NO_ERRORS;

        if (lost) {
            for (auto it = m_validObjectMap.cbegin(), end = m_validObjectMap.cend(); it != end; ++it) {
                it.key()->setInvalidated(true);
                disconnect(it.key(), &QObject::destroyed, this, &CanvasContext::handleObjectDeletion);
            }
            m_validObjectMap.clear();
            m_quickItemToTextureMap.clear();
            m_idToCanvasBufferMap.clear();

            m_currentProgram = 0;
            m_currentArrayBuffer = 0;
            m_currentElementArrayBuffer = 0;
            m_currentTexture2D = 0;
            m_currentTextureCubeMap = 0;
            m_currentFramebuffer = 0;
            m_currentRenderbuffer = 0;
            m_contextLostErrorReported = false;
        }
    }
}

void CanvasContext::setCommandQueue(CanvasGlCommandQueue *queue)
{
    m_commandQueue = queue;
    connect(m_commandQueue, &CanvasGlCommandQueue::queueFull,
            this, &CanvasContext::handleFullCommandQueue, Qt::DirectConnection);
}

void CanvasContext::markQuickTexturesDirty()
{
    if (m_quickItemToTextureMap.size()) {
        QMap<QQuickItem *, CanvasTexture *>::iterator i = m_quickItemToTextureMap.begin();
        while (i != m_quickItemToTextureMap.end()) {
            m_commandQueue->addQuickItemAsTexture(i.key(), i.value()->textureId());
            ++i;
        }
    }
}

void CanvasContext::handleObjectDeletion(QObject *obj)
{
    CanvasAbstractObject *jsObj = qobject_cast<CanvasAbstractObject *>(obj);
    if (jsObj)
        m_validObjectMap.remove(jsObj);
}

void CanvasContext::addObjectToValidList(CanvasAbstractObject *jsObj)
{
    m_validObjectMap.insert(jsObj, 0);
    connect(jsObj, &QObject::destroyed, this, &CanvasContext::handleObjectDeletion);
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
