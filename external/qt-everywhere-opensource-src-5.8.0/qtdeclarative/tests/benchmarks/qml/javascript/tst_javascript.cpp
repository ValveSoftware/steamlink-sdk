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
