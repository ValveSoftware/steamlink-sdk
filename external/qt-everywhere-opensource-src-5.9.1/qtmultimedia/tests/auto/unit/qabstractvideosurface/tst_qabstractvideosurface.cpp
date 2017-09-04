/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtTest/QtTest>

#include <qabstractvideosurface.h>
#include <qvideosurfaceformat.h>

class tst_QAbstractVideoSurface : public QObject
{
    Q_OBJECT
public:
    tst_QAbstractVideoSurface();
    ~tst_QAbstractVideoSurface();

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void setError();
    void isFormatSupported_data();
    void isFormatSupported();
    void nearestFormat_data();
    void nearestFormat();
    void start_data();
    void start();
    void nativeResolution();
    void supportedFormatsChanged();
};

typedef QMap<QAbstractVideoBuffer::HandleType, QVideoFrame::PixelFormat> SupportedFormatMap;

Q_DECLARE_METATYPE(SupportedFormatMap)

class QtTestVideoSurface : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    explicit QtTestVideoSurface(QObject *parent = 0) : QAbstractVideoSurface(parent) {}
    explicit QtTestVideoSurface(SupportedFormatMap formats, QObject *parent = 0)
            : QAbstractVideoSurface(parent), supportedFormats(formats) {}

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const
    {
        return supportedFormats.values(handleType);
    }

    bool present(const QVideoFrame &) { return false; }

    using QAbstractVideoSurface::setError;

    /* adding protected setNativeResolution*/
    using QAbstractVideoSurface::setNativeResolution;

    /* fun to generate supportedFormatsChanged signal */
    QList<QVideoFrame::PixelFormat> supportedPixelFormatsChange(QList<QVideoFrame::PixelFormat> formats)
    {
        supportedFormats.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_RGB32);
        QList<QVideoFrame::PixelFormat> supportedFormats = supportedPixelFormats();
        if (supportedFormats.count() != formats.count()) {
            emit supportedFormatsChanged();
        }
        return supportedFormats;
    }

private:
    SupportedFormatMap supportedFormats;
};

tst_QAbstractVideoSurface::tst_QAbstractVideoSurface()
{
}

tst_QAbstractVideoSurface::~tst_QAbstractVideoSurface()
{
}

void tst_QAbstractVideoSurface::initTestCase()
{
}

void tst_QAbstractVideoSurface::cleanupTestCase()
{
}

void tst_QAbstractVideoSurface::init()
{
}

void tst_QAbstractVideoSurface::cleanup()
{
}

void tst_QAbstractVideoSurface::setError()
{
    qRegisterMetaType<QAbstractVideoSurface::Error>();

    QtTestVideoSurface surface;

    QCOMPARE(surface.error(), QAbstractVideoSurface::NoError);
    QTest::ignoreMessage(QtDebugMsg, "NoError");
    qDebug() << QAbstractVideoSurface::NoError;

    surface.setError(QAbstractVideoSurface::StoppedError);
    QCOMPARE(surface.error(), QAbstractVideoSurface::StoppedError);
    QTest::ignoreMessage(QtDebugMsg, "StoppedError");
    qDebug() << QAbstractVideoSurface::StoppedError;

    surface.setError(QAbstractVideoSurface::ResourceError);
    QCOMPARE(surface.error(), QAbstractVideoSurface::ResourceError);
    QTest::ignoreMessage(QtDebugMsg, "ResourceError");
    qDebug() << QAbstractVideoSurface::ResourceError;

    surface.setError(QAbstractVideoSurface::NoError);
    QCOMPARE(surface.error(), QAbstractVideoSurface::NoError);
    QTest::ignoreMessage(QtDebugMsg, "NoError");
    qDebug() << QAbstractVideoSurface::NoError;

    surface.setError(QAbstractVideoSurface::UnsupportedFormatError);
    QCOMPARE(surface.error(), QAbstractVideoSurface::UnsupportedFormatError);
    QTest::ignoreMessage(QtDebugMsg, "UnsupportedFormatError");
    qDebug() << QAbstractVideoSurface::UnsupportedFormatError;

    surface.setError(QAbstractVideoSurface::IncorrectFormatError);
    QCOMPARE(surface.error(), QAbstractVideoSurface::IncorrectFormatError);
    QTest::ignoreMessage(QtDebugMsg, "IncorrectFormatError");
    qDebug() << QAbstractVideoSurface::IncorrectFormatError;
}

void tst_QAbstractVideoSurface::isFormatSupported_data()
{
    QTest::addColumn<SupportedFormatMap>("supportedFormats");
    QTest::addColumn<QVideoSurfaceFormat>("format");
    QTest::addColumn<bool>("supported");

    SupportedFormatMap formats;

    QTest::newRow("no formats: rgb32")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_RGB32)
            << false;
    QTest::newRow("no formats: yv12")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_YV12)
            << false;
    QTest::newRow("no formats: rgb32 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_RGB32,
                    QAbstractVideoBuffer::GLTextureHandle)
            << false;
    QTest::newRow("no formats: rgb24 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_RGB24,
                    QAbstractVideoBuffer::GLTextureHandle)
            << false;

    formats.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_RGB32);
    formats.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_RGB24);
    formats.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_YUV444);
    formats.insertMulti(QAbstractVideoBuffer::GLTextureHandle, QVideoFrame::Format_RGB32);

    QTest::newRow("supported: rgb32")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_RGB32)
            << true;
    QTest::newRow("supported: rgb24")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_RGB24)
            << true;
    QTest::newRow("unsupported: yv12")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_YV12)
            << false;
    QTest::newRow("supported: rgb32 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_RGB32,
                    QAbstractVideoBuffer::GLTextureHandle)
            << true;
    QTest::newRow("unsupported: rgb24 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_RGB24,
                    QAbstractVideoBuffer::GLTextureHandle)
            << false;
    QTest::newRow("unsupported: yv12 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_YV12,
                    QAbstractVideoBuffer::GLTextureHandle)
            << false;

    formats.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_YV12);
    formats.insertMulti(QAbstractVideoBuffer::GLTextureHandle, QVideoFrame::Format_RGB24);

    QTest::newRow("supported: yv12")
            << formats
            << QVideoSurfaceFormat(QSize(800, 600), QVideoFrame::Format_YV12)
            << true;
    QTest::newRow("supported: rgb24 gl")
            << formats
            << QVideoSurfaceFormat(
                    QSize(800, 600),
                    QVideoFrame::Format_RGB24,
                    QAbstractVideoBuffer::GLTextureHandle)
            << true;
}

void tst_QAbstractVideoSurface::isFormatSupported()
{
    QFETCH(SupportedFormatMap, supportedFormats);
    QFETCH(QVideoSurfaceFormat, format);
    QFETCH(bool, supported);

    QtTestVideoSurface surface(supportedFormats);

    QCOMPARE(surface.isFormatSupported(format), supported);
}

void tst_QAbstractVideoSurface::nearestFormat_data()
{
    isFormatSupported_data();
}

void tst_QAbstractVideoSurface::nearestFormat()
{
    QFETCH(SupportedFormatMap, supportedFormats);
    QFETCH(QVideoSurfaceFormat, format);
    QFETCH(bool, supported);

    QtTestVideoSurface surface(supportedFormats);

    QCOMPARE(surface.nearestFormat(format) == format, supported);
}

void tst_QAbstractVideoSurface::start_data()
{
    QTest::addColumn<QVideoSurfaceFormat>("format");

    QTest::newRow("rgb32") << QVideoSurfaceFormat(
            QSize(800, 600),
            QVideoFrame::Format_RGB32);
    QTest::newRow("yv12") << QVideoSurfaceFormat(
            QSize(800, 600),
            QVideoFrame::Format_YV12);
    QTest::newRow("rgb32 gl") << QVideoSurfaceFormat(
            QSize(800, 600),
            QVideoFrame::Format_RGB32,
            QAbstractVideoBuffer::GLTextureHandle);
}

void tst_QAbstractVideoSurface::start()
{
    QFETCH(QVideoSurfaceFormat, format);

    QtTestVideoSurface surface;
    surface.setError(QAbstractVideoSurface::ResourceError);

    QSignalSpy formatSpy(&surface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)));
    QSignalSpy activeSpy(&surface, SIGNAL(activeChanged(bool)));

    QVERIFY(!surface.isActive());
    QCOMPARE(surface.surfaceFormat(), QVideoSurfaceFormat());

    QVERIFY(surface.start(format));

    QVERIFY(surface.isActive());
    QCOMPARE(surface.surfaceFormat(), format);

    QCOMPARE(formatSpy.count(), 1);
    QCOMPARE(qvariant_cast<QVideoSurfaceFormat>(formatSpy.last().at(0)), format);

    QCOMPARE(activeSpy.count(), 1);
    QCOMPARE(activeSpy.last().at(0).toBool(), true);

    // Starting twice won't change active
    // XXX should this also not emit surfaceFormatChanged?
    QVERIFY(surface.start(format));
    QCOMPARE(activeSpy.count(), 1);
    QVERIFY(surface.isActive());

    // error() is reset on a successful start.
    QCOMPARE(surface.error(), QAbstractVideoSurface::NoError);

    surface.stop();

    QVERIFY(!surface.isActive());
    QCOMPARE(surface.surfaceFormat(), QVideoSurfaceFormat());

    QCOMPARE(formatSpy.count(), 3);
    QCOMPARE(qvariant_cast<QVideoSurfaceFormat>(formatSpy.last().at(0)), QVideoSurfaceFormat());

    QCOMPARE(activeSpy.count(), 2);
    QCOMPARE(activeSpy.last().at(0).toBool(), false);

    // Stopping a stopped surface shouldn't hurt
    surface.stop();

    QVERIFY(!surface.isActive());
    QCOMPARE(surface.surfaceFormat(), QVideoSurfaceFormat());

    QCOMPARE(formatSpy.count(), 3);
    QCOMPARE(qvariant_cast<QVideoSurfaceFormat>(formatSpy.last().at(0)), QVideoSurfaceFormat());

    QCOMPARE(activeSpy.count(), 2);
    QCOMPARE(activeSpy.last().at(0).toBool(), false);
}

// Test nativeResolution property
void tst_QAbstractVideoSurface::nativeResolution()
{
    QtTestVideoSurface surface;
    QSignalSpy spy(&surface, SIGNAL(nativeResolutionChanged(QSize)));
    QSize size1 = surface.nativeResolution();
    QVERIFY(size1.width() == -1);
    QVERIFY(size1.height() == -1);
    QVERIFY(spy.count() == 0);

    QSize res(100,150);
    surface.setNativeResolution(res);
    QVERIFY(spy.count() == 1);

    QSize size2 = qvariant_cast<QSize>(spy.at(0).at(0));
    QVERIFY(size2.width() == 100);
    QVERIFY(size2.height() == 150);

    // Setting again should not emit
    surface.setNativeResolution(res);
    QVERIFY(spy.count() == 1);

    size2 = qvariant_cast<QSize>(spy.at(0).at(0));
    QVERIFY(size2.width() == 100);
    QVERIFY(size2.height() == 150);

    spy.clear();
}

// QAbstractVideoSurface's supported Formats Changed Signal
void tst_QAbstractVideoSurface::supportedFormatsChanged()
{
    SupportedFormatMap formatMap;
    formatMap.insertMulti(QAbstractVideoBuffer::NoHandle, QVideoFrame::Format_RGB24);
    QtTestVideoSurface surface(formatMap);
    QSignalSpy spy(&surface, SIGNAL(supportedFormatsChanged()));
    QList<QVideoFrame::PixelFormat> formats = surface.supportedPixelFormats();
    QVERIFY(formats.count() == 1);
    QVERIFY(spy.count() == 0);

    // user defined implementation for generation of supportedFormatsChanged signal
    QList<QVideoFrame::PixelFormat> newFormats = surface.supportedPixelFormatsChange(formats);
    QVERIFY(newFormats.count() == (formats.count() + 1));
    QVERIFY(spy.count() == 1);
    spy.clear();
}

QTEST_MAIN(tst_QAbstractVideoSurface)

#include "tst_qabstractvideosurface.moc"
