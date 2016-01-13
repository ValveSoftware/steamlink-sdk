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
#include <QDeclarativeError>
#include <QDebug>

class tst_qdeclarativeerror : public QObject
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

void tst_qdeclarativeerror::url()
{
    QDeclarativeError error;

    QCOMPARE(error.url(), QUrl());

    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));

    QCOMPARE(error.url(), QUrl("http://www.qt-project.org/main.qml"));

    QDeclarativeError error2 = error;

    QCOMPARE(error2.url(), QUrl("http://www.qt-project.org/main.qml"));

    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));

    QCOMPARE(error.url(), QUrl("http://www.qt-project.org/main.qml"));
    QCOMPARE(error2.url(), QUrl("http://www.qt-project.org/main.qml"));
}

void tst_qdeclarativeerror::description()
{
    QDeclarativeError error;

    QCOMPARE(error.description(), QString());

    error.setDescription("An Error");

    QCOMPARE(error.description(), QString("An Error"));

    QDeclarativeError error2 = error;

    QCOMPARE(error2.description(), QString("An Error"));

    error.setDescription("Another Error");

    QCOMPARE(error.description(), QString("Another Error"));
    QCOMPARE(error2.description(), QString("An Error"));
}

void tst_qdeclarativeerror::line()
{
    QDeclarativeError error;

    QCOMPARE(error.line(), -1);

    error.setLine(102);

    QCOMPARE(error.line(), 102);

    QDeclarativeError error2 = error;

    QCOMPARE(error2.line(), 102);

    error.setLine(4);

    QCOMPARE(error.line(), 4);
    QCOMPARE(error2.line(), 102);
}

void tst_qdeclarativeerror::column()
{
    QDeclarativeError error;

    QCOMPARE(error.column(), -1);

    error.setColumn(16);

    QCOMPARE(error.column(), 16);

    QDeclarativeError error2 = error;

    QCOMPARE(error2.column(), 16);

    error.setColumn(3);

    QCOMPARE(error.column(), 3);
    QCOMPARE(error2.column(), 16);
}

void tst_qdeclarativeerror::toString()
{
    {
        QDeclarativeError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);
        error.setColumn(13);

        QCOMPARE(error.toString(), QString("http://www.qt-project.org/main.qml:92:13: An Error"));
    }

    {
        QDeclarativeError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);

        QCOMPARE(error.toString(), QString("http://www.qt-project.org/main.qml:92: An Error"));
    }
}

void tst_qdeclarativeerror::copy()
{
    QDeclarativeError error;
    error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
    error.setDescription("An Error");
    error.setLine(92);
    error.setColumn(13);

    QDeclarativeError error2(error);
    QDeclarativeError error3;
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

void tst_qdeclarativeerror::debug()
{
    {
        QDeclarativeError error;
        error.setUrl(QUrl("http://www.qt-project.org/main.qml"));
        error.setDescription("An Error");
        error.setLine(92);
        error.setColumn(13);

        QTest::ignoreMessage(QtWarningMsg, "http://www.qt-project.org/main.qml:92:13: An Error");
        qWarning() << error;
    }

    {
        QUrl url(QUrl::fromLocalFile(QString(SRCDIR) + "/").resolved(QUrl("test.txt")));
        QDeclarativeError error;
        error.setUrl(url);
        error.setDescription("An Error");
        error.setLine(2);
        error.setColumn(5);

        QString out = url.toString() + ":2:5: An Error \n     Line2 Content \n         ^";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(out));

        qWarning() << error;
    }

    {
        QUrl url(QUrl::fromLocalFile(QString(SRCDIR) + "/").resolved(QUrl("foo.txt")));
        QDeclarativeError error;
        error.setUrl(url);
        error.setDescription("An Error");
        error.setLine(2);
        error.setColumn(5);

        QString out = url.toString() + ":2:5: An Error";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(out));

        qWarning() << error;
    }
}



QTEST_MAIN(tst_qdeclarativeerror)

#include "tst_qdeclarativeerror.moc"
