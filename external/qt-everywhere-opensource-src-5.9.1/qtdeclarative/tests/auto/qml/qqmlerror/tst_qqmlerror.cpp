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
#include <QQmlError>
#include <QDebug>
#include "../../shared/util.h"

class tst_qqmlerror : public QQmlDataTest
{
    Q_OBJECT
private slots:
    void url();
    void description();
    void line();
    void column();
    void toString();

    void copy();
    void debug();
};

void tst_qqmlerror::url()
{
    QQmlError error;

    QCOMPARE(error.url(), QUrl());

    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));

    QCOMPARE(error.url(), QUrl("http://www.qt-project.org/main.qml"));

    QQmlError error2 = error;

    QCOMPARE(error2.url(), QUrl("http://www.qt-project.org/main.qml"));

    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));

    QCOMPARE(error.url(), QUrl("http://www.qt-project.org/main.qml"));
    QCOMPARE(error2.url(), QUrl("http://www.qt-project.org/main.qml"));
}

void tst_qqmlerror::description()
{
    QQmlError error;

    QCOMPARE(error.description(), QString());

    error.setDescription("An Error");

    QCOMPARE(error.description(), QString("An Error"));

    QQmlError error2 = error;

    QCOMPARE(error2.description(), QString("An Error"));

    error.setDescription("Another Error");

    QCOMPARE(error.description(), QString("Another Error"));
    QCOMPARE(error2.description(), QString("An Error"));
}

void tst_qqmlerror::line()
{
    QQmlError error;

    QCOMPARE(error.line(), -1);

    error.setLine(102);

    QCOMPARE(error.line(), 102);

    QQmlError error2 = error;

    QCOMPARE(error2.line(), 102);

    error.setLine(4);

    QCOMPARE(error.line(), 4);
    QCOMPARE(error2.line(), 102);
}

void tst_qqmlerror::column()
{
    QQmlError error;

    QCOMPARE(error.column(), -1);

    error.setColumn(16);

    QCOMPARE(error.column(), 16);

    QQmlError error2 = error;

    QCOMPARE(error2.column(), 16);

    error.setColumn(3);

    QCOMPARE(error.column(), 3);
    QCOMPARE(error2.column(), 16);
}

void tst_qqmlerror::toString()
{
    {
        QQmlError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);
        error.setColumn(13);

        QCOMPARE(error.toString(), QString("http://www.qt-project.org/main.qml:92:13: An Error"));
    }

    {
        QQmlError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);

        QCOMPARE(error.toString(), QString("http://www.qt-project.org/main.qml:92: An Error"));
    }
}

void tst_qqmlerror::copy()
{
    QQmlError error;
    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
    error.setDescription("An Error");
    error.setLine(92);
    error.setColumn(13);

    QQmlError error2(error);
    QQmlError error3;
    error3 = error;

    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
    error.setDescription("Another Error");
    error.setLine(2);
    error.setColumn(33);

    QCOMPARE(error.url(), QUrl("http://www.qt-project.org/main.qml"));
    QCOMPARE(error.description(), QString("Another Error"));
    QCOMPARE(error.line(), 2);
    QCOMPARE(error.column(), 33);

    QCOMPARE(error2.url(), QUrl("http://www.qt-project.org/main.qml"));
    QCOMPARE(error2.description(), QString("An Error"));
    QCOMPARE(error2.line(), 92);
    QCOMPARE(error2.column(), 13);

    QCOMPARE(error3.url(), QUrl("http://www.qt-project.org/main.qml"));
    QCOMPARE(error3.description(), QString("An Error"));
    QCOMPARE(error3.line(), 92);
    QCOMPARE(error3.column(), 13);

}

void tst_qqmlerror::debug()
{
    {
        QQmlError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);
        error.setColumn(13);

        QTest::ignoreMessage(QtWarningMsg, "http://www.qt-project.org/main.qml:92:13: An Error");
        qWarning() << error;
    }

    {
        QUrl url(dataDirectoryUrl().resolved(QUrl("test.txt")));
        QQmlError error;
        error.setUrl(url);
        error.setDescription("An Error");
        error.setLine(2);
        error.setColumn(5);

        QString out = url.toString() + ":2:5: An Error \n     Line2 Content \n         ^ ";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(out));

        qWarning() << error;
    }

    {
        QUrl url(dataDirectoryUrl().resolved(QUrl("foo.txt")));
        QQmlError error;
        error.setUrl(url);
        error.setDescription("An Error");
        error.setLine(2);
        error.setColumn(5);

        QString out = url.toString() + ":2:5: An Error ";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(out));

        qWarning() << error;
    }
}



QTEST_MAIN(tst_qqmlerror)

#include "tst_qqmlerror.moc"
