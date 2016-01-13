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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceReply>

QT_USE_NAMESPACE

class TestReply : public QPlaceReply
{
public:
    TestReply(QObject *parent) : QPlaceReply(parent) {}
    void setFinished(bool finished) { QPlaceReply::setFinished(finished); }
    void setError(QPlaceReply::Error error, const QString &errorString) {
        QPlaceReply::setError(error,errorString);
    }
};

class tst_QPlaceReply : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceReply();

private Q_SLOTS:
    void constructorTest();
    void typeTest();
    void finishedTest();
    void errorTest();
};

tst_QPlaceReply::tst_QPlaceReply()
{

}

void tst_QPlaceReply::constructorTest()
{
    TestReply *reply = new TestReply(this);
    QCOMPARE(reply->parent(), this);
    delete reply;
}

void tst_QPlaceReply::typeTest()
{
    TestReply *reply = new TestReply(this);
    QCOMPARE(reply->type(), QPlaceReply::Reply);

    delete reply;
}

void tst_QPlaceReply::finishedTest()
{
    TestReply *reply = new TestReply(this);
    QCOMPARE(reply->isFinished(), false);
    reply->setFinished(true);
    QCOMPARE(reply->isFinished(), true);

    delete reply;
}

void tst_QPlaceReply::errorTest()
{
    TestReply *reply = new TestReply(this);
    QCOMPARE(reply->error(), QPlaceReply::NoError);
    QCOMPARE(reply->errorString(), QString());

    reply->setError(QPlaceReply::CommunicationError, QStringLiteral("Could not connect to server"));
    QCOMPARE(reply->error(), QPlaceReply::CommunicationError);
    QCOMPARE(reply->errorString(), QStringLiteral("Could not connect to server"));

    reply->setError(QPlaceReply::NoError, QString());
    QCOMPARE(reply->error(), QPlaceReply::NoError);
    QCOMPARE(reply->errorString(), QString());

    delete reply;
}

QTEST_APPLESS_MAIN(tst_QPlaceReply)

#include "tst_qplacereply.moc"
