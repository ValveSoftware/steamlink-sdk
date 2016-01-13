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


#include <QFile>
#include <QtTest/QtTest>

/* We expect these headers to be available. */
#include <QtXmlPatterns/QAbstractMessageHandler>
#include <QtXmlPatterns/qabstractmessagehandler.h>
#include <QAbstractMessageHandler>
#include <qabstractmessagehandler.h>

/*!
 \class tst_QAbstractMessageHandler
 \internal
 \since 4.4
 \brief Tests the QAbstractMessageHandler class.
 */
class tst_QAbstractMessageHandler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constructor() const;
    void constCorrectness() const;
    void message() const;
    void messageDefaultArguments() const;
    void objectSize() const;
    void hasQ_OBJECTMacro() const;
};

class Received
{
public:
    QtMsgType       type;
    QString         description;
    QUrl            identifier;
    QSourceLocation sourceLocation;

    bool operator==(const Received &other) const
    {
        return type == other.type
               && description == other.description
               && identifier == other.identifier
               && sourceLocation == other.sourceLocation;
    }
};

class TestMessageHandler : public QAbstractMessageHandler
{
public:
    QList<Received> received;

protected:
    virtual void handleMessage(QtMsgType type, const QString &description, const QUrl &identifier, const QSourceLocation &sourceLocation);
};

void TestMessageHandler::handleMessage(QtMsgType type, const QString &description, const QUrl &identifier, const QSourceLocation &sourceLocation)
{
    Received r;
    r.type = type;
    r.description = description;
    r.identifier = identifier;
    r.sourceLocation = sourceLocation;
    received.append(r);
}

void tst_QAbstractMessageHandler::constructor() const
{
    /* Allocate instance. */
    {
        TestMessageHandler instance;
    }

    {
        TestMessageHandler instance1;
        TestMessageHandler instance2;
    }

    {
        TestMessageHandler instance1;
        TestMessageHandler instance2;
        TestMessageHandler instance3;
    }
}

void tst_QAbstractMessageHandler::constCorrectness() const
{
    /* No members are supposed to be const. */
}

void tst_QAbstractMessageHandler::objectSize() const
{
    /* We shouldn't be adding anything. */
    QCOMPARE(sizeof(QAbstractMessageHandler), sizeof(QObject));
}

void tst_QAbstractMessageHandler::message() const
{
    TestMessageHandler handler;

    /* Check that the arguments comes out as expected. */
    handler.message(QtDebugMsg,
                    QLatin1String("A description"),
                    QUrl(QLatin1String("http://example.com/ID")),
                    QSourceLocation(QUrl(QLatin1String("http://example.com/Location")), 4, 5));

    Received expected;
    expected.type = QtDebugMsg;
    expected.description = QLatin1String("A description");
    expected.identifier = QUrl(QLatin1String("http://example.com/ID"));
    expected.sourceLocation = QSourceLocation(QUrl(QLatin1String("http://example.com/Location")), 4, 5);

    QCOMPARE(expected, handler.received.first());
}

void tst_QAbstractMessageHandler::messageDefaultArguments() const
{
    TestMessageHandler handler;

    /* The three last arguments in message() are optional. Check that they are what we promise. */
    handler.message(QtDebugMsg, QLatin1String("A description"));

    Received expected;
    expected.type = QtDebugMsg;
    expected.description = QLatin1String("A description");
    expected.identifier = QUrl();
    expected.sourceLocation = QSourceLocation();

    QCOMPARE(expected, handler.received.first());
}

void tst_QAbstractMessageHandler::hasQ_OBJECTMacro() const
{
    TestMessageHandler messageHandler;
    /* If this code fails to compile, the Q_OBJECT macro is missing in
     * the class declaration. */
    QAbstractMessageHandler *const secondPointer = qobject_cast<QAbstractMessageHandler *>(&messageHandler);
    /* The static_cast is for compiling on broken compilers. */
    QCOMPARE(static_cast<QAbstractMessageHandler *>(&messageHandler), secondPointer);
}

QTEST_MAIN(tst_QAbstractMessageHandler)

#include "tst_qabstractmessagehandler.moc"

// vim: et:ts=4:sw=4:sts=4
