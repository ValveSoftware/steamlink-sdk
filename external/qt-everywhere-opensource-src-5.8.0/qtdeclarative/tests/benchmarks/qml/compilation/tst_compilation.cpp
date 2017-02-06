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
#ifdef QT_NO_OPENGL
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

QTEST_MAIN(tst_compilation)

#include "tst_compilation.moc"
