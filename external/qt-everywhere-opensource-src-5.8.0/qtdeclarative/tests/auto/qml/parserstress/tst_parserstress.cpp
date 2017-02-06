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

#include <qtest.h>
#include <QQmlEngine>
#include <QQmlComponent>
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
    QQmlEngine engine;
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
    QString testDataDir = QFileInfo(QFINDTESTDATA("tests/shell.js")).absolutePath();
    QVERIFY2(!testDataDir.isEmpty(), qPrintable("Cannot find testDataDir!"));

    QDir dir(testDataDir);
    QStringList files = findJSFiles(dir);

    QTest::addColumn<QString>("file");
    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;
}

void tst_parserstress::ecmascript()
{
    QFETCH(QString, file);

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));

    QByteArray data = f.readAll();

    QVERIFY(!data.isEmpty());

    QString dataStr = QString::fromUtf8(data);

    QString qml = "import QtQuick 2.0\n";
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

    QQmlComponent component(&engine);

    component.setData(qmlData, QUrl());

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
