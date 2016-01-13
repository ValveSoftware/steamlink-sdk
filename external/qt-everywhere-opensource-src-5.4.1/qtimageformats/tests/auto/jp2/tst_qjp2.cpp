/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2014 Petroules Corporation.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the MNG autotests in the Qt ImageFormats module.
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

#include <QtTest/QtTest>
#include <QtGui/QtGui>

class tst_qjp2: public QObject
{
    Q_OBJECT

private slots:
    void readImage_data();
    void readImage();
};

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

    QCOMPARE(image, referenceImage);
}

QTEST_MAIN(tst_qjp2)
#include "tst_qjp2.moc"
