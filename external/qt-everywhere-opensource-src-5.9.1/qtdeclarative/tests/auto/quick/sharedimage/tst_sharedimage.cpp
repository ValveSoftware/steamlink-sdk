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

#include <QtTest>
#include <private/qquickimage_p.h>
#include <QQmlApplicationEngine>

class tst_sharedimage : public QObject
{
    Q_OBJECT
public:
    tst_sharedimage()
    {
    }

private slots:
    void initTestCase();
    void compareToPlainLoad_data();
    void compareToPlainLoad();
};

void tst_sharedimage::initTestCase()
{
#if !QT_CONFIG(systemsemaphore)
    QSKIP("Shared image not supported");
#endif
}

void tst_sharedimage::compareToPlainLoad_data()
{
    QString imagePath = QFINDTESTDATA("data/yellow.png");
    if (imagePath.startsWith(QLatin1Char('/')))
        imagePath.remove(0, 1);
    QString plainImage("Image { source: \"file:///%1\"; cache: false; %2 }");
    QString sharedImage("Image { source: \"image://shared/%1\"; cache: false; %2 }");
    QString script("import QtQuick 2.0\nimport Qt.labs.sharedimage 1.0\n%1\n");

    QString plain = script.arg(plainImage).arg(imagePath);
    QString shared = script.arg(sharedImage).arg(imagePath);

    QTest::addColumn<QByteArray>("plainScript");
    QTest::addColumn<QByteArray>("sharedScript");

    QString opts = QStringLiteral("asynchronous: false;");
    QTest::newRow("sync") << plain.arg(opts).toLatin1() << shared.arg(opts).toLatin1();

    opts = QStringLiteral("asynchronous: true");
    QTest::newRow("async") << plain.arg(opts).toLatin1() << shared.arg(opts).toLatin1();

    opts = QStringLiteral("sourceSize: Qt.size(50, 50)");
    QTest::newRow("scaled, stretch") << plain.arg(opts).toLatin1() << shared.arg(opts).toLatin1();

    opts = QStringLiteral("sourceSize: Qt.size(50, 50); fillMode: Image.PreserveAspectFit");
    QTest::newRow("scaled, aspectfit") << plain.arg(opts).toLatin1() << shared.arg(opts).toLatin1();
}

void tst_sharedimage::compareToPlainLoad()
{
    QFETCH(QByteArray, plainScript);
    QFETCH(QByteArray, sharedScript);

    QImage images[2];
    for (int i = 0; i < 2; i++) {
        QQmlApplicationEngine engine;
        engine.loadData(i ? sharedScript : plainScript);
        QVERIFY(engine.rootObjects().size());
        QQuickImage *obj = qobject_cast<QQuickImage*>(engine.rootObjects().at(0));
        QVERIFY(obj != 0);
        QTRY_VERIFY(!obj->image().isNull());
        images[i] = obj->image();
    }

    QCOMPARE(images[1], images[0].convertToFormat(images[1].format()));
}

QTEST_MAIN(tst_sharedimage)

#include "tst_sharedimage.moc"
