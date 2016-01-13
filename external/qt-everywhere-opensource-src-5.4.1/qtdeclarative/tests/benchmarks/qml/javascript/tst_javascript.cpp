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

#include <QDir>
#include <QDebug>
#include <qtest.h>
#include <QQmlEngine>
#include <QQmlComponent>

#include "testtypes.h"

class tst_javascript : public QObject
{
    Q_OBJECT

public:
    tst_javascript();
    virtual ~tst_javascript();

private slots:
    void run_data();
    void run();

private:
    QQmlEngine engine;
};

tst_javascript::tst_javascript()
{
    registerTypes();
}

tst_javascript::~tst_javascript()
{
}

void tst_javascript::run_data()
{
    QTest::addColumn<QString>("file");

    QDir dir(SRCDIR "/data");

    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    for (int ii = 0; ii < files.count(); ++ii) {
        QString file = files.at(ii);
        if (file.endsWith(".qml") && file.at(0).isLower()) {

            QString testName = file.left(file.length() - 4 /* strlen(".qml") */);
            QString fileName = QLatin1String(SRCDIR) + QLatin1String("/data/") + file;


            QTest::newRow(qPrintable(testName)) << fileName;

        }
    }
}

void tst_javascript::run()
{
    QFETCH(QString, file);

    QQmlComponent c(&engine, file);

    if (c.isError()) {
        qWarning() << c.errors();
    }

    QVERIFY(!c.isError());

    QObject *o = c.create();
    QVERIFY(o != 0);

    QMetaMethod method = o->metaObject()->method(o->metaObject()->indexOfMethod("runtest()"));

    QBENCHMARK {
        method.invoke(o);
    }

    delete o;
}

QTEST_MAIN(tst_javascript)

#include "tst_javascript.moc"
