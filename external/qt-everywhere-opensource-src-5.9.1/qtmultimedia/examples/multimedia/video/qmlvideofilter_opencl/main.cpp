/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Multimedia module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>
#include <QQuickView>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QAbstractVideoFilter>
#include <QQmlContext>
#include <QFileInfo>

#ifdef Q_OS_OSX
#include <OpenCL/opencl.h>
#include <OpenGL/OpenGL.h>
#else
#include <CL/opencl.h>
#endif

#ifdef Q_OS_LINUX
#include <QtPlatformHeaders/QGLXNativeContext>
#endif

#include "rgbframehelper.h"

static const char *openclSrc =
        "__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;\n"
        "__kernel void Emboss(__read_only image2d_t imgIn, __write_only image2d_t imgOut, float factor) {\n"
        "    const int2 pos = { get_global_id(0), get_global_id(1) };\n"
        "    float4 diff = read_imagef(imgIn, sampler, pos + (int2)(1,1)) - read_imagef(imgIn, sampler, pos - (int2)(1,1));\n"
        "    float color = (diff.x + diff.y + diff.z) / factor + 0.5f;\n"
        "    write_imagef(imgOut, pos, (float4)(color, color, color, 1.0f));\n"
        "}\n";

class CLFilter : public QAbstractVideoFilter
{
    Q_OBJECT
    Q_PROPERTY(qreal factor READ factor WRITE setFactor NOTIFY factorChanged)

public:
    CLFilter() : m_factor(1) { }
    qreal factor() const { return m_factor; }
    void setFactor(qreal v);

    QVideoFilterRunnable *createFilterRunnable() Q_DECL_OVERRIDE;

signals:
    void factorChanged();

private:
    qreal m_factor;
};

class CLFilterRunnable : public QVideoFilterRunnable
{
public:
    CLFilterRunnable(CLFilter *filter);
    ~CLFilterRunnable();
    QVideoFrame run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags) Q_DECL_OVERRIDE;

private:
    void releaseTextures();
    uint newTexture();

    CLFilter *m_filter;
    QSize m_size;
    uint m_tempTexture;
    uint m_outTexture;
    uint m_lastInputTexture;
    cl_context m_clContext;
    cl_device_id m_clDeviceId;
    cl_mem m_clImage[2];
    cl_command_queue m_clQueue;
    cl_program m_clProgram;
    cl_kernel m_clKernel;
};

QVideoFilterRunnable *CLFilter::createFilterRunnable()
{
    return new CLFilterRunnable(this);
}

CLFilterRunnable::CLFilterRunnable(CLFilter *filter) :
    m_filter(filter),
    m_tempTexture(0),
    m_outTexture(0),
    m_lastInputTexture(0),
    m_clContext(0),
    m_clQueue(0),
    m_clProgram(0),
    m_clKernel(0)
{
    m_clImage[0] = m_clImage[1] = 0;

    // Set up OpenCL.
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    cl_uint n;
    cl_int err = clGetPlatformIDs(0, 0, &n);
    if (err != CL_SUCCESS) {
        qWarning("Failed to get platform ID count (error %d)", err);
        if (err == -1001) {
            qDebug("Could not find OpenCL implementation. ICD missing?"
#ifdef Q_OS_LINUX
                   " Check /etc/OpenCL/vendors."
#endif
                );
        }
        return;
    }
    if (n == 0) {
        qWarning("No OpenCL platform found");
        return;
    }
    QVector<cl_platform_id> platformIds;
    platformIds.resize(n);
    if (clGetPlatformIDs(n, platformIds.data(), 0) != CL_SUCCESS) {
        qWarning("Failed to get platform IDs");
        return;
    }
    cl_platform_id platform = platformIds[0];
    const char *vendor = (const char *) f->glGetString(GL_VENDOR);
    qDebug("GL_VENDOR: %s", vendor);
    const bool isNV = vendor && strstr(vendor, "NVIDIA");
    const bool isIntel = vendor && strstr(vendor, "Intel");
    const bool isAMD = vendor && strstr(vendor, "ATI");
    qDebug("Found %u OpenCL platforms:", n);
    for (cl_uint i = 0; i < n; ++i) {
        QByteArray name;
        name.resize(1024);
        clGetPlatformInfo(platformIds[i], CL_PLATFORM_NAME, name.size(), name.data(), 0);
        qDebug("Platform %p: %s", platformIds[i], name.constData());
        // Running with an OpenCL platform without GPU support is not going
        // to cut it. In practice we want the platform for the GPU which we
        // are using with OpenGL.
        if (isNV && name.contains(QByteArrayLiteral("NVIDIA")))
            platform = platformIds[i];
        else if (isIntel && name.contains(QByteArrayLiteral("Intel")))
            platform = platformIds[i];
        else if (isAMD && name.contains(QByteArrayLiteral("AMD")))
            platform = platformIds[i];
    }
    qDebug("Using platform %p", platform);

    // Set up the context with OpenCL/OpenGL interop.
#if defined (Q_OS_OSX)
    cl_context_properties contextProps[] = { CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                             (cl_context_properties) CGLGetShareGroup(CGLGetCurrentContext()),
                                             0 };
#elif defined(Q_OS_WIN)
    cl_context_properties contextProps[] = { CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
                                             CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
                                             CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
                                             0 };
#elif defined(Q_OS_LINUX)
    QVariant nativeGLXHandle = QOpenGLContext::currentContext()->nativeHandle();
    QGLXNativeContext nativeGLXContext;
    if (!nativeGLXHandle.isNull() && nativeGLXHandle.canConvert<QGLXNativeContext>())
        nativeGLXContext = nativeGLXHandle.value<QGLXNativeContext>();
    else
        qWarning("Failed to get the underlying GLX context from the current QOpenGLContext");
    cl_context_properties contextProps[] = { CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
                                             CL_GL_CONTEXT_KHR, (cl_context_properties) nativeGLXContext.context(),
                                             CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
                                             0 };
#endif

    m_clContext = clCreateContextFromType(contextProps, CL_DEVICE_TYPE_GPU, 0, 0, &err);
    if (!m_clContext) {
        qWarning("Failed to create OpenCL context: %d", err);
        return;
    }

    // Get the GPU device id
#if defined(Q_OS_OSX)
    // On OS X, get the "online" device/GPU. This is required for OpenCL/OpenGL context sharing.
    err = clGetGLContextInfoAPPLE(m_clContext, CGLGetCurrentContext(),
                                  CL_CGL_DEVICE_FOR_CURRENT_VIRTUAL_SCREEN_APPLE,
                                  sizeof(cl_device_id), &m_clDeviceId, 0);
    if (err != CL_SUCCESS) {
        qWarning("Failed to get OpenCL device for current screen: %d", err);
        return;
    }
#else
    clGetGLContextInfoKHR_fn getGLContextInfo = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
    if (!getGLContextInfo || getGLContextInfo(contextProps, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                              sizeof(cl_device_id), &m_clDeviceId, 0) != CL_SUCCESS) {
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &m_clDeviceId, 0);
        if (err != CL_SUCCESS) {
            qWarning("Failed to get OpenCL device: %d", err);
            return;
        }
    }
#endif

    m_clQueue = clCreateCommandQueue(m_clContext, m_clDeviceId, 0, &err);
    if (!m_clQueue) {
        qWarning("Failed to create OpenCL command queue: %d", err);
        return;
    }
    // Build the program.
    m_clProgram = clCreateProgramWithSource(m_clContext, 1, &openclSrc, 0, &err);
    if (!m_clProgram) {
        qWarning("Failed to create OpenCL program: %d", err);
        return;
    }
    if (clBuildProgram(m_clProgram, 1, &m_clDeviceId, 0, 0, 0) != CL_SUCCESS) {
        qWarning("Failed to build OpenCL program");
        QByteArray log;
        log.resize(2048);
        clGetProgramBuildInfo(m_clProgram, m_clDeviceId, CL_PROGRAM_BUILD_LOG, log.size(), log.data(), 0);
        qDebug("Build log: %s", log.constData());
        return;
    }
    m_clKernel = clCreateKernel(m_clProgram, "Emboss", &err);
    if (!m_clKernel) {
        qWarning("Failed to create emboss OpenCL kernel: %d", err);
        return;
    }
}

CLFilterRunnable::~CLFilterRunnable()
{
    releaseTextures();
    if (m_clKernel)
        clReleaseKernel(m_clKernel);
    if (m_clProgram)
        clReleaseProgram(m_clProgram);
    if (m_clQueue)
        clReleaseCommandQueue(m_clQueue);
    if (m_clContext)
        clReleaseContext(m_clContext);
}

void CLFilterRunnable::releaseTextures()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (m_tempTexture)
        f->glDeleteTextures(1, &m_tempTexture);
    if (m_outTexture)
        f->glDeleteTextures(1, &m_outTexture);
    m_tempTexture = m_outTexture = m_lastInputTexture = 0;
    if (m_clImage[0])
        clReleaseMemObject(m_clImage[0]);
    if (m_clImage[1])
        clReleaseMemObject(m_clImage[1]);
    m_clImage[0] = m_clImage[1] = 0;
}

uint CLFilterRunnable::newTexture()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    GLuint texture;
    f->glGenTextures(1, &texture);
    f->glBindTexture(GL_TEXTURE_2D, texture);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_size.width(), m_size.height(),
                    0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    return texture;
}

QVideoFrame CLFilterRunnable::run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags)
{
    Q_UNUSED(surfaceFormat);
    Q_UNUSED(flags);

    // This example supports RGB data only, either in system memory (typical with cameras on all
    // platforms) or as an OpenGL texture (e.g. video playback on OS X).
    // The latter is the fast path where everything happens on GPU. THe former involves a texture upload.

    if (!input->isValid()
            || (input->handleType() != QAbstractVideoBuffer::NoHandle
                && input->handleType() != QAbstractVideoBuffer::GLTextureHandle)) {
        qWarning("Invalid input format");
        return *input;
    }

    if (input->pixelFormat() == QVideoFrame::Format_YUV420P
            || input->pixelFormat() == QVideoFrame::Format_YV12) {
        qWarning("YUV data is not supported");
        return *input;
    }

    if (m_size != input->size()) {
        releaseTextures();
        m_size = input->size();
    }

    // Create a texture from the image data.
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    GLuint texture;
    if (input->handleType() == QAbstractVideoBuffer::NoHandle) {
        // Upload.
        if (m_tempTexture)
            f->glBindTexture(GL_TEXTURE_2D, m_tempTexture);
        else
            m_tempTexture = newTexture();
        input->map(QAbstractVideoBuffer::ReadOnly);
        // glTexImage2D only once and use TexSubImage later on. This avoids the need
        // to recreate the CL image object on every frame.
        f->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_size.width(), m_size.height(),
                           GL_RGBA, GL_UNSIGNED_BYTE, input->bits());
        input->unmap();
        texture = m_tempTexture;
    } else {
        // Already an OpenGL texture.
        texture = input->handle().toUInt();
        f->glBindTexture(GL_TEXTURE_2D, texture);
        // Unlike on the other branch, the input texture may change, so m_clImage[0] may need to be recreated.
        if (m_lastInputTexture && m_lastInputTexture != texture && m_clImage[0]) {
            clReleaseMemObject(m_clImage[0]);
            m_clImage[0] = 0;
        }
        m_lastInputTexture = texture;
    }

    // OpenCL image objects cannot be read and written at the same time. So use
    // a separate texture for the result.
    if (!m_outTexture)
        m_outTexture = newTexture();

    // Create the image objects if not yet done.
    cl_int err;
    if (!m_clImage[0]) {
        m_clImage[0] = clCreateFromGLTexture2D(m_clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, texture, &err);
        if (!m_clImage[0]) {
            qWarning("Failed to create OpenGL image object from OpenGL texture: %d", err);
            return *input;
        }
        cl_image_format fmt;
        if (clGetImageInfo(m_clImage[0], CL_IMAGE_FORMAT, sizeof(fmt), &fmt, 0) != CL_SUCCESS) {
            qWarning("Failed to query image format");
            return *input;
        }
        if (fmt.image_channel_order != CL_RGBA)
            qWarning("OpenCL image is not RGBA, expect errors");
    }
    if (!m_clImage[1]) {
        m_clImage[1] = clCreateFromGLTexture2D(m_clContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, m_outTexture, &err);
        if (!m_clImage[1]) {
            qWarning("Failed to create output OpenGL image object from OpenGL texture: %d", err);
            return *input;
        }
    }

    // We are all set. Queue acquiring the image objects.
    f->glFinish();
    clEnqueueAcquireGLObjects(m_clQueue, 2, m_clImage, 0, 0, 0);

    // Set up the kernel arguments.
    clSetKernelArg(m_clKernel, 0, sizeof(cl_mem), &m_clImage[0]);
    clSetKernelArg(m_clKernel, 1, sizeof(cl_mem), &m_clImage[1]);
    // Accessing dynamic properties on the filter element is simple:
    cl_float factor = m_filter->factor();
    clSetKernelArg(m_clKernel, 2, sizeof(cl_float), &factor);

    // And queue the kernel.
    const size_t workSize[] = { size_t(m_size.width()), size_t(m_size.height()) };
    err = clEnqueueNDRangeKernel(m_clQueue, m_clKernel, 2, 0, workSize, 0, 0, 0, 0);
    if (err != CL_SUCCESS)
        qWarning("Failed to enqueue kernel: %d", err);

    // Return the texture from our output image object.
    // We return a texture even when the original video frame had pixel data in system memory.
    // Qt Multimedia is smart enough to handle this. Once the data is on the GPU, it stays there. No readbacks, no copies.
    clEnqueueReleaseGLObjects(m_clQueue, 2, m_clImage, 0, 0, 0);
    clFinish(m_clQueue);
    return frameFromTexture(m_outTexture, m_size, input->pixelFormat());
}

// InfoFilter will just provide some information about the video frame, to demonstrate
// passing arbitrary data to QML via its finished() signal.
class InfoFilter : public QAbstractVideoFilter
{
    Q_OBJECT

public:
    QVideoFilterRunnable *createFilterRunnable() Q_DECL_OVERRIDE;

signals:
    void finished(QObject *result);

private:
    friend class InfoFilterRunnable;
};

class InfoFilterRunnable : public QVideoFilterRunnable
{
public:
    InfoFilterRunnable(InfoFilter *filter) : m_filter(filter) { }
    QVideoFrame run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags) Q_DECL_OVERRIDE;

private:
    InfoFilter *m_filter;
};

class InfoFilterResult : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize frameResolution READ frameResolution)
    Q_PROPERTY(QString handleType READ handleType)
    Q_PROPERTY(int pixelFormat READ pixelFormat)

public:
    InfoFilterResult() : m_pixelFormat(0) { }
    QSize frameResolution() const { return m_frameResolution; }
    QString handleType() const { return m_handleType; }
    int pixelFormat() const { return m_pixelFormat; }

private:
    QSize m_frameResolution;
    QString m_handleType;
    int m_pixelFormat;
    friend class InfoFilterRunnable;
};

void CLFilter::setFactor(qreal v)
{
    if (m_factor != v) {
        m_factor = v;
        emit factorChanged();
    }
}

QVideoFilterRunnable *InfoFilter::createFilterRunnable()
{
    return new InfoFilterRunnable(this);
}

QVideoFrame InfoFilterRunnable::run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags)
{
    Q_UNUSED(surfaceFormat);
    Q_UNUSED(flags);
    InfoFilterResult *result = new InfoFilterResult;
    result->m_frameResolution = input->size();
    switch (input->handleType()) {
    case QAbstractVideoBuffer::NoHandle:
        result->m_handleType = QLatin1String("pixel data");
        result->m_pixelFormat = input->pixelFormat();
        break;
    case QAbstractVideoBuffer::GLTextureHandle:
        result->m_handleType = QLatin1String("OpenGL texture");
        break;
    default:
        result->m_handleType = QLatin1String("unknown");
        break;
    }
    emit m_filter->finished(result); // parent-less QObject -> ownership transferred to the JS engine
    return *input;
}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN // avoid ANGLE on Windows
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#endif
    QGuiApplication app(argc, argv);

    qmlRegisterType<CLFilter>("qmlvideofilter.cl.test", 1, 0, "CLFilter");
    qmlRegisterType<InfoFilter>("qmlvideofilter.cl.test", 1, 0, "InfoFilter");

    QQuickView view;
    QString fn;
    if (argc > 1) {
        fn = QUrl::fromLocalFile(QFileInfo(QString::fromUtf8(argv[1])).absoluteFilePath()).toString();
        qDebug("Playing video %s", qPrintable(fn));
    } else {
        qDebug("No video file specified, using camera instead.");
    }
    view.rootContext()->setContextProperty("videoFilename", fn);
    view.setSource(QUrl("qrc:///main.qml"));

    view.show();

    return app.exec();
}

#include "main.moc"
