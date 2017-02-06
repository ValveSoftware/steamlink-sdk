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

#include <QtTest/QtTest>
#include <QtPositioning/QGeoShape>
#include <QtCore/QDebug>
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>

QString tst_qgeoshape_debug;

void tst_qgeoshape_messageHandler(QtMsgType type, const QMessageLogContext&,
                                  const QString &msg)
{
    switch (type) {
        case QtDebugMsg :
            tst_qgeoshape_debug = msg;
            break;
        default:
            break;
    }
}

class tst_qgeoshape : public QObject
{
    Q_OBJECT

private slots:
    void testArea();
    void debug_data();
    void debug();
    void conversions();
};

void tst_qgeoshape::testArea()
{
    QGeoShape area;
    QVERIFY(!area.isValid());
    QVERIFY(area.isEmpty());
    QCOMPARE(area.type(), QGeoShape::UnknownType);
    QVERIFY(!area.contains(QGeoCoordinate()));

    // QGeoShape never constructs a QGeoShapePrivate.  Hence d_ptr is always 0.

    QGeoShape area2;

    QCOMPARE(area, area2);

    area = area2;

    QCOMPARE(area, area2);

    QGeoShape area3(area2);

    QCOMPARE(area2, area3);
}

void tst_qgeoshape::debug_data()
{
    QTest::addColumn<QGeoShape>("shape");
    QTest::addColumn<int>("nextValue");
    QTest::addColumn<QString>("debugString");

    QTest::newRow("uninitialized") << QGeoShape() << 45
            << QString("QGeoShape(Unknown) 45");
    QTest::newRow("uninitialized") << QGeoShape(QGeoRectangle()) << 45
            << QString("QGeoShape(Rectangle) 45");
    QTest::newRow("uninitialized") << QGeoShape(QGeoCircle()) << 45
            << QString("QGeoShape(Circle) 45");
}


void tst_qgeoshape::debug()
{
    QFETCH(QGeoShape, shape);
    QFETCH(int, nextValue);
    QFETCH(QString, debugString);

    qInstallMessageHandler(tst_qgeoshape_messageHandler);
    qDebug() << shape << nextValue;
    qInstallMessageHandler(0);
    QCOMPARE(tst_qgeoshape_debug, debugString);
}

void tst_qgeoshape::conversions()
{
    QVariant varShape = QVariant::fromValue(QGeoShape());
    QVariant varRect = QVariant::fromValue(QGeoRectangle(
                                               QGeoCoordinate(1, 1),
                                               QGeoCoordinate(2, 2)));
    QVariant varCircle = QVariant::fromValue(QGeoCircle(QGeoCoordinate(3, 3), 1000));

    QVERIFY(varShape.canConvert<QGeoShape>());
    QVERIFY(varShape.canConvert<QGeoCircle>());
    QVERIFY(varShape.canConvert<QGeoRectangle>());
    QVERIFY(!varRect.canConvert<QGeoCircle>());
    QVERIFY(varRect.canConvert<QGeoRectangle>());
    QVERIFY(varRect.canConvert<QGeoShape>());
    QVERIFY(varCircle.canConvert<QGeoCircle>());
    QVERIFY(!varCircle.canConvert<QGeoRectangle>());
    QVERIFY(varCircle.canConvert<QGeoShape>());
}

QTEST_MAIN(tst_qgeoshape)
#include "tst_qgeoshape.moc"
