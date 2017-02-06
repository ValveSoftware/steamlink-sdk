/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the MNG benchmarks in the Qt ImageFormats module.
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

class tst_qmng: public QObject
{
    Q_OBJECT

private slots:
    void readImage_data();
    void readImage();
    void readCorruptImage_data();
    void readCorruptImage();
};

void tst_qmng::readImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("size");

    QTest::newRow("animation") << QString("animation.mng") << QSize(100, 100);
    QTest::newRow("ball") << QString("ball.mng") << QSize(32, 32);
    QTest::newRow("dutch") << QString("dutch.mng") << QSize(352, 264);
    QTest::newRow("fire") << QString("fire.mng") << QSize(30, 60);
}

void tst_qmng::readImage()
{
    QFETCH(QString, fileName);
    QFETCH(QSize, size);

    QString path = QString(":/mng/") + fileName;
    QBENCHMARK {
        QImageReader reader(path);
        QVERIFY(reader.canRead());
        QImage image = reader.read();
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), size);
    }
}

void tst_qmng::readCorruptImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("message");

    QTest::newRow("corrupt")
            << QString("corrupt.mng")
            << QString("MNG error 901: Application signalled I/O error; chunk IHDR; subcode 0:0");
}

void tst_qmng::readCorruptImage()
{
    QFETCH(QString, fileName);
    QFETCH(QString, message);

    QString path = QString(":/mng/") + fileName;
    QBENCHMARK {
        QImageReader reader(path);
        if (!message.isEmpty())
            QTest::ignoreMessage(QtWarningMsg, message.toLatin1());
        QVERIFY(reader.canRead());
        QImage image = reader.read();
        QVERIFY(image.isNull());
    }
}

QTEST_MAIN(tst_qmng)
#include "tst_qmng.moc"
