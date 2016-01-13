/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qeglimagetexturesurface_p.h>
#include <qpaintervideosurface_p.h>

#include <QtCore/qmath.h>
#include <QtCore/qvariant.h>
#include <QtCore/qdebug.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qx11info_x11.h>
#include <qvideosurfaceformat.h>


QT_BEGIN_NAMESPACE

//#define DEBUG_OMAPFB_SURFACE

const QAbstractVideoBuffer::HandleType EGLImageTextureHandle =
QAbstractVideoBuffer::HandleType(QAbstractVideoBuffer::UserHandle+3434);

/*!
    \class QEglImageTextureSurface
    \internal
*/

/*!
*/
QEglImageTextureSurface::QEglImageTextureSurface(QObject *parent)
    : QAbstractVideoSurface(parent)
    , m_context(0)
    , m_program(0)
    , m_pixelFormat(QVideoFrame::Format_Invalid)
    , m_ready(false)
    , m_colorKey(49,0,49)
    , m_fallbackSurface(0)
    , m_fallbackSurfaceActive(false)
{
    m_fallbackSurface = new QPainterVideoSurface(this);
}

/*!
*/
QEglImageTextureSurface::~QEglImageTextureSurface()
{
    if (isActive())
        stop();
}

/*!
*/
QList<QVideoFrame::PixelFormat> QEglImageTextureSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
#ifdef DEBUG_OMAPFB_SURFACE
    qDebug() << Q_FUNC_INFO << handleType;
#endif

    if (handleType == EGLImageTextureHandle) {
        return QList<QVideoFrame::PixelFormat>()
                << QVideoFrame::Format_RGB32
                << QVideoFrame::Format_ARGB32;
    }

    return m_fallbackSurface->supportedPixelFormats(handleType);
}

const char *qt_glsl_eglTextureVertexShaderProgram =
        "attribute highp vec4 vertexCoordArray;\n"
        "attribute mediump vec2 textureCoordArray;\n"
        "uniform highp mat4 positionMatrix;\n"
        "varying mediump vec2 textureCoord;\n"
        "void main (void)\n"
        "{\n"
        "   gl_Position = positionMatrix * vertexCoordArray;\n"
        "   textureCoord = textureCoordArray;\n"
        "}";

static const char* qt_glsl_eglTextureShaderProgram =
        "#extension GL_OES_EGL_image_external: enable\n"
        "\n"
        "uniform samplerExternalOES texRgb;\n"
        "varying mediump vec2 textureCoord;\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texRgb, textureCoord);\n"
        "}";


/*!
*/
bool QEglImageTextureSurface::start(const QVideoSurfaceFormat &format)
{
#ifdef DEBUG_OMAPFB_SURFACE
    qDebug() << Q_FUNC_INFO << format;
#endif

    m_fallbackSurfaceActive = false;
    if (format.handleType() != EGLImageTextureHandle) {
        qWarning() << Q_FUNC_INFO << "Non EGLImageTextureHandle based format requested, fallback to QPainterVideoSurface";
        connect(m_fallbackSurface, SIGNAL(activeChanged(bool)),
                this, SIGNAL(activeChanged(bool)));
        connect(m_fallbackSurface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)),
                this, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)));
        connect(m_fallbackSurface, SIGNAL(supportedFormatsChanged()),
                this, SIGNAL(supportedFormatsChanged()));
        connect(m_fallbackSurface, SIGNAL(nativeResolutionChanged(QSize)),
                this, SIGNAL(nativeResolutionChanged(QSize)));
        connect(m_fallbackSurface, SIGNAL(frameChanged()),
                this, SIGNAL(frameChanged()));

        if (m_fallbackSurface->start(format)) {
            m_fallbackSurfaceActive = true;
            QAbstractVideoSurface::start(format);
        } else {
            qWarning() << Q_FUNC_INFO << "failed to start video surface:" << m_fallbackSurface->error();
            setError(m_fallbackSurface->error());

            disconnect(m_fallbackSurface, SIGNAL(activeChanged(bool)),
                       this, SIGNAL(activeChanged(bool)));
            disconnect(m_fallbackSurface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)),
                       this, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)));
            disconnect(m_fallbackSurface, SIGNAL(supportedFormatsChanged()),
                       this, SIGNAL(supportedFormatsChanged()));
            disconnect(m_fallbackSurface, SIGNAL(nativeResolutionChanged(QSize)),
                       this, SIGNAL(nativeResolutionChanged(QSize)));
            disconnect(m_fallbackSurface, SIGNAL(frameChanged()),
                       this, SIGNAL(frameChanged()));
        }

        return m_fallbackSurfaceActive;
    }

    QAbstractVideoSurface::Error error = NoError;

    if (isActive())
        stop();

    if (format.frameSize().isEmpty()) {
        setError(UnsupportedFormatError);
    } else if (m_context) {
        m_context->makeCurrent();
        m_program = new QGLShaderProgram(m_context, this);

        if (!m_program->addShaderFromSourceCode(QGLShader::Vertex, qt_glsl_eglTextureVertexShaderProgram)) {
            qWarning("QOmapFbVideoSurface: Vertex shader compile error %s",
                     qPrintable(m_program->log()));
            error = ResourceError;
        }

        if (error == NoError
                && !m_program->addShaderFromSourceCode(QGLShader::Fragment, qt_glsl_eglTextureShaderProgram)) {
            qWarning("QOmapFbVideoSurface: Vertex shader compile error %s",
                     qPrintable(m_program->log()));
            error = QAbstractVideoSurface::ResourceError;
        }

        if (error == NoError) {
            m_program->bindAttributeLocation("textureCoordArray", 1);
            if(!m_program->link()) {
                qWarning("QOmapFbVideoSurface: Shader link error %s", qPrintable(m_program->log()));
                m_program->removeAllShaders();
                error = QAbstractVideoSurface::ResourceError;
            }
        }

        if (error != QAbstractVideoSurface::NoError) {
            delete m_program;
            m_program = 0;
        }
    }

    if (error == QAbstractVideoSurface::NoError) {
        m_scanLineDirection = format.scanLineDirection();
        m_frameSize = format.frameSize();
        m_pixelFormat = format.pixelFormat();
        m_frameSize = format.frameSize();
        m_sourceRect = format.viewport();
        m_ready = true;

        return QAbstractVideoSurface::start(format);
    }

    QAbstractVideoSurface::stop();
    return false;
}

/*!
*/
void QEglImageTextureSurface::stop()
{
#ifdef DEBUG_OMAPFB_SURFACE
    qDebug() << Q_FUNC_INFO;
#endif

    if (m_fallbackSurfaceActive) {
        m_fallbackSurface->stop();
        m_fallbackSurfaceActive = false;

        disconnect(m_fallbackSurface, SIGNAL(activeChanged(bool)),
                   this, SIGNAL(activeChanged(bool)));
        disconnect(m_fallbackSurface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)),
                   this, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)));
        disconnect(m_fallbackSurface, SIGNAL(supportedFormatsChanged()),
                   this, SIGNAL(supportedFormatsChanged()));
        disconnect(m_fallbackSurface, SIGNAL(nativeResolutionChanged(QSize)),
                   this, SIGNAL(nativeResolutionChanged(QSize)));
        disconnect(m_fallbackSurface, SIGNAL(frameChanged()),
                   this, SIGNAL(frameChanged()));

        m_ready = false;
        QAbstractVideoSurface::stop();
    }

    if (isActive()) {
        if (m_context)
            m_context->makeCurrent();
        m_frame = QVideoFrame();

        m_program->removeAllShaders();
        delete m_program;
        m_program = 0;
        m_ready = false;

        QAbstractVideoSurface::stop();
    }
}

/*!
*/
bool QEglImageTextureSurface::present(const QVideoFrame &frame)
{
    if (m_fallbackSurfaceActive) {
        if (m_fallbackSurface->present(frame)) {
            return true;
        } else {
            setError(m_fallbackSurface->error());
            stop();
            return false;
        }
    }

    if (!m_ready) {
        if (!isActive())
            setError(StoppedError);
        else
            m_frame = frame;
    } else if (frame.isValid()
               && (frame.pixelFormat() != m_pixelFormat || frame.size() != m_frameSize)) {
        setError(IncorrectFormatError);
        qWarning() << "Received frame of incorrect format, stopping the surface";

        stop();
    } else {
        if (m_context)
            m_context->makeCurrent();
        m_frame = frame;
        m_ready = false;
        emit frameChanged();
        return true;
    }
    return false;
}

/*!
*/
int QEglImageTextureSurface::brightness() const
{
    return m_fallbackSurface->brightness();
}

/*!
*/
void QEglImageTextureSurface::setBrightness(int brightness)
{
    m_fallbackSurface->setBrightness(brightness);
}

/*!
*/
int QEglImageTextureSurface::contrast() const
{
    return m_fallbackSurface->contrast();
}

/*!
*/
void QEglImageTextureSurface::setContrast(int contrast)
{
    m_fallbackSurface->setContrast(contrast);
}

/*!
*/
int QEglImageTextureSurface::hue() const
{
    return m_fallbackSurface->hue();
}

/*!
*/
void QEglImageTextureSurface::setHue(int hue)
{
    m_fallbackSurface->setHue(hue);
}

/*!
*/
int QEglImageTextureSurface::saturation() const
{
    return m_fallbackSurface->saturation();
}

/*!
*/
void QEglImageTextureSurface::setSaturation(int saturation)
{
    m_fallbackSurface->setSaturation(saturation);
}

/*!
*/
bool QEglImageTextureSurface::isReady() const
{
    return m_fallbackSurfaceActive ? m_fallbackSurface->isReady() : m_ready;
}

/*!
*/
void QEglImageTextureSurface::setReady(bool ready)
{
    m_ready = ready;
    if (m_fallbackSurfaceActive)
        m_fallbackSurface->setReady(ready);
}

/*!
*/
void QEglImageTextureSurface::paint(QPainter *painter, const QRectF &target, const QRectF &sourceRect)
{
    if (m_fallbackSurfaceActive) {
        m_fallbackSurface->paint(painter, target, sourceRect);
        return;
    }

    if (!isActive() || !m_frame.isValid()) {
        painter->fillRect(target, QBrush(Qt::black));
    } else {
        const QRectF source(
                    m_sourceRect.x() + m_sourceRect.width() * sourceRect.x(),
                    m_sourceRect.y() + m_sourceRect.height() * sourceRect.y(),
                    m_sourceRect.width() * sourceRect.width(),
                    m_sourceRect.height() * sourceRect.height());

        bool stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
        bool scissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);

        painter->beginNativePainting();

        if (stencilTestEnabled)
            glEnable(GL_STENCIL_TEST);
        if (scissorTestEnabled)
            glEnable(GL_SCISSOR_TEST);

        const int width = QGLContext::currentContext()->device()->width();
        const int height = QGLContext::currentContext()->device()->height();

        const QTransform transform = painter->deviceTransform();

        const GLfloat wfactor = 2.0 / width;
        const GLfloat hfactor = -2.0 / height;

        const GLfloat positionMatrix[4][4] =
        {
            {
                /*(0,0)*/ GLfloat(wfactor * transform.m11() - transform.m13()),
                /*(0,1)*/ GLfloat(hfactor * transform.m12() + transform.m13()),
                /*(0,2)*/ 0.0,
                /*(0,3)*/ GLfloat(transform.m13())
            }, {
                /*(1,0)*/ GLfloat(wfactor * transform.m21() - transform.m23()),
                /*(1,1)*/ GLfloat(hfactor * transform.m22() + transform.m23()),
                /*(1,2)*/ 0.0,
                /*(1,3)*/ GLfloat(transform.m23())
            }, {
                /*(2,0)*/ 0.0,
                /*(2,1)*/ 0.0,
                /*(2,2)*/ -1.0,
                /*(2,3)*/ 0.0
            }, {
                /*(3,0)*/ GLfloat(wfactor * transform.dx() - transform.m33()),
                /*(3,1)*/ GLfloat(hfactor * transform.dy() + transform.m33()),
                /*(3,2)*/ 0.0,
                /*(3,3)*/ GLfloat(transform.m33())
            }
        };

        const GLfloat vTop = m_scanLineDirection == QVideoSurfaceFormat::TopToBottom
                ? target.top()
                : target.bottom() + 1;
        const GLfloat vBottom = m_scanLineDirection == QVideoSurfaceFormat::TopToBottom
                ? target.bottom() + 1
                : target.top();


        const GLfloat vertexCoordArray[] =
        {
            GLfloat(target.left())     , GLfloat(vBottom),
            GLfloat(target.right() + 1), GLfloat(vBottom),
            GLfloat(target.left())     , GLfloat(vTop),
            GLfloat(target.right() + 1), GLfloat(vTop)
        };

        const GLfloat txLeft = source.left() / m_frameSize.width();
        const GLfloat txRight = source.right() / m_frameSize.width();
        const GLfloat txTop = m_scanLineDirection == QVideoSurfaceFormat::TopToBottom
                ? source.top() / m_frameSize.height()
                : source.bottom() / m_frameSize.height();
        const GLfloat txBottom = m_scanLineDirection == QVideoSurfaceFormat::TopToBottom
                ? source.bottom() / m_frameSize.height()
                : source.top() / m_frameSize.height();

        const GLfloat textureCoordArray[] =
        {
            txLeft , txBottom,
            txRight, txBottom,
            txLeft , txTop,
            txRight, txTop
        };

        m_program->bind();

        m_program->enableAttributeArray("vertexCoordArray");
        m_program->enableAttributeArray("textureCoordArray");
        m_program->setAttributeArray("vertexCoordArray", vertexCoordArray, 2);
        m_program->setAttributeArray("textureCoordArray", textureCoordArray, 2);
        m_program->setUniformValue("positionMatrix", positionMatrix);
        m_program->setUniformValue("texRgb", 0);

        //map() binds the external texture
        m_frame.map(QAbstractVideoBuffer::ReadOnly);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        //it's necessary to unbind the external texture
        m_frame.unmap();

        m_program->release();

        painter->endNativePainting();
    }
}

/*!
    \fn QEglImageTextureSurface::frameChanged()
*/

/*!
*/
const QGLContext *QEglImageTextureSurface::glContext() const
{
    return m_context;
}

/*!
*/
void QEglImageTextureSurface::setGLContext(QGLContext *context)
{
    if (m_context == context)
        return;

    stop();

    m_context = context;

    m_fallbackSurface->setGLContext(context);
    if (m_fallbackSurface->supportedShaderTypes() & QPainterVideoSurface::GlslShader) {
        m_fallbackSurface->setShaderType(QPainterVideoSurface::GlslShader);
    } else {
        m_fallbackSurface->setShaderType(QPainterVideoSurface::FragmentProgramShader);
    }

    emit supportedFormatsChanged();
}

void QEglImageTextureSurface::viewportDestroyed()
{
    m_context = 0;
    m_fallbackSurface->viewportDestroyed();

    setError(ResourceError);
    stop();
}

#include "moc_qeglimagetexturesurface_p.cpp"
QT_END_NAMESPACE
