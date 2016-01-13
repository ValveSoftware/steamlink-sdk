/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDebug>
#include <QDir>
#include <QFile>

class tst_parserstress : public QObject
{
    Q_OBJECT
public:
    tst_parserstress() {}

private slots:
    void ecmascript_data();
    void ecmascript();

private:
    static QStringList findJSFiles(const QDir &);
    QDeclarativeEngine engine;
};

QStringList tst_parserstress::findJSFiles(const QDir &d)
{
    QStringList rv;

    QStringList files = d.entryList(QStringList() << QLatin1String("*.js"),
                                    QDir::Files);
    foreach (const QString &file, files) {
        if (file == "browser.js")
            continue;
        rv << d.absoluteFilePath(file);
    }

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                   QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        QDir sub = d;
        sub.cd(dir);
        rv << findJSFiles(sub);
    }

    return rv;
}

void tst_parserstress::ecmascript_data()
{
    QDir dir(SRCDIR "../../qscriptjstestsuite/tests");
    QStringList files = findJSFiles(dir);

    QTest::addColumn<QString>("file");
    foreach (const QString &file, files) {
        QTest::newRow(qPrintable(file)) << file;
    }
}

void tst_parserstress::ecmascript()
{
    QSKIP("Test data (located in the QtScript module) not currently available");

    QFETCH(QString, file);

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));

    QByteArray data = f.readAll();

    QVERIFY(!data.isEmpty());

    QString dataStr = QString::fromUtf8(data);

    QString qml = "import QtQuick 1.0\n";
            qml+= "\n";
            qml+= "QtObject {\n";
            qml+= "    property int test\n";
            qml+= "    test: {\n";
            qml+= dataStr + "\n";
            qml+= "        return 1;\n";
            qml+= "    }\n";
            qml+= "    function stress() {\n";
            qml+= dataStr;
            qml+= "    }\n";
            qml+= "}\n";

    QByteArray qmlData = qml.toUtf8();

    QDeclarativeComponent component(&engine);

    component.setData(qmlData, QUrl::fromLocalFile(SRCDIR + QString("/dummy.qml")));

    QFileInfo info(file);

    if (info.fileName() == QLatin1String("regress-352044-02-n.js")) {
        QVERIFY(component.isError());

        QCOMPARE(component.errors().length(), 2);

        QCOMPARE(component.errors().at(0).description(), QString("Expected token `;'"));
        QCOMPARE(component.errors().at(0).line(), 66);

        QCOMPARE(component.errors().at(1).description(), QString("Expected token `;'"));
        QCOMPARE(component.errors().at(1).line(), 142);

    } else {

        QVERIFY(!component.isError());
    }
}


QTEST_MAIN(tst_parserstress)

#include "tst_parserstress.moc"
