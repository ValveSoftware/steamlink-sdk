/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>
#include "bytearrayclass.h"

static QString readFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
        return QString();
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    return stream.readAll();
}

class tst_QScriptClass_ByteArray : public QObject
{
    Q_OBJECT

public:
    tst_QScriptClass_ByteArray();

private slots:
    void benchmark_data();
    void benchmark();

private:
    QDir testsDir;
};

tst_QScriptClass_ByteArray::tst_QScriptClass_ByteArray()
{
    testsDir = QDir(":/tests");
    if (!testsDir.exists())
        qWarning("*** no tests/ dir!");
}

void tst_QScriptClass_ByteArray::benchmark_data()
{
    QTest::addColumn<QString>("testName");
    const QFileInfoList testFileInfos = testsDir.entryInfoList(QStringList() << "*.js", QDir::Files);
    for (const QFileInfo &tfi : testFileInfos) {
        QString name = tfi.baseName();
        QTest::newRow(name.toLatin1().constData()) << name;
    }
}

void tst_QScriptClass_ByteArray::benchmark()
{
    QFETCH(QString, testName);
    QString testContents = readFile(testsDir.absoluteFilePath(testName + ".js"));
    QVERIFY(!testContents.isEmpty());

    QScriptEngine eng;
    ByteArrayClass *baClass = new ByteArrayClass(&eng);
    eng.globalObject().setProperty("ByteArray", baClass->constructor());

    QBENCHMARK {
        eng.evaluate(testContents);
    }
    QVERIFY(!eng.hasUncaughtException());
}

QTEST_MAIN(tst_QScriptClass_ByteArray)
#include "tst_qscriptclass_bytearray.moc"
