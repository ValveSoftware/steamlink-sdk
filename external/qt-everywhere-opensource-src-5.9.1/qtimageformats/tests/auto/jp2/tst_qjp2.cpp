/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Petroules Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the MNG autotests in the Qt ImageFormats module.
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

class tst_qjp2: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readImage_data();
    void readImage();
};

void tst_qjp2::initTestCase()
{
    if (!QImageReader::supportedImageFormats().contains("jp2"))
        QSKIP("The image format handler is not installed.");
}

void tst_qjp2::readImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("referenceFileName");
    QTest::addColumn<QSize>("size");

    QTest::newRow("logo") << QString("logo.jp2") << QString("logo.bmp") << QSize(498, 80);
}

void tst_qjp2::readImage()
{
    QFETCH(QString, fileName);
    QFETCH(QString, referenceFileName);
    QFETCH(QSize, size);

    QString path = QString(":/jp2/") + fileName;
    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QImage image = reader.read();
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), size);

    path = QString(":jp2/") + referenceFileName;
    QImageReader referenceReader(path);
    QVERIFY(referenceReader.canRead());
    QImage referenceImage = referenceReader.read();
    QVERIFY(!referenceImage.isNull());
    QCOMPARE(referenceImage.size(), size);
}

QTEST_MAIN(tst_qjp2)
#include "tst_qjp2.moc"
