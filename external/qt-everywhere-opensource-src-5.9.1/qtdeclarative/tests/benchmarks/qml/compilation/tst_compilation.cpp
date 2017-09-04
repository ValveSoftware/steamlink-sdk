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

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/private/qqmljsengine_p.h>
#include <QtQml/private/qqmljsmemorypool_p.h>
#include <QtQml/private/qqmljsparser_p.h>
#include <QtQml/private/qqmljslexer_p.h>

#include <QFile>
#include <QDebug>
#include <QTextStream>

class tst_compilation : public QObject
{
    Q_OBJECT
public:
    tst_compilation();

private slots:
    void boomblock();

    void jsparser_data();
    void jsparser();

    void bigimport_data();
    void bigimport();

private:
    QQmlEngine engine;
};

tst_compilation::tst_compilation()
{
}

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}

void tst_compilation::boomblock()
{
    QFile f(SRCDIR + QLatin1String("/data/BoomBlock.qml"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    //get rid of initialization effects
    {
        QQmlComponent c(&engine);
        c.setData(data, QUrl());
    }
#if !QT_CONFIG(opengl)
    QSKIP("boomblock imports Particles which requires OpenGL Support");
#endif
    QBENCHMARK {
        QQmlComponent c(&engine);
        c.setData(data, QUrl());
        QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    }
}

void tst_compilation::jsparser_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("boomblock") << QString(SRCDIR + QLatin1String("/data/BoomBlock.qml"));
}

void tst_compilation::jsparser()
{
    QFETCH(QString, file);

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    QTextStream stream(data, QIODevice::ReadOnly);
    const QString code = stream.readAll();

    QBENCHMARK {
        QQmlJS::Engine engine;

        QQmlJS::Lexer lexer(&engine);
        lexer.setCode(code, -1);

        QQmlJS::Parser parser(&engine);
        parser.parse();
        parser.ast();
    }
}

void tst_compilation::bigimport_data()
{
    QTest::addColumn<int>("filesToCreate");
    QTest::addColumn<bool>("writeQmldir");

    QTest::newRow("10, qmldir")
        << 10 << true;
    QTest::newRow("100, qmldir")
        << 100 << true;
    QTest::newRow("1000, qmldir")
        << 1000 << true;

    QTest::newRow("10, noqmldir")
        << 10 << false;
    QTest::newRow("100, noqmldir")
        << 100 << false;
    QTest::newRow("1000, noqmldir")
        << 1000 << false;
}

void tst_compilation::bigimport()
{
    QFETCH(int, filesToCreate);
    QFETCH(bool, writeQmldir);
    QTemporaryDir d;
    //d.setAutoRemove(false); // for debugging

    QString p;
    {
        for (int i = 0; i < filesToCreate; ++i) {
            QFile f(d.path() + QDir::separator() + QString::fromLatin1("Type%1.qml").arg(i));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("import QtQuick 2.0\n");
            f.write("import \".\"\n");
            f.write("Item {}\n");
        }

        QFile qmldir(d.path() + QDir::separator() + "qmldir");
        if (writeQmldir)
            QVERIFY(qmldir.open(QIODevice::WriteOnly));
        QFile main(d.path() + QDir::separator() + "main.qml");
        QVERIFY(main.open(QIODevice::WriteOnly));
        p = QFileInfo(main).absoluteFilePath();
        //qDebug() << p; // for debugging

        main.write("import QtQuick 2.0\n");
        main.write("import \".\"\n");
        main.write("\n");
        main.write("Item {\n");

        for (int i = 0; i < filesToCreate; ++i) {
            main.write(qPrintable(QString::fromLatin1("Type%1 {}\n").arg(i)));
            if (writeQmldir)
                qmldir.write(qPrintable(QString::fromLatin1("Type%1 1.0 Type%1.qml\n").arg(i)));
        }

        main.write("}");
    }

    QBENCHMARK {
        QQmlEngine e;
        QQmlComponent c(&e, p);
        QCOMPARE(c.status(), QQmlComponent::Ready);
        QScopedPointer<QObject> o(c.create());
        QVERIFY(o->children().count() == filesToCreate);
    }
}

QTEST_MAIN(tst_compilation)

#include "tst_compilation.moc"
