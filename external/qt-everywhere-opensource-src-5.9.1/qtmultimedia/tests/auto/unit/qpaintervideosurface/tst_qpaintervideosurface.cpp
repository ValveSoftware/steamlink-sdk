/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/multimedia

#include <private/qpaintervideosurface_p.h>
#include <QtTest/QtTest>

#include <QtWidgets/qapplication.h>
#include <qvideosurfaceformat.h>

#if QT_CONFIG(opengl)
#include <QtOpenGL/qgl.h>
#include <QtOpenGL/qglframebufferobject.h>
#include <QtGui/qopenglfunctions.h>
#endif

QT_USE_NAMESPACE
class tst_QPainterVideoSurface : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() {}
    void cleanupTestCase() {}

    void colors();

    void supportedFormat_data();
    void supportedFormat();

    void present_data();
    void present();
    void presentOpaqueFrame();

#if QT_CONFIG(opengl)

    void shaderType();

    void shaderTypeStarted_data();
    void shaderTypeStarted();

    void shaderSupportedFormat_data();
    void shaderSupportedFormat();

    void shaderPresent_data();
    void shaderPresent();
    void shaderPresentOpaqueFrame_data();
    void shaderPresentOpaqueFrame();
    void shaderPresentGLFrame_data();
    void shaderPresentGLFrame();
#endif
};

Q_DECLARE_METATYPE(const uchar *)

#if QT_CONFIG(opengl)
Q_DECLARE_METATYPE(QPainterVideoSurface::ShaderType);

class QtTestGLVideoBuffer : public QAbstractVideoBuffer
{
public:
    QtTestGLVideoBuffer()
        : QAbstractVideoBuffer(GLTextureHandle)
        , m_textureId(0)
    {
        QOpenGLContext::currentContext()->functions()->glGenTextures(1, &m_textureId);
    }

    ~QtTestGLVideoBuffer()
    {
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_textureId);
    }

    GLuint textureId() const { return m_textureId; }

    QVariant handle() const { return m_textureId; }

    uchar *map(MapMode, int *, int *) { return 0; }
    void unmap() {}
    MapMode mapMode() const { return NotMapped; }

private:
    GLuint m_textureId;
};

#endif

class QtTestOpaqueVideoBuffer : public QAbstractVideoBuffer
{
public:
    QtTestOpaqueVideoBuffer()
        : QAbstractVideoBuffer(UserHandle)
    {}

    uchar *map(MapMode, int *, int *) { return 0; }
    void unmap() {}
    MapMode mapMode() const { return NotMapped; }
};

void tst_QPainterVideoSurface::colors()
{
    QPainterVideoSurface surface;

    QCOMPARE(surface.brightness(), 0);
    QCOMPARE(surface.contrast(), 0);
    QCOMPARE(surface.hue(), 0);
    QCOMPARE(surface.saturation(), 0);

    surface.setBrightness(56);
    QCOMPARE(surface.brightness(), 56);

    surface.setContrast(43);
    QCOMPARE(surface.contrast(), 43);

    surface.setHue(-84);
    QCOMPARE(surface.hue(), -84);

    surface.setSaturation(100);
    QCOMPARE(surface.saturation(), 100);
}

static const uchar rgb32ImageData[] =
{
    0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00,
    0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00
};

static const uchar argb32ImageData[] =
{
    0x00, 0xff, 0x00, 0x00, 0xcc, 0x00, 0xff, 0xcc,
    0x77, 0x00, 0x00, 0x77, 0x00, 0xff, 0xff, 0x00
};

static const uchar rgb24ImageData[] =
{
    0x00, 0xff, 0x00, 0x00, 0xcc, 0x00, 0x00, 0x00,
    0xcc, 0x00, 0xcc, 0x77, 0xff, 0x77, 0x00, 0x00
};

static const uchar rgb565ImageData[] =
{
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00
};

static const uchar yuvPlanarImageData[] =
{
    0x00, 0x00, 0x0f, 0xff, 0xff, 0x0f, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0x0f, 0x0f, 0xff, 0x0f, 0x00,
    0x00, 0xff, 0x0f, 0x00, 0x00, 0x0f, 0xff, 0x00,
    0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff,
    0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff,
    0x0f, 0xff, 0x0f, 0x00, 0x00, 0x0f, 0xff, 0x0f,
    0x00, 0x0f, 0xff, 0x0f, 0x0f, 0xff, 0x0f, 0x00,
    0x00, 0x00, 0x0f, 0xff, 0xff, 0x0f, 0x00, 0x00,
    0x00, 0x0f, 0x0f, 0x00,
    0x0f, 0x00, 0x00, 0x0f,
    0x0f, 0x00, 0x00, 0x0f,
    0x00, 0x0f, 0x0f, 0x00,
    0x00, 0x0f, 0x0f, 0x00,
    0x0f, 0x00, 0x00, 0x0f,
    0x0f, 0x00, 0x00, 0x0f,
    0x00, 0x0f, 0x0f, 0x00,
};

void tst_QPainterVideoSurface::supportedFormat_data()
{
    QTest::addColumn<QAbstractVideoBuffer::HandleType>("handleType");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<QSize>("frameSize");
    QTest::addColumn<bool>("supportedPixelFormat");
    QTest::addColumn<bool>("supportedFormat");

    QTest::newRow("rgb32 640x480")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB32
            << QSize(640, 480)
            << true
            << true;
    QTest::newRow("rgb32 -640x480")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB32
            << QSize(-640, 480)
            << true
            << false;
    QTest::newRow("rgb24 1024x768")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB24
            << QSize(1024, 768)
#ifndef QT_OPENGL_ES
            << true
            << true;
#else
            << false
            << false;
#endif
    QTest::newRow("rgb24 -1024x-768")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB24
            << QSize(-1024, -768)
#ifndef QT_OPENGL_ES
            << true
#else
            << false
#endif
            << false;
    QTest::newRow("rgb565 0x0")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB565
            << QSize(0, 0)
            << true
            << false;
    QTest::newRow("YUV420P 640x480")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_YUV420P
            << QSize(640, 480)
            << false
            << false;
    QTest::newRow("YUV420P 640x-480")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_YUV420P
            << QSize(640, -480)
            << false
            << false;
    QTest::newRow("Y8 640x480")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_Y8
            << QSize(640, 480)
            << false
            << false;
    QTest::newRow("Texture: rgb32 640x480")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_RGB32
            << QSize(640, 480)
            << false
            << false;
    QTest::newRow("Texture: rgb32 -640x480")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_RGB32
            << QSize(-640, 480)
            << false
            << false;
    QTest::newRow("rgb565 32x32")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB565
            << QSize(32, 32)
            << true
            << true;
    QTest::newRow("rgb565 0x0")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_RGB565
            << QSize(0, 0)
            << true
            << false;
    QTest::newRow("argb32 256x256")
            << QAbstractVideoBuffer::NoHandle
            << QVideoFrame::Format_ARGB32
            << QSize(256, 256)
            << true
            << true;
    QTest::newRow("Texture: rgb24 1024x768")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_RGB24
            << QSize(1024, 768)
            << false
            << false;
    QTest::newRow("Texture: rgb24 -1024x-768")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_RGB24
            << QSize(-1024, -768)
            << false
            << false;
    QTest::newRow("Texture: YUV420P 640x480")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_YUV420P
            << QSize(640, 480)
            << false
            << false;
    QTest::newRow("Texture: YUV420P 640x-480")
            << QAbstractVideoBuffer::GLTextureHandle
            << QVideoFrame::Format_YUV420P
            << QSize(640, -480)
            << false
            << false;
    QTest::newRow("User Buffer: rgb32 256x256")
            << QAbstractVideoBuffer::UserHandle
            << QVideoFrame::Format_RGB32
            << QSize(256, 256)
            << false
            << false;
#if !defined(Q_OS_MAC)
    QTest::newRow("Pixmap: rgb32 640x480")
            << QAbstractVideoBuffer::QPixmapHandle
            << QVideoFrame::Format_RGB32
            << QSize(640, 480)
            << true
            << true;
    QTest::newRow("Pixmap: YUV420P 640x480")
            << QAbstractVideoBuffer::QPixmapHandle
            << QVideoFrame::Format_YUV420P
            << QSize(640, 480)
            << false
            << true;
#endif
}

void tst_QPainterVideoSurface::supportedFormat()
{
    QFETCH(QAbstractVideoBuffer::HandleType, handleType);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(QSize, frameSize);
    QFETCH(bool, supportedPixelFormat);
    QFETCH(bool, supportedFormat);

    QPainterVideoSurface surface;

    const QList<QVideoFrame::PixelFormat> pixelFormats = surface.supportedPixelFormats(handleType);

    QCOMPARE(pixelFormats.contains(pixelFormat), supportedPixelFormat);

    QVideoSurfaceFormat format(frameSize, pixelFormat, handleType);

    QCOMPARE(surface.isFormatSupported(format), supportedFormat);
    QCOMPARE(surface.start(format), supportedFormat);
}

void tst_QPainterVideoSurface::present_data()
{
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormatA");
    QTest::addColumn<QSize>("frameSizeA");
    QTest::addColumn<const uchar *>("frameDataA");
    QTest::addColumn<int>("bytesA");
    QTest::addColumn<int>("bytesPerLineA");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormatB");
    QTest::addColumn<QSize>("frameSizeB");
    QTest::addColumn<const uchar *>("frameDataB");
    QTest::addColumn<int>("bytesB");
    QTest::addColumn<int>("bytesPerLineB");

    QTest::newRow("rgb32 -> argb32")
            << QVideoFrame::Format_RGB32
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb32ImageData)
            << int(sizeof(rgb32ImageData))
            << 8
            << QVideoFrame::Format_ARGB32
            << QSize(2, 2)
            << static_cast<const uchar *>(argb32ImageData)
            << int(sizeof(argb32ImageData))
            << 8;

#ifndef QT_OPENGL_ES
    QTest::newRow("rgb32 -> rgb24")
            << QVideoFrame::Format_RGB32
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb32ImageData)
            << int(sizeof(rgb32ImageData))
            << 8
            << QVideoFrame::Format_RGB24
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb24ImageData)
            << int(sizeof(rgb24ImageData))
            << 8;
#endif

    QTest::newRow("rgb32 -> rgb565")
            << QVideoFrame::Format_RGB32
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb32ImageData)
            << int(sizeof(rgb32ImageData))
            << 8
            << QVideoFrame::Format_RGB565
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb565ImageData)
            << int(sizeof(rgb565ImageData))
            << 4;

#ifndef QT_OPENGL_ES
    QTest::newRow("rgb24 -> rgb565")
            << QVideoFrame::Format_RGB24
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb24ImageData)
            << int(sizeof(rgb24ImageData))
            << 8
            << QVideoFrame::Format_RGB565
            << QSize(2, 2)
            << static_cast<const uchar *>(rgb565ImageData)
            << int(sizeof(rgb565ImageData))
            << 4;
#endif
}

void tst_QPainterVideoSurface::present()
{
    QFETCH(QVideoFrame::PixelFormat, pixelFormatA);
    QFETCH(QSize, frameSizeA);
    QFETCH(const uchar *, frameDataA);
    QFETCH(int, bytesA);
    QFETCH(int, bytesPerLineA);
    QFETCH(QVideoFrame::PixelFormat, pixelFormatB);
    QFETCH(QSize, frameSizeB);
    QFETCH(const uchar *, frameDataB);
    QFETCH(int, bytesB);
    QFETCH(int, bytesPerLineB);

    QPainterVideoSurface surface;

    QImage image(320, 240, QImage::Format_RGB32);

    QSignalSpy frameSpy(&surface, SIGNAL(frameChanged()));

    const QList<QVideoFrame::PixelFormat> pixelFormats = surface.supportedPixelFormats();

    {   // Test painting before started.
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }

    QVideoSurfaceFormat formatA(frameSizeA, pixelFormatA);

    QVERIFY(surface.start(formatA));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    {   // Test painting before receiving a frame.
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.error(), QAbstractVideoSurface::NoError);

    QVideoFrame frameA(bytesA, frameSizeA, bytesPerLineA, pixelFormatA);

    frameA.map(QAbstractVideoBuffer::WriteOnly);
    memcpy(frameA.bits(), frameDataA, frameA.mappedBytes());
    frameA.unmap();

    QVERIFY(surface.present(frameA));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(frameSpy.count(), 1);

    {
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);

    {   // Test repainting before receiving another frame.
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);

    // Not ready.
    QVERIFY(!surface.present(frameA));
    QCOMPARE(frameSpy.count(), 1);

    surface.setReady(true);
    QCOMPARE(surface.isReady(), true);
    QVERIFY(surface.present(frameA));
    QCOMPARE(frameSpy.count(), 2);

    // Try switching to a different format after starting.
    QVideoSurfaceFormat formatB(frameSizeB, pixelFormatB);

    QVERIFY(surface.start(formatB));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QVideoFrame frameB(bytesB, frameSizeB, bytesPerLineB, pixelFormatB);

    frameB.map(QAbstractVideoBuffer::WriteOnly);
    memcpy(frameB.bits(), frameDataB, frameB.mappedBytes());
    frameB.unmap();

    QVERIFY(surface.present(frameB));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(frameSpy.count(), 3);

    {
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QVERIFY(surface.isActive());

    surface.stop();

    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);

    // Try presenting a frame while stopped.
    QVERIFY(!surface.present(frameB));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::StoppedError);

    // Try presenting a frame with a different format.
    QVERIFY(surface.start(formatB));
    QVERIFY(!surface.present(frameA));
    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::IncorrectFormatError);
}

void tst_QPainterVideoSurface::presentOpaqueFrame()
{
    QPainterVideoSurface surface;

    QImage image(320, 240, QImage::Format_RGB32);

    QVideoSurfaceFormat format(QSize(64, 64), QVideoFrame::Format_RGB32);

    QVERIFY(surface.start(format));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QVideoFrame frame(new QtTestOpaqueVideoBuffer, QSize(64, 64), QVideoFrame::Format_RGB32);

    if (surface.present(frame)) {
        QPainter painter(&image);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }

    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::IncorrectFormatError);
}

#if QT_CONFIG(opengl)

void tst_QPainterVideoSurface::shaderType()
{
    QPainterVideoSurface surface;
    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
    QCOMPARE(surface.supportedShaderTypes(), QPainterVideoSurface::NoShaders);

    surface.setGLContext(const_cast<QGLContext *>(widget.context()));
    QCOMPARE(surface.glContext(), widget.context());

    {
        QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));

        surface.setShaderType(QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(spy.count(), 0);
    }

#ifndef QT_OPENGL_ES
    if (surface.supportedShaderTypes() & QPainterVideoSurface::FragmentProgramShader) {
        QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));

        surface.setShaderType(QPainterVideoSurface::FragmentProgramShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::FragmentProgramShader);
        QCOMPARE(spy.count(), 1);

        surface.setShaderType(QPainterVideoSurface::FragmentProgramShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::FragmentProgramShader);
        QCOMPARE(spy.count(), 1);
    }
#endif

    if (surface.supportedShaderTypes() & QPainterVideoSurface::GlslShader) {
        QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));

        surface.setShaderType(QPainterVideoSurface::GlslShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::GlslShader);
        QCOMPARE(spy.count(), 1);

        surface.setShaderType(QPainterVideoSurface::GlslShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::GlslShader);
        QCOMPARE(spy.count(), 1);
    }

    {
        QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));

        surface.setGLContext(const_cast<QGLContext *>(widget.context()));
        QCOMPARE(surface.glContext(), widget.context());
        QCOMPARE(spy.count(), 0);
    }

    surface.setGLContext(0);
    QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
    QCOMPARE(surface.supportedShaderTypes(), QPainterVideoSurface::NoShaders);

    {
        QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));

        surface.setShaderType(QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(spy.count(), 0);

#ifndef QT_OPENGL_ES
        surface.setShaderType(QPainterVideoSurface::FragmentProgramShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(spy.count(), 0);
#endif

        surface.setShaderType(QPainterVideoSurface::GlslShader);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(spy.count(), 0);
    }
}

void tst_QPainterVideoSurface::shaderTypeStarted_data()
{
    QTest::addColumn<QPainterVideoSurface::ShaderType>("shaderType");

#ifndef QT_OPENGL_ES
    QTest::newRow("ARBfp")
            << QPainterVideoSurface::FragmentProgramShader;
#endif
    QTest::newRow("GLSL")
            << QPainterVideoSurface::GlslShader;
}

void tst_QPainterVideoSurface::shaderTypeStarted()
{
    QFETCH(QPainterVideoSurface::ShaderType, shaderType);

    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QPainterVideoSurface surface;

    surface.setGLContext(const_cast<QGLContext *>(widget.context()));

    if (!(surface.supportedShaderTypes() & shaderType))
        QSKIP("Shader type unsupported on this platform");

    surface.setShaderType(shaderType);
    QCOMPARE(surface.shaderType(), shaderType);

    QVERIFY(surface.start(QVideoSurfaceFormat(QSize(640, 480), QVideoFrame::Format_RGB32)));
    {
        QSignalSpy spy(&surface, SIGNAL(activeChanged(bool)));

        surface.setShaderType(QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.isActive(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().value(0).toBool(), false);
    }

    QVERIFY(surface.start(QVideoSurfaceFormat(QSize(640, 480), QVideoFrame::Format_RGB32)));
    {
        QSignalSpy spy(&surface, SIGNAL(activeChanged(bool)));

        surface.setShaderType(QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.isActive(), true);
        QCOMPARE(spy.count(), 0);

        surface.setShaderType(shaderType);
        QCOMPARE(surface.shaderType(), shaderType);
        QCOMPARE(surface.isActive(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().value(0).toBool(), false);
    }

    QVERIFY(surface.start(QVideoSurfaceFormat(QSize(640, 480), QVideoFrame::Format_RGB32)));
    {
        QSignalSpy spy(&surface, SIGNAL(activeChanged(bool)));

        surface.setShaderType(shaderType);
        QCOMPARE(surface.shaderType(), shaderType);
        QCOMPARE(surface.isActive(), true);
        QCOMPARE(spy.count(), 0);

        surface.setGLContext(0);
        QCOMPARE(surface.shaderType(), QPainterVideoSurface::NoShaders);
        QCOMPARE(surface.isActive(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().value(0).toBool(), false);
    }
}

void tst_QPainterVideoSurface::shaderSupportedFormat_data()
{
    QTest::addColumn<QPainterVideoSurface::ShaderType>("shaderType");
    QTest::addColumn<QAbstractVideoBuffer::HandleType>("handleType");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<QSize>("frameSize");
    QTest::addColumn<bool>("supportedPixelFormat");
    QTest::addColumn<bool>("supportedFormat");

    QList<QPair<QPainterVideoSurface::ShaderType, QByteArray> > types;


#ifndef QT_OPENGL_ES
    types << qMakePair(QPainterVideoSurface::FragmentProgramShader, QByteArray("ARBfp: "));
#endif
    types << qMakePair(QPainterVideoSurface::GlslShader, QByteArray("GLSL: "));

    QPair<QPainterVideoSurface::ShaderType, QByteArray> type;
    foreach (type, types) {
        QTest::newRow((type.second + "rgb32 640x480").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB32
                << QSize(640, 480)
                << true
                << true;
        QTest::newRow((type.second + "rgb32 -640x480").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB32
                << QSize(-640, 480)
                << true
                << false;
        QTest::newRow((type.second + "rgb565 32x32").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB565
                << QSize(32, 32)
                << true
                << true;
        QTest::newRow((type.second + "rgb565 0x0").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB565
                << QSize(0, 0)
                << true
                << false;
        QTest::newRow((type.second + "argb32 256x256").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_ARGB32
                << QSize(256, 256)
                << true
                << true;
        QTest::newRow((type.second + "rgb24 1024x768").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB24
                << QSize(1024, 768)
#ifndef QT_OPENGL_ES
                << true
                << true;
#else
                << false
                << false;
#endif
        QTest::newRow((type.second + "rgb24 -1024x-768").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_RGB24
                << QSize(-1024, -768)
#ifndef QT_OPENGL_ES
                << true
#else
                << false
#endif
                << false;
#ifndef Q_OS_MAC
        QTest::newRow((type.second + "YUV420P 640x480").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_YUV420P
                << QSize(640, 480)
                << true
                << true;
        QTest::newRow((type.second + "YUV420P 640x-480").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_YUV420P
                << QSize(640, -480)
                << true
                << false;
#endif
        QTest::newRow((type.second + "Y8 640x480").constData())
                << type.first
                << QAbstractVideoBuffer::NoHandle
                << QVideoFrame::Format_Y8
                << QSize(640, 480)
                << false
                << false;
        QTest::newRow((type.second + "Texture: rgb32 640x480").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB32
                << QSize(640, 480)
                << true
                << true;
        QTest::newRow((type.second + "Texture: rgb32 -640x480").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB32
                << QSize(-640, 480)
                << true
                << false;
#ifndef Q_OS_MAC
        QTest::newRow((type.second + "Texture: rgb565 32x32").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB565
                << QSize(32, 32)
                << false
                << false;
        QTest::newRow((type.second + "Texture: rgb565 0x0").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB565
                << QSize(0, 0)
                << false
                << false;
#endif
        QTest::newRow((type.second + "Texture argb32 256x256").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_ARGB32
                << QSize(256, 256)
                << true
                << true;
#ifndef Q_OS_MAC
        QTest::newRow((type.second + "Texture: rgb24 1024x768").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB24
                << QSize(1024, 768)
                << false
                << false;
        QTest::newRow((type.second + "Texture: rgb24 -1024x-768").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_RGB24
                << QSize(-1024, -768)
                << false
                << false;
        QTest::newRow((type.second + "Texture: YUV420P 640x480").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_YUV420P
                << QSize(640, 480)
                << false
                << false;
        QTest::newRow((type.second + "Texture: YUV420P 640x-480").constData())
                << type.first
                << QAbstractVideoBuffer::GLTextureHandle
                << QVideoFrame::Format_YUV420P
                << QSize(640, -480)
                << false
                << false;
#endif
        QTest::newRow(type.second + "User Buffer: rgb32 256x256")
                << type.first
                << QAbstractVideoBuffer::UserHandle
                << QVideoFrame::Format_RGB32
                << QSize(256, 256)
                << false
                << false;
    }
}

void tst_QPainterVideoSurface::shaderSupportedFormat()
{
    QFETCH(QPainterVideoSurface::ShaderType, shaderType);
    QFETCH(QAbstractVideoBuffer::HandleType, handleType);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(QSize, frameSize);
    QFETCH(bool, supportedPixelFormat);
    QFETCH(bool, supportedFormat);

    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QPainterVideoSurface surface;
    surface.setGLContext(const_cast<QGLContext *>(widget.context()));


    if (!(surface.supportedShaderTypes() & shaderType))
        QSKIP("Shader type not supported on this platform");

    surface.setShaderType(shaderType);
    if (surface.shaderType() != shaderType)
        QSKIP("Shader type couldn't be set");

    const QList<QVideoFrame::PixelFormat> pixelFormats = surface.supportedPixelFormats(handleType);

    QCOMPARE(pixelFormats.contains(pixelFormat), supportedPixelFormat);

    QVideoSurfaceFormat format(frameSize, pixelFormat, handleType);

    QCOMPARE(surface.isFormatSupported(format), supportedFormat);
    QCOMPARE(surface.start(format), supportedFormat);
}

void tst_QPainterVideoSurface::shaderPresent_data()
{
    QTest::addColumn<QPainterVideoSurface::ShaderType>("shaderType");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormatA");
    QTest::addColumn<QSize>("frameSizeA");
    QTest::addColumn<const uchar *>("frameDataA");
    QTest::addColumn<int>("bytesA");
    QTest::addColumn<int>("bytesPerLineA");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormatB");
    QTest::addColumn<QSize>("frameSizeB");
    QTest::addColumn<const uchar *>("frameDataB");
    QTest::addColumn<int>("bytesB");
    QTest::addColumn<int>("bytesPerLineB");

    QList<QPair<QPainterVideoSurface::ShaderType, QByteArray> > types;
#ifndef QT_OPENGL_ES
    types << qMakePair(QPainterVideoSurface::FragmentProgramShader, QByteArray("ARBfp: "));
#endif
    types << qMakePair(QPainterVideoSurface::GlslShader, QByteArray("GLSL: "));

    QPair<QPainterVideoSurface::ShaderType, QByteArray> type;
    foreach (type, types) {
        QTest::newRow((type.second + "rgb32 -> argb32").constData())
                << type.first
                << QVideoFrame::Format_RGB32
                << QSize(2, 2)
                << static_cast<const uchar *>(rgb32ImageData)
                << int(sizeof(rgb32ImageData))
                << 8
                << QVideoFrame::Format_ARGB32
                << QSize(2, 2)
                << static_cast<const uchar *>(argb32ImageData)
                << int(sizeof(argb32ImageData))
                << 8;

        QTest::newRow((type.second + "rgb32 -> rgb565").constData())
                << type.first
                << QVideoFrame::Format_RGB32
                << QSize(2, 2)
                << static_cast<const uchar *>(rgb32ImageData)
                << int(sizeof(rgb32ImageData))
                << 8
                << QVideoFrame::Format_RGB565
                << QSize(2, 2)
                << static_cast<const uchar *>(rgb565ImageData)
                << int(sizeof(rgb565ImageData))
                << 4;
#ifndef Q_OS_MAC
        QTest::newRow((type.second + "rgb32 -> yuv420p").constData())
                << type.first
                << QVideoFrame::Format_RGB32
                << QSize(2, 2)
                << static_cast<const uchar *>(rgb32ImageData)
                << int(sizeof(rgb32ImageData))
                << 8
                << QVideoFrame::Format_YUV420P
                << QSize(8, 8)
                << static_cast<const uchar *>(yuvPlanarImageData)
                << int(sizeof(yuvPlanarImageData))
                << 8;

        QTest::newRow((type.second + "yv12 -> rgb32").constData())
                << type.first
                << QVideoFrame::Format_YV12
                << QSize(8, 8)
                << static_cast<const uchar *>(yuvPlanarImageData)
                << int(sizeof(yuvPlanarImageData))
                << 8
                << QVideoFrame::Format_RGB32
                << QSize(2, 2)
                << static_cast<const uchar *>(rgb32ImageData)
                << int(sizeof(rgb32ImageData))
                << 8;
#endif
    }
}

void tst_QPainterVideoSurface::shaderPresent()
{
    QFETCH(QPainterVideoSurface::ShaderType, shaderType);
    QFETCH(QVideoFrame::PixelFormat, pixelFormatA);
    QFETCH(QSize, frameSizeA);
    QFETCH(const uchar *, frameDataA);
    QFETCH(int, bytesA);
    QFETCH(int, bytesPerLineA);
    QFETCH(QVideoFrame::PixelFormat, pixelFormatB);
    QFETCH(QSize, frameSizeB);
    QFETCH(const uchar *, frameDataB);
    QFETCH(int, bytesB);
    QFETCH(int, bytesPerLineB);

    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QPainterVideoSurface surface;
    surface.setGLContext(const_cast<QGLContext *>(widget.context()));

    if (!(surface.supportedShaderTypes() & shaderType))
        QSKIP("Shader type unsupported on this platform");

    surface.setShaderType(shaderType);
    if (surface.shaderType() != shaderType)
        QSKIP("Shader type couldn't be set");

    QSignalSpy frameSpy(&surface, SIGNAL(frameChanged()));

    {   // Test painting before starting the surface.
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.error(), QAbstractVideoSurface::NoError);

    QVideoSurfaceFormat formatA(frameSizeA, pixelFormatA);

    QVERIFY(surface.start(formatA));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    // Test painting before receiving a frame.
    {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QVideoFrame frameA(bytesA, frameSizeA, bytesPerLineA, pixelFormatA);

    frameA.map(QAbstractVideoBuffer::WriteOnly);
    memcpy(frameA.bits(), frameDataA, frameA.mappedBytes());
    frameA.unmap();

    QVERIFY(surface.present(frameA));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(frameSpy.count(), 1);

    {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);

    {   // Test repainting before receiving another frame.
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);

    // Not ready.
    QVERIFY(!surface.present(frameA));
    QCOMPARE(frameSpy.count(), 1);

    surface.setReady(true);
    QCOMPARE(surface.isReady(), true);
    QVERIFY(surface.present(frameA));
    QCOMPARE(frameSpy.count(), 2);

    // Try switching to a different format after starting.
    QVideoSurfaceFormat formatB(frameSizeB, pixelFormatB);

    QVERIFY(surface.start(formatB));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QVideoFrame frameB(bytesB, frameSizeB, bytesPerLineB, pixelFormatB);

    frameB.map(QAbstractVideoBuffer::WriteOnly);
    memcpy(frameB.bits(), frameDataB, frameB.mappedBytes());
    frameB.unmap();

    QVERIFY(surface.present(frameB));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(frameSpy.count(), 3);

    {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }
    QCOMPARE(surface.isActive(), true);

    surface.stop();
    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);

    // Try presenting a frame while stopped.
    QVERIFY(!surface.present(frameB));
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::StoppedError);

    // Try stopping while already stopped.
    surface.stop();
    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);

    // Try presenting a frame with a different format.
    QVERIFY(surface.start(formatB));
    QVERIFY(!surface.present(frameA));
    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::IncorrectFormatError);
}

void tst_QPainterVideoSurface::shaderPresentOpaqueFrame_data()
{
    QTest::addColumn<QPainterVideoSurface::ShaderType>("shaderType");

#ifndef QT_OPENGL_ES
    QTest::newRow("ARBfp")
            << QPainterVideoSurface::FragmentProgramShader;
#endif
    QTest::newRow("GLSL")
            << QPainterVideoSurface::GlslShader;
}

void tst_QPainterVideoSurface::shaderPresentOpaqueFrame()
{
    QFETCH(QPainterVideoSurface::ShaderType, shaderType);

    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QPainterVideoSurface surface;
    surface.setGLContext(const_cast<QGLContext *>(widget.context()));

    if (!(surface.supportedShaderTypes() & shaderType))
        QSKIP("Shader type unsupported on this platform");

    surface.setShaderType(shaderType);
    if (surface.shaderType() != shaderType)
        QSKIP("Shader type couldn't be set");

    QVideoSurfaceFormat format(QSize(64, 64), QVideoFrame::Format_RGB32);

    QVERIFY(surface.start(format));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QVideoFrame frame(new QtTestOpaqueVideoBuffer, QSize(64, 64), QVideoFrame::Format_RGB32);

    if (surface.present(frame)) {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }

    QCOMPARE(surface.isActive(), false);
    QCOMPARE(surface.isReady(), false);
    QCOMPARE(surface.error(), QAbstractVideoSurface::IncorrectFormatError);
}

void tst_QPainterVideoSurface::shaderPresentGLFrame_data()
{
    QTest::addColumn<QPainterVideoSurface::ShaderType>("shaderType");

#ifndef QT_OPENGL_ES
    QTest::newRow("ARBfp")
            << QPainterVideoSurface::FragmentProgramShader;
#endif
    QTest::newRow("GLSL")
            << QPainterVideoSurface::GlslShader;
}

void tst_QPainterVideoSurface::shaderPresentGLFrame()
{
    QFETCH(QPainterVideoSurface::ShaderType, shaderType);

    QGLWidget widget;
    widget.show();
    QTest::qWaitForWindowExposed(&widget);
    widget.makeCurrent();

    QPainterVideoSurface surface;
    surface.setGLContext(const_cast<QGLContext *>(widget.context()));

    if (!(surface.supportedShaderTypes() & shaderType))
        QSKIP("Shader type unsupported on this platform");

    surface.setShaderType(shaderType);
    if (surface.shaderType() != shaderType)
        QSKIP("Shader type couldn't be set");

    QVideoSurfaceFormat format(
            QSize(2, 2), QVideoFrame::Format_RGB32, QAbstractVideoBuffer::GLTextureHandle);

    QVERIFY(surface.start(format));
    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), true);

    QtTestGLVideoBuffer *buffer = new QtTestGLVideoBuffer;

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GL_TEXTURE_2D, buffer->textureId());
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb32ImageData);
    f->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    f->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    QVideoFrame frame(buffer, QSize(2, 2), QVideoFrame::Format_RGB32);

    QVERIFY(surface.present(frame));

    {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }

    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);

    {
        QPainter painter(&widget);
        surface.paint(&painter, QRect(0, 0, 320, 240));
    }

    QCOMPARE(surface.isActive(), true);
    QCOMPARE(surface.isReady(), false);
}

#endif

QTEST_MAIN(tst_QPainterVideoSurface)

#include "tst_qpaintervideosurface.moc"
