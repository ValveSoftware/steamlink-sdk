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

#include <qvideoframe.h>
#include <QtGui/QImage>
#include <QtCore/QPointer>

// Adds an enum, and the stringized version
#define ADD_ENUM_TEST(x) \
    QTest::newRow(#x) \
        << QVideoFrame::x \
    << QString(QLatin1String(#x));


class tst_QVideoFrame : public QObject
{
    Q_OBJECT
public:
    tst_QVideoFrame();
    ~tst_QVideoFrame();

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void create_data();
    void create();
    void createInvalid_data();
    void createInvalid();
    void createFromBuffer_data();
    void createFromBuffer();
    void createFromImage_data();
    void createFromImage();
    void createFromIncompatibleImage();
    void createNull();
    void destructor();
    void copy_data();
    void copy();
    void assign_data();
    void assign();
    void map_data();
    void map();
    void mapImage_data();
    void mapImage();
    void mapPlanes_data();
    void mapPlanes();
    void imageDetach();
    void formatConversion_data();
    void formatConversion();

    void metadata();

    void isMapped();
    void isReadable();
    void isWritable();
};

Q_DECLARE_METATYPE(QImage::Format)

class QtTestVideoBuffer : public QObject, public QAbstractVideoBuffer
{
    Q_OBJECT
public:
    QtTestVideoBuffer()
        : QAbstractVideoBuffer(NoHandle) {}
    explicit QtTestVideoBuffer(QAbstractVideoBuffer::HandleType type)
        : QAbstractVideoBuffer(type) {}

    MapMode mapMode() const { return NotMapped; }

    uchar *map(MapMode, int *, int *) { return 0; }
    void unmap() {}
};

class QtTestPlanarVideoBuffer : public QAbstractPlanarVideoBuffer
{
public:
    QtTestPlanarVideoBuffer()
        : QAbstractPlanarVideoBuffer(NoHandle), m_planeCount(0), m_mapMode(NotMapped) {}
    explicit QtTestPlanarVideoBuffer(QAbstractVideoBuffer::HandleType type)
        : QAbstractPlanarVideoBuffer(type), m_planeCount(0), m_mapMode(NotMapped) {}

    MapMode mapMode() const { return m_mapMode; }

    int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) {
        m_mapMode = mode;
        if (numBytes)
            *numBytes = m_numBytes;
        for (int i = 0; i < m_planeCount; ++i) {
            data[i] = m_data[i];
            bytesPerLine[i] = m_bytesPerLine[i];
        }
        return m_planeCount;
    }
    void unmap() { m_mapMode = NotMapped; }

    uchar *m_data[4];
    int m_bytesPerLine[4];
    int m_planeCount;
    int m_numBytes;
    MapMode m_mapMode;
};

tst_QVideoFrame::tst_QVideoFrame()
{
}

tst_QVideoFrame::~tst_QVideoFrame()
{
}

void tst_QVideoFrame::initTestCase()
{
}

void tst_QVideoFrame::cleanupTestCase()
{
}

void tst_QVideoFrame::init()
{
}

void tst_QVideoFrame::cleanup()
{
}

void tst_QVideoFrame::create_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<int>("bytes");
    QTest::addColumn<int>("bytesPerLine");

    QTest::newRow("64x64 ARGB32")
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32
            << 16384
            << 256;
    QTest::newRow("32x256 YUV420P")
            << QSize(32, 256)
            << QVideoFrame::Format_YUV420P
            << 13288
            << 32;
}

void tst_QVideoFrame::create()
{
    QFETCH(QSize, size);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(int, bytes);
    QFETCH(int, bytesPerLine);

    QVideoFrame frame(bytes, size, bytesPerLine, pixelFormat);

    QVERIFY(frame.isValid());
    QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
    QCOMPARE(frame.handle(), QVariant());
    QCOMPARE(frame.pixelFormat(), pixelFormat);
    QCOMPARE(frame.size(), size);
    QCOMPARE(frame.width(), size.width());
    QCOMPARE(frame.height(), size.height());
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::createInvalid_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<int>("bytes");
    QTest::addColumn<int>("bytesPerLine");

    QTest::newRow("64x64 ARGB32 0 size")
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32
            << 0
            << 45;
    QTest::newRow("32x256 YUV420P negative size")
            << QSize(32, 256)
            << QVideoFrame::Format_YUV420P
            << -13288
            << 32;
}

void tst_QVideoFrame::createInvalid()
{
    QFETCH(QSize, size);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(int, bytes);
    QFETCH(int, bytesPerLine);

    QVideoFrame frame(bytes, size, bytesPerLine, pixelFormat);

    QVERIFY(!frame.isValid());
    QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
    QCOMPARE(frame.handle(), QVariant());
    QCOMPARE(frame.pixelFormat(), pixelFormat);
    QCOMPARE(frame.size(), size);
    QCOMPARE(frame.width(), size.width());
    QCOMPARE(frame.height(), size.height());
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::createFromBuffer_data()
{
    QTest::addColumn<QAbstractVideoBuffer::HandleType>("handleType");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");

    QTest::newRow("64x64 ARGB32 no handle")
            << QAbstractVideoBuffer::NoHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32;
    QTest::newRow("64x64 ARGB32 gl handle")
            << QAbstractVideoBuffer::GLTextureHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32;
    QTest::newRow("64x64 ARGB32 user handle")
            << QAbstractVideoBuffer::UserHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32;
}

void tst_QVideoFrame::createFromBuffer()
{
    QFETCH(QAbstractVideoBuffer::HandleType, handleType);
    QFETCH(QSize, size);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);

    QVideoFrame frame(new QtTestVideoBuffer(handleType), size, pixelFormat);

    QVERIFY(frame.isValid());
    QCOMPARE(frame.handleType(), handleType);
    QCOMPARE(frame.pixelFormat(), pixelFormat);
    QCOMPARE(frame.size(), size);
    QCOMPARE(frame.width(), size.width());
    QCOMPARE(frame.height(), size.height());
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::createFromImage_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QImage::Format>("imageFormat");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");

    QTest::newRow("64x64 RGB32")
            << QSize(64, 64)
            << QImage::Format_RGB32
            << QVideoFrame::Format_RGB32;
    QTest::newRow("12x45 RGB16")
            << QSize(12, 45)
            << QImage::Format_RGB16
            << QVideoFrame::Format_RGB565;
    QTest::newRow("19x46 ARGB32_Premultiplied")
            << QSize(19, 46)
            << QImage::Format_ARGB32_Premultiplied
            << QVideoFrame::Format_ARGB32_Premultiplied;
}

void tst_QVideoFrame::createFromImage()
{
    QFETCH(QSize, size);
    QFETCH(QImage::Format, imageFormat);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);

    const QImage image(size.width(), size.height(), imageFormat);

    QVideoFrame frame(image);

    QVERIFY(frame.isValid());
    QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
    QCOMPARE(frame.pixelFormat(), pixelFormat);
    QCOMPARE(frame.size(), size);
    QCOMPARE(frame.width(), size.width());
    QCOMPARE(frame.height(), size.height());
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::createFromIncompatibleImage()
{
    const QImage image(64, 64, QImage::Format_Mono);

    QVideoFrame frame(image);

    QVERIFY(!frame.isValid());
    QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
    QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_Invalid);
    QCOMPARE(frame.size(), QSize(64, 64));
    QCOMPARE(frame.width(), 64);
    QCOMPARE(frame.height(), 64);
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::createNull()
{
    // Default ctor
    {
        QVideoFrame frame;

        QVERIFY(!frame.isValid());
        QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
        QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_Invalid);
        QCOMPARE(frame.size(), QSize());
        QCOMPARE(frame.width(), -1);
        QCOMPARE(frame.height(), -1);
        QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
        QCOMPARE(frame.startTime(), qint64(-1));
        QCOMPARE(frame.endTime(), qint64(-1));
        QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadOnly));
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadWrite));
        QVERIFY(!frame.map(QAbstractVideoBuffer::WriteOnly));
        QCOMPARE(frame.isMapped(), false);
        frame.unmap(); // Shouldn't crash
        QCOMPARE(frame.isReadable(), false);
        QCOMPARE(frame.isWritable(), false);
    }

    // Null buffer (shouldn't crash)
    {
        QVideoFrame frame(0, QSize(1024,768), QVideoFrame::Format_ARGB32);
        QVERIFY(!frame.isValid());
        QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
        QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_ARGB32);
        QCOMPARE(frame.size(), QSize(1024, 768));
        QCOMPARE(frame.width(), 1024);
        QCOMPARE(frame.height(), 768);
        QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
        QCOMPARE(frame.startTime(), qint64(-1));
        QCOMPARE(frame.endTime(), qint64(-1));
        QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadOnly));
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadWrite));
        QVERIFY(!frame.map(QAbstractVideoBuffer::WriteOnly));
        QCOMPARE(frame.isMapped(), false);
        frame.unmap(); // Shouldn't crash
        QCOMPARE(frame.isReadable(), false);
        QCOMPARE(frame.isWritable(), false);
    }
}

void tst_QVideoFrame::destructor()
{
    QPointer<QtTestVideoBuffer> buffer = new QtTestVideoBuffer;

    {
        QVideoFrame frame(buffer, QSize(4, 1), QVideoFrame::Format_ARGB32);
    }

    QVERIFY(buffer.isNull());
}

void tst_QVideoFrame::copy_data()
{
    QTest::addColumn<QAbstractVideoBuffer::HandleType>("handleType");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<QVideoFrame::FieldType>("fieldType");
    QTest::addColumn<qint64>("startTime");
    QTest::addColumn<qint64>("endTime");

    QTest::newRow("64x64 ARGB32")
            << QAbstractVideoBuffer::GLTextureHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32
            << QVideoFrame::TopField
            << qint64(63641740)
            << qint64(63641954);
    QTest::newRow("64x64 ARGB32")
            << QAbstractVideoBuffer::GLTextureHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32
            << QVideoFrame::BottomField
            << qint64(63641740)
            << qint64(63641954);
    QTest::newRow("32x256 YUV420P")
            << QAbstractVideoBuffer::UserHandle
            << QSize(32, 256)
            << QVideoFrame::Format_YUV420P
            << QVideoFrame::InterlacedFrame
            << qint64(12345)
            << qint64(12389);
    QTest::newRow("1052x756 ARGB32")
            << QAbstractVideoBuffer::NoHandle
            << QSize(1052, 756)
            << QVideoFrame::Format_ARGB32
            << QVideoFrame::ProgressiveFrame
            << qint64(12345)
            << qint64(12389);
    QTest::newRow("32x256 YUV420P")
            << QAbstractVideoBuffer::UserHandle
            << QSize(32, 256)
            << QVideoFrame::Format_YUV420P
            << QVideoFrame::InterlacedFrame
            << qint64(12345)
            << qint64(12389);
}

void tst_QVideoFrame::copy()
{
    QFETCH(QAbstractVideoBuffer::HandleType, handleType);
    QFETCH(QSize, size);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(QVideoFrame::FieldType, fieldType);
    QFETCH(qint64, startTime);
    QFETCH(qint64, endTime);

    QPointer<QtTestVideoBuffer> buffer = new QtTestVideoBuffer(handleType);

    {
        QVideoFrame frame(buffer, size, pixelFormat);
        frame.setFieldType(QVideoFrame::FieldType(fieldType));
        frame.setStartTime(startTime);
        frame.setEndTime(endTime);

        QVERIFY(frame.isValid());
        QCOMPARE(frame.handleType(), handleType);
        QCOMPARE(frame.pixelFormat(), pixelFormat);
        QCOMPARE(frame.size(), size);
        QCOMPARE(frame.width(), size.width());
        QCOMPARE(frame.height(), size.height());
        QCOMPARE(frame.fieldType(), fieldType);
        QCOMPARE(frame.startTime(), startTime);
        QCOMPARE(frame.endTime(), endTime);

        {
            QVideoFrame otherFrame(frame);

            QVERIFY(!buffer.isNull());

            QVERIFY(otherFrame.isValid());
            QCOMPARE(otherFrame.handleType(), handleType);
            QCOMPARE(otherFrame.pixelFormat(), pixelFormat);
            QCOMPARE(otherFrame.size(), size);
            QCOMPARE(otherFrame.width(), size.width());
            QCOMPARE(otherFrame.height(), size.height());
            QCOMPARE(otherFrame.fieldType(), fieldType);
            QCOMPARE(otherFrame.startTime(), startTime);
            QCOMPARE(otherFrame.endTime(), endTime);

            otherFrame.setEndTime(-1);

            QVERIFY(!buffer.isNull());

            QVERIFY(otherFrame.isValid());
            QCOMPARE(otherFrame.handleType(), handleType);
            QCOMPARE(otherFrame.pixelFormat(), pixelFormat);
            QCOMPARE(otherFrame.size(), size);
            QCOMPARE(otherFrame.width(), size.width());
            QCOMPARE(otherFrame.height(), size.height());
            QCOMPARE(otherFrame.fieldType(), fieldType);
            QCOMPARE(otherFrame.startTime(), startTime);
            QCOMPARE(otherFrame.endTime(), qint64(-1));
        }

        QVERIFY(!buffer.isNull());

        QVERIFY(frame.isValid());
        QCOMPARE(frame.handleType(), handleType);
        QCOMPARE(frame.pixelFormat(), pixelFormat);
        QCOMPARE(frame.size(), size);
        QCOMPARE(frame.width(), size.width());
        QCOMPARE(frame.height(), size.height());
        QCOMPARE(frame.fieldType(), fieldType);
        QCOMPARE(frame.startTime(), startTime);
        QCOMPARE(frame.endTime(), qint64(-1));  // Explicitly shared.
    }

    QVERIFY(buffer.isNull());
}

void tst_QVideoFrame::assign_data()
{
    QTest::addColumn<QAbstractVideoBuffer::HandleType>("handleType");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<QVideoFrame::FieldType>("fieldType");
    QTest::addColumn<qint64>("startTime");
    QTest::addColumn<qint64>("endTime");

    QTest::newRow("64x64 ARGB32")
            << QAbstractVideoBuffer::GLTextureHandle
            << QSize(64, 64)
            << QVideoFrame::Format_ARGB32
            << QVideoFrame::TopField
            << qint64(63641740)
            << qint64(63641954);
    QTest::newRow("32x256 YUV420P")
            << QAbstractVideoBuffer::UserHandle
            << QSize(32, 256)
            << QVideoFrame::Format_YUV420P
            << QVideoFrame::InterlacedFrame
            << qint64(12345)
            << qint64(12389);
}

void tst_QVideoFrame::assign()
{
    QFETCH(QAbstractVideoBuffer::HandleType, handleType);
    QFETCH(QSize, size);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(QVideoFrame::FieldType, fieldType);
    QFETCH(qint64, startTime);
    QFETCH(qint64, endTime);

    QPointer<QtTestVideoBuffer> buffer = new QtTestVideoBuffer(handleType);

    QVideoFrame frame;
    {
        QVideoFrame otherFrame(buffer, size, pixelFormat);
        otherFrame.setFieldType(fieldType);
        otherFrame.setStartTime(startTime);
        otherFrame.setEndTime(endTime);

        frame = otherFrame;

        QVERIFY(!buffer.isNull());

        QVERIFY(otherFrame.isValid());
        QCOMPARE(otherFrame.handleType(), handleType);
        QCOMPARE(otherFrame.pixelFormat(), pixelFormat);
        QCOMPARE(otherFrame.size(), size);
        QCOMPARE(otherFrame.width(), size.width());
        QCOMPARE(otherFrame.height(), size.height());
        QCOMPARE(otherFrame.fieldType(), fieldType);
        QCOMPARE(otherFrame.startTime(), startTime);
        QCOMPARE(otherFrame.endTime(), endTime);

        otherFrame.setStartTime(-1);

        QVERIFY(!buffer.isNull());

        QVERIFY(otherFrame.isValid());
        QCOMPARE(otherFrame.handleType(), handleType);
        QCOMPARE(otherFrame.pixelFormat(), pixelFormat);
        QCOMPARE(otherFrame.size(), size);
        QCOMPARE(otherFrame.width(), size.width());
        QCOMPARE(otherFrame.height(), size.height());
        QCOMPARE(otherFrame.fieldType(), fieldType);
        QCOMPARE(otherFrame.startTime(), qint64(-1));
        QCOMPARE(otherFrame.endTime(), endTime);
    }

    QVERIFY(!buffer.isNull());

    QVERIFY(frame.isValid());
    QCOMPARE(frame.handleType(), handleType);
    QCOMPARE(frame.pixelFormat(), pixelFormat);
    QCOMPARE(frame.size(), size);
    QCOMPARE(frame.width(), size.width());
    QCOMPARE(frame.height(), size.height());
    QCOMPARE(frame.fieldType(), fieldType);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), endTime);

    frame = QVideoFrame();

    QVERIFY(buffer.isNull());

    QVERIFY(!frame.isValid());
    QCOMPARE(frame.handleType(), QAbstractVideoBuffer::NoHandle);
    QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_Invalid);
    QCOMPARE(frame.size(), QSize());
    QCOMPARE(frame.width(), -1);
    QCOMPARE(frame.height(), -1);
    QCOMPARE(frame.fieldType(), QVideoFrame::ProgressiveFrame);
    QCOMPARE(frame.startTime(), qint64(-1));
    QCOMPARE(frame.endTime(), qint64(-1));
}

void tst_QVideoFrame::map_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("mappedBytes");
    QTest::addColumn<int>("bytesPerLine");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");
    QTest::addColumn<QAbstractVideoBuffer::MapMode>("mode");

    QTest::newRow("read-only")
            << QSize(64, 64)
            << 16384
            << 256
            << QVideoFrame::Format_ARGB32
            << QAbstractVideoBuffer::ReadOnly;

    QTest::newRow("write-only")
            << QSize(64, 64)
            << 16384
            << 256
            << QVideoFrame::Format_ARGB32
            << QAbstractVideoBuffer::WriteOnly;

    QTest::newRow("read-write")
            << QSize(64, 64)
            << 16384
            << 256
            << QVideoFrame::Format_ARGB32
            << QAbstractVideoBuffer::ReadWrite;
}

void tst_QVideoFrame::map()
{
    QFETCH(QSize, size);
    QFETCH(int, mappedBytes);
    QFETCH(int, bytesPerLine);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);
    QFETCH(QAbstractVideoBuffer::MapMode, mode);

    QVideoFrame frame(mappedBytes, size, bytesPerLine, pixelFormat);

    QVERIFY(!frame.bits());
    QCOMPARE(frame.mappedBytes(), 0);
    QCOMPARE(frame.bytesPerLine(), 0);
    QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);

    QVERIFY(frame.map(mode));

    // Mapping multiple times is allowed in ReadOnly mode
    if (mode == QAbstractVideoBuffer::ReadOnly) {
        const uchar *bits = frame.bits();

        QVERIFY(frame.map(QAbstractVideoBuffer::ReadOnly));
        QVERIFY(frame.isMapped());
        QCOMPARE(frame.bits(), bits);

        frame.unmap();
        //frame should still be mapped after the first nested unmap
        QVERIFY(frame.isMapped());
        QCOMPARE(frame.bits(), bits);

        //re-mapping in Write or ReadWrite modes should fail
        QVERIFY(!frame.map(QAbstractVideoBuffer::WriteOnly));
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadWrite));
    } else {
        // Mapping twice in ReadWrite or WriteOnly modes should fail, but leave it mapped (and the mode is ignored)
        QVERIFY(!frame.map(mode));
        QVERIFY(!frame.map(QAbstractVideoBuffer::ReadOnly));
    }

    QVERIFY(frame.bits());
    QCOMPARE(frame.mappedBytes(), mappedBytes);
    QCOMPARE(frame.bytesPerLine(), bytesPerLine);
    QCOMPARE(frame.mapMode(), mode);

    frame.unmap();

    QVERIFY(!frame.bits());
    QCOMPARE(frame.mappedBytes(), 0);
    QCOMPARE(frame.bytesPerLine(), 0);
    QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);
}

void tst_QVideoFrame::mapImage_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QImage::Format>("format");
    QTest::addColumn<QAbstractVideoBuffer::MapMode>("mode");

    QTest::newRow("read-only")
            << QSize(64, 64)
            << QImage::Format_ARGB32
            << QAbstractVideoBuffer::ReadOnly;

    QTest::newRow("write-only")
            << QSize(15, 106)
            << QImage::Format_RGB32
            << QAbstractVideoBuffer::WriteOnly;

    QTest::newRow("read-write")
            << QSize(23, 111)
            << QImage::Format_RGB16
            << QAbstractVideoBuffer::ReadWrite;
}

void tst_QVideoFrame::mapImage()
{
    QFETCH(QSize, size);
    QFETCH(QImage::Format, format);
    QFETCH(QAbstractVideoBuffer::MapMode, mode);

    QImage image(size.width(), size.height(), format);

    QVideoFrame frame(image);

    QVERIFY(!frame.bits());
    QCOMPARE(frame.mappedBytes(), 0);
    QCOMPARE(frame.bytesPerLine(), 0);
    QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);

    QVERIFY(frame.map(mode));

    QVERIFY(frame.bits());
    QCOMPARE(frame.mappedBytes(), image.byteCount());
    QCOMPARE(frame.bytesPerLine(), image.bytesPerLine());
    QCOMPARE(frame.mapMode(), mode);

    frame.unmap();

    QVERIFY(!frame.bits());
    QCOMPARE(frame.mappedBytes(), 0);
    QCOMPARE(frame.bytesPerLine(), 0);
    QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped);
}

void tst_QVideoFrame::mapPlanes_data()
{
    QTest::addColumn<QVideoFrame>("frame");
    QTest::addColumn<QList<int> >("strides");
    QTest::addColumn<QList<int> >("offsets");

    static uchar bufferData[1024];

    QtTestPlanarVideoBuffer *planarBuffer = new QtTestPlanarVideoBuffer;
    planarBuffer->m_data[0] = bufferData;
    planarBuffer->m_data[1] = bufferData + 512;
    planarBuffer->m_data[2] = bufferData + 765;
    planarBuffer->m_bytesPerLine[0] = 64;
    planarBuffer->m_bytesPerLine[1] = 36;
    planarBuffer->m_bytesPerLine[2] = 36;
    planarBuffer->m_planeCount = 3;
    planarBuffer->m_numBytes = sizeof(bufferData);

    QTest::newRow("Planar")
            << QVideoFrame(planarBuffer, QSize(64, 64), QVideoFrame::Format_YUV420P)
            << (QList<int>() << 64 << 36 << 36)
            << (QList<int>() << 512 << 765);
    QTest::newRow("Format_YUV420P")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_YUV420P)
            << (QList<int>() << 64 << 62 << 62)
            << (QList<int>() << 4096 << 6080);
    QTest::newRow("Format_YV12")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_YV12)
            << (QList<int>() << 64 << 62 << 62)
            << (QList<int>() << 4096 << 6080);
    QTest::newRow("Format_NV12")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_NV12)
            << (QList<int>() << 64 << 64)
            << (QList<int>() << 4096);
    QTest::newRow("Format_NV21")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_NV21)
            << (QList<int>() << 64 << 64)
            << (QList<int>() << 4096);
    QTest::newRow("Format_IMC2")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_IMC2)
            << (QList<int>() << 64 << 64)
            << (QList<int>() << 4096);
    QTest::newRow("Format_IMC4")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_IMC4)
            << (QList<int>() << 64 << 64)
            << (QList<int>() << 4096);
    QTest::newRow("Format_IMC1")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_IMC1)
            << (QList<int>() << 64 << 64 << 64)
            << (QList<int>() << 4096 << 6144);
    QTest::newRow("Format_IMC3")
            << QVideoFrame(8096, QSize(60, 64), 64, QVideoFrame::Format_IMC3)
            << (QList<int>() << 64 << 64 << 64)
            << (QList<int>() << 4096 << 6144);
    QTest::newRow("Format_ARGB32")
            << QVideoFrame(8096, QSize(60, 64), 256, QVideoFrame::Format_ARGB32)
            << (QList<int>() << 256)
            << (QList<int>());
}

void tst_QVideoFrame::mapPlanes()
{
    QFETCH(QVideoFrame, frame);
    QFETCH(QList<int>, strides);
    QFETCH(QList<int>, offsets);

    QCOMPARE(strides.count(), offsets.count() + 1);

    QCOMPARE(frame.map(QAbstractVideoBuffer::ReadOnly), true);
    QCOMPARE(frame.planeCount(), strides.count());

    QVERIFY(strides.count() > 0);
    QCOMPARE(frame.bytesPerLine(0), strides.at(0));
    QVERIFY(frame.bits(0));

    if (strides.count() > 1) {
        QCOMPARE(frame.bytesPerLine(1), strides.at(1));
        QCOMPARE(int(frame.bits(1) - frame.bits(0)), offsets.at(0));
    }
    if (strides.count() > 2) {
        QCOMPARE(frame.bytesPerLine(2), strides.at(2));
        QCOMPARE(int(frame.bits(2) - frame.bits(0)), offsets.at(1));
    }
    if (strides.count() > 3) {
        QCOMPARE(frame.bytesPerLine(3), strides.at(3));
        QCOMPARE(int(frame.bits(3) - frame.bits(0)), offsets.at(0));
    }

    frame.unmap();
}

void tst_QVideoFrame::imageDetach()
{
    const uint red = qRgb(255, 0, 0);
    const uint blue = qRgb(0, 0, 255);

    QImage image(8, 8, QImage::Format_RGB32);

    image.fill(red);
    QCOMPARE(image.pixel(4, 4), red);

    QVideoFrame frame(image);

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadWrite));

    QImage frameImage(frame.bits(), 8, 8, frame.bytesPerLine(), QImage::Format_RGB32);

    QCOMPARE(frameImage.pixel(4, 4), red);

    frameImage.fill(blue);
    QCOMPARE(frameImage.pixel(4, 4), blue);

    // Original image has detached and is therefore unchanged.
    QCOMPARE(image.pixel(4, 4), red);
}

void tst_QVideoFrame::formatConversion_data()
{
    QTest::addColumn<QImage::Format>("imageFormat");
    QTest::addColumn<QVideoFrame::PixelFormat>("pixelFormat");

    QTest::newRow("QImage::Format_RGB32 | QVideoFrame::Format_RGB32")
            << QImage::Format_RGB32
            << QVideoFrame::Format_RGB32;
    QTest::newRow("QImage::Format_ARGB32 | QVideoFrame::Format_ARGB32")
            << QImage::Format_ARGB32
            << QVideoFrame::Format_ARGB32;
    QTest::newRow("QImage::Format_ARGB32_Premultiplied | QVideoFrame::Format_ARGB32_Premultiplied")
            << QImage::Format_ARGB32_Premultiplied
            << QVideoFrame::Format_ARGB32_Premultiplied;
    QTest::newRow("QImage::Format_RGB16 | QVideoFrame::Format_RGB565")
            << QImage::Format_RGB16
            << QVideoFrame::Format_RGB565;
    QTest::newRow("QImage::Format_ARGB8565_Premultiplied | QVideoFrame::Format_ARGB8565_Premultiplied")
            << QImage::Format_ARGB8565_Premultiplied
            << QVideoFrame::Format_ARGB8565_Premultiplied;
    QTest::newRow("QImage::Format_RGB555 | QVideoFrame::Format_RGB555")
            << QImage::Format_RGB555
            << QVideoFrame::Format_RGB555;
    QTest::newRow("QImage::Format_RGB888 | QVideoFrame::Format_RGB24")
            << QImage::Format_RGB888
            << QVideoFrame::Format_RGB24;

    QTest::newRow("QImage::Format_MonoLSB")
            << QImage::Format_MonoLSB
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_Indexed8")
            << QImage::Format_Indexed8
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_ARGB6666_Premultiplied")
            << QImage::Format_ARGB6666_Premultiplied
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_ARGB8555_Premultiplied")
            << QImage::Format_ARGB8555_Premultiplied
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_RGB666")
            << QImage::Format_RGB666
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_RGB444")
            << QImage::Format_RGB444
            << QVideoFrame::Format_Invalid;
    QTest::newRow("QImage::Format_ARGB4444_Premultiplied")
            << QImage::Format_ARGB4444_Premultiplied
            << QVideoFrame::Format_Invalid;

    QTest::newRow("QVideoFrame::Format_BGRA32")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGRA32;
    QTest::newRow("QVideoFrame::Format_BGRA32_Premultiplied")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGRA32_Premultiplied;
    QTest::newRow("QVideoFrame::Format_BGR32")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGR32;
    QTest::newRow("QVideoFrame::Format_BGR24")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGR24;
    QTest::newRow("QVideoFrame::Format_BGR565")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGR565;
    QTest::newRow("QVideoFrame::Format_BGR555")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGR555;
    QTest::newRow("QVideoFrame::Format_BGRA5658_Premultiplied")
            << QImage::Format_Invalid
            << QVideoFrame::Format_BGRA5658_Premultiplied;
    QTest::newRow("QVideoFrame::Format_AYUV444")
            << QImage::Format_Invalid
            << QVideoFrame::Format_AYUV444;
    QTest::newRow("QVideoFrame::Format_AYUV444_Premultiplied")
            << QImage::Format_Invalid
            << QVideoFrame::Format_AYUV444_Premultiplied;
    QTest::newRow("QVideoFrame::Format_YUV444")
            << QImage::Format_Invalid
            << QVideoFrame::Format_YUV444;
    QTest::newRow("QVideoFrame::Format_YUV420P")
            << QImage::Format_Invalid
            << QVideoFrame::Format_YUV420P;
    QTest::newRow("QVideoFrame::Format_YV12")
            << QImage::Format_Invalid
            << QVideoFrame::Format_YV12;
    QTest::newRow("QVideoFrame::Format_UYVY")
            << QImage::Format_Invalid
            << QVideoFrame::Format_UYVY;
    QTest::newRow("QVideoFrame::Format_YUYV")
            << QImage::Format_Invalid
            << QVideoFrame::Format_YUYV;
    QTest::newRow("QVideoFrame::Format_NV12")
            << QImage::Format_Invalid
            << QVideoFrame::Format_NV12;
    QTest::newRow("QVideoFrame::Format_NV21")
            << QImage::Format_Invalid
            << QVideoFrame::Format_NV21;
    QTest::newRow("QVideoFrame::Format_IMC1")
            << QImage::Format_Invalid
            << QVideoFrame::Format_IMC1;
    QTest::newRow("QVideoFrame::Format_IMC2")
            << QImage::Format_Invalid
            << QVideoFrame::Format_IMC2;
    QTest::newRow("QVideoFrame::Format_IMC3")
            << QImage::Format_Invalid
            << QVideoFrame::Format_IMC3;
    QTest::newRow("QVideoFrame::Format_IMC4")
            << QImage::Format_Invalid
            << QVideoFrame::Format_IMC4;
    QTest::newRow("QVideoFrame::Format_Y8")
            << QImage::Format_Invalid
            << QVideoFrame::Format_Y8;
    QTest::newRow("QVideoFrame::Format_Y16")
            << QImage::Format_Invalid
            << QVideoFrame::Format_Y16;
    QTest::newRow("QVideoFrame::Format_Jpeg")
            << QImage::Format_Invalid
            << QVideoFrame::Format_Jpeg;
    QTest::newRow("QVideoFrame::Format_CameraRaw")
            << QImage::Format_Invalid
            << QVideoFrame::Format_CameraRaw;
    QTest::newRow("QVideoFrame::Format_AdobeDng")
            << QImage::Format_Invalid
            << QVideoFrame::Format_AdobeDng;
    QTest::newRow("QVideoFrame::Format_User")
            << QImage::Format_Invalid
            << QVideoFrame::Format_User;
    QTest::newRow("QVideoFrame::Format_User + 1")
            << QImage::Format_Invalid
            << QVideoFrame::PixelFormat(QVideoFrame::Format_User + 1);
}

void tst_QVideoFrame::formatConversion()
{
    QFETCH(QImage::Format, imageFormat);
    QFETCH(QVideoFrame::PixelFormat, pixelFormat);

    QCOMPARE(QVideoFrame::pixelFormatFromImageFormat(imageFormat) == pixelFormat,
             imageFormat != QImage::Format_Invalid);

    QCOMPARE(QVideoFrame::imageFormatFromPixelFormat(pixelFormat) == imageFormat,
             pixelFormat != QVideoFrame::Format_Invalid);
}

void tst_QVideoFrame::metadata()
{
    // Simple metadata test
    QVideoFrame f;

    QCOMPARE(f.availableMetaData(), QVariantMap());
    f.setMetaData("frob", QVariant("string"));
    f.setMetaData("bar", QVariant(42));
    QCOMPARE(f.metaData("frob"), QVariant("string"));
    QCOMPARE(f.metaData("bar"), QVariant(42));

    QVariantMap map;
    map.insert("frob", QVariant("string"));
    map.insert("bar", QVariant(42));

    QCOMPARE(f.availableMetaData(), map);

    f.setMetaData("frob", QVariant(56));
    QCOMPARE(f.metaData("frob"), QVariant(56));

    f.setMetaData("frob", QVariant());
    QCOMPARE(f.metaData("frob"), QVariant());

    QCOMPARE(f.availableMetaData().count(), 1);

    f.setMetaData("frob", QVariant("")); // empty but not null
    QCOMPARE(f.availableMetaData().count(), 2);
}

#define TEST_MAPPED(frame, mode) \
do { \
    QVERIFY(frame.bits()); \
    QVERIFY(frame.isMapped()); \
    QCOMPARE(frame.mappedBytes(), 16384); \
    QCOMPARE(frame.bytesPerLine(), 256); \
    QCOMPARE(frame.mapMode(), mode); \
} while (0)

#define TEST_UNMAPPED(frame) \
do { \
    QVERIFY(!frame.bits()); \
    QVERIFY(!frame.isMapped()); \
    QCOMPARE(frame.mappedBytes(), 0); \
    QCOMPARE(frame.bytesPerLine(), 0); \
    QCOMPARE(frame.mapMode(), QAbstractVideoBuffer::NotMapped); \
} while (0)

void tst_QVideoFrame::isMapped()
{
    QVideoFrame frame(16384, QSize(64, 64), 256,  QVideoFrame::Format_ARGB32);
    const QVideoFrame& constFrame(frame);

    TEST_UNMAPPED(frame);
    TEST_UNMAPPED(constFrame);

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadOnly));
    TEST_MAPPED(frame, QAbstractVideoBuffer::ReadOnly);
    TEST_MAPPED(constFrame, QAbstractVideoBuffer::ReadOnly);
    frame.unmap();
    TEST_UNMAPPED(frame);
    TEST_UNMAPPED(constFrame);

    QVERIFY(frame.map(QAbstractVideoBuffer::WriteOnly));
    TEST_MAPPED(frame, QAbstractVideoBuffer::WriteOnly);
    TEST_MAPPED(constFrame, QAbstractVideoBuffer::WriteOnly);
    frame.unmap();
    TEST_UNMAPPED(frame);
    TEST_UNMAPPED(constFrame);

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadWrite));
    TEST_MAPPED(frame, QAbstractVideoBuffer::ReadWrite);
    TEST_MAPPED(constFrame, QAbstractVideoBuffer::ReadWrite);
    frame.unmap();
    TEST_UNMAPPED(frame);
    TEST_UNMAPPED(constFrame);
}

void tst_QVideoFrame::isReadable()
{
    QVideoFrame frame(16384, QSize(64, 64), 256,  QVideoFrame::Format_ARGB32);

    QVERIFY(!frame.isMapped());
    QVERIFY(!frame.isReadable());

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadOnly));
    QVERIFY(frame.isMapped());
    QVERIFY(frame.isReadable());
    frame.unmap();

    QVERIFY(frame.map(QAbstractVideoBuffer::WriteOnly));
    QVERIFY(frame.isMapped());
    QVERIFY(!frame.isReadable());
    frame.unmap();

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadWrite));
    QVERIFY(frame.isMapped());
    QVERIFY(frame.isReadable());
    frame.unmap();
}

void tst_QVideoFrame::isWritable()
{
    QVideoFrame frame(16384, QSize(64, 64), 256,  QVideoFrame::Format_ARGB32);

    QVERIFY(!frame.isMapped());
    QVERIFY(!frame.isWritable());

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadOnly));
    QVERIFY(frame.isMapped());
    QVERIFY(!frame.isWritable());
    frame.unmap();

    QVERIFY(frame.map(QAbstractVideoBuffer::WriteOnly));
    QVERIFY(frame.isMapped());
    QVERIFY(frame.isWritable());
    frame.unmap();

    QVERIFY(frame.map(QAbstractVideoBuffer::ReadWrite));
    QVERIFY(frame.isMapped());
    QVERIFY(frame.isWritable());
    frame.unmap();
}

QTEST_MAIN(tst_QVideoFrame)

#include "tst_qvideoframe.moc"
