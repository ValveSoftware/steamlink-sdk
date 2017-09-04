/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Ivan Komissarov.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the DDS plugin in the Qt ImageFormats module.
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

#include <QtTest/QtTest>
#include <QtGui/QtGui>

class tst_qdds: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readImage_data();
    void readImage();
    void testMipmaps_data();
    void testMipmaps();
    void testWriteImage_data();
    void testWriteImage();
};

void tst_qdds::initTestCase()
{
    if (!QImageReader::supportedImageFormats().contains("dds"))
        QSKIP("The image format handler is not installed.");
}

void tst_qdds::readImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("size");

    QTest::newRow("1") << QString("A1R5G5B5") << QSize(64, 64);
    QTest::newRow("2") << QString("A2B10G10R10") << QSize(64, 64);
    QTest::newRow("3") << QString("A2R10G10B10") << QSize(64, 64);
    QTest::newRow("4") << QString("A2W10V10U10") << QSize(64, 64);
    QTest::newRow("5") << QString("A4L4") << QSize(64, 64);
    QTest::newRow("6") << QString("A4R4G4B4") << QSize(64, 64);
    QTest::newRow("7") << QString("A8") << QSize(64, 64);
    QTest::newRow("8") << QString("A8B8G8R8") << QSize(64, 64);
    QTest::newRow("9") << QString("A8L8") << QSize(64, 64);
    QTest::newRow("10") << QString("A8R3G3B2") << QSize(64, 64);
    QTest::newRow("11") << QString("A8R8G8B8") << QSize(64, 64);
    QTest::newRow("12") << QString("A16B16G16R16") << QSize(64, 64);
    QTest::newRow("13") << QString("A16B16G16R16F") << QSize(64, 64);
    QTest::newRow("14") << QString("A32B32G32R32F") << QSize(64, 64);
    QTest::newRow("15") << QString("CxV8U8") << QSize(64, 64);
    QTest::newRow("16") << QString("DXT1") << QSize(50, 50);
    QTest::newRow("17") << QString("DXT2") << QSize(64, 64);
    QTest::newRow("18") << QString("DXT3") << QSize(64, 64);
    QTest::newRow("19") << QString("DXT4") << QSize(64, 64);
    QTest::newRow("20") << QString("DXT5") << QSize(64, 64);
    QTest::newRow("21") << QString("G8R8_G8B8") << QSize(64, 64);
    QTest::newRow("22") << QString("G16R16") << QSize(64, 64);
    QTest::newRow("23") << QString("G16R16F") << QSize(64, 64);
    QTest::newRow("24") << QString("G32R32F") << QSize(64, 64);
    QTest::newRow("25") << QString("L6V5U5") << QSize(64, 64);
    QTest::newRow("26") << QString("L8") << QSize(64, 64);
    QTest::newRow("27") << QString("L16") << QSize(64, 64);
    QTest::newRow("28") << QString("P8") << QSize(64, 64);
    QTest::newRow("29") << QString("Q8W8V8U8") << QSize(64, 64);
    QTest::newRow("30") << QString("Q16W16V16U16") << QSize(64, 64);
    QTest::newRow("31") << QString("R3G3B2") << QSize(64, 64);
    QTest::newRow("32") << QString("R5G6B5") << QSize(64, 64);
    QTest::newRow("33") << QString("R8G8_B8G8") << QSize(64, 64);
    QTest::newRow("34") << QString("R8G8B8") << QSize(64, 64);
    QTest::newRow("35") << QString("R16F") << QSize(64, 64);
    QTest::newRow("36") << QString("R32F") << QSize(64, 64);
    QTest::newRow("37") << QString("UYVY") << QSize(64, 64);
    QTest::newRow("38") << QString("V8U8") << QSize(64, 64);
    QTest::newRow("39") << QString("V16U16") << QSize(64, 64);
    QTest::newRow("40") << QString("X1R5G5B5") << QSize(64, 64);
    QTest::newRow("41") << QString("X4R4G4B4") << QSize(64, 64);
    QTest::newRow("42") << QString("X8B8G8R8") << QSize(64, 64);
    QTest::newRow("43") << QString("X8L8V8U8") << QSize(64, 64);
    QTest::newRow("44") << QString("X8R8G8B8") << QSize(64, 64);
    QTest::newRow("45") << QString("YUY2") << QSize(64, 64);
    QTest::newRow("46") << QString("RXGB") << QSize(64, 64);
    QTest::newRow("47") << QString("ATI2") << QSize(64, 64);
    QTest::newRow("48") << QString("P4") << QSize(64, 64);
    QTest::newRow("49") << QString("A8R8G8B8.2") << QSize(64, 32);
}

void tst_qdds::readImage()
{
    QFETCH(QString, fileName);
    QFETCH(QSize, size);

    const QString path = QStringLiteral(":/dds/") + fileName + QStringLiteral(".dds");
    const QByteArray subType = fileName.left(fileName.lastIndexOf(QLatin1Char('.'))).toLatin1();
    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QVERIFY(reader.supportsOption(QImageIOHandler::SubType));
    QCOMPARE(reader.subType(), subType);
    QVERIFY(reader.supportsOption(QImageIOHandler::SupportedSubTypes));
    QImage image = reader.read();
    QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
    QCOMPARE(image.size(), size);
}

void tst_qdds::testMipmaps_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("imageCount");

    QTest::newRow("1") << QString("mipmaps") << QSize(64, 64) << 7;
}

void tst_qdds::testMipmaps()
{
    QFETCH(QString, fileName);
    QFETCH(QSize, size);
    QFETCH(int, imageCount);

    const QString path = QStringLiteral(":/dds/") + fileName + QStringLiteral(".dds");
    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.imageCount(), imageCount);

    for (int i = 0; i < reader.imageCount(); ++i) {
        reader.jumpToImage(i);
        QImage image = reader.read();
        QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
        QCOMPARE(image.size(), size / (1 << i));
    }
}

void tst_qdds::testWriteImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("size");

    QTest::newRow("1") << QString("A8R8G8B8") << QSize(64, 64);
    QTest::newRow("2") << QString("A8R8G8B8.2") << QSize(64, 32);
}

void tst_qdds::testWriteImage()
{
    QFETCH(QString, fileName);
    QFETCH(QSize, size);

    const QString path = fileName + QStringLiteral(".dds");
    const QString sourcePath = QStringLiteral(":/dds/") + fileName + QStringLiteral(".dds");
    const QByteArray subType = fileName.left(fileName.lastIndexOf(QLatin1Char('.'))).toLatin1();

    QImage image(sourcePath);
    QVERIFY(!image.isNull());
    QVERIFY(image.size() == size);

    QImageWriter writer(path, QByteArrayLiteral("dds"));
    QVERIFY2(writer.canWrite(), qPrintable(writer.errorString()));
    writer.setSubType(subType);
    QVERIFY2(writer.write(image), qPrintable(writer.errorString()));

    QVERIFY(image == QImage(path));

    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.size(), size);
    QCOMPARE(reader.subType(), subType);
}

QTEST_MAIN(tst_qdds)
#include "tst_qdds.moc"
