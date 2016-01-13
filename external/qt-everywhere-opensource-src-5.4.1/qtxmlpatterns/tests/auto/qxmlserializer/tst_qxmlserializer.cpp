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


#include <QtTest/QtTest>

#include <QtCore/QTextCodec>
#include <QtXmlPatterns/QXmlSerializer>
#include <QtXmlPatterns/QXmlQuery>

#include "../qxmlquery/MessageSilencer.h"

/*!
 \class tst_QXmlSerializer
 \internal
 \since 4.4
 \brief Tests QSourceLocation

 */
class tst_QXmlSerializer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constructorTriggerWarnings() const;
    void objectSize() const;
    void constCorrectness() const;
    void setCodec() const;
    void codec() const;
    void outputDevice() const;
    void serializationError() const;
    void serializationError_data() const;
    void cleanUpTestCase() const;
};

void tst_QXmlSerializer::constructorTriggerWarnings() const
{
    QXmlQuery query;

    QTest::ignoreMessage(QtWarningMsg, "outputDevice cannot be null.");
    QXmlSerializer(query, 0);

    QTest::ignoreMessage(QtWarningMsg, "outputDevice must be opened in write mode.");
    QBuffer buffer;
    QXmlSerializer(query, &buffer);
}

void tst_QXmlSerializer::constCorrectness() const
{
    QXmlQuery query;
    QFile file(QLatin1String("dummy.xml"));
    file.open(QIODevice::WriteOnly);
    const QXmlSerializer serializer(query, &file);
    /* These functions must be const. */

    serializer.outputDevice();
    serializer.codec();
}

void tst_QXmlSerializer::objectSize() const
{
    QCOMPARE(sizeof(QXmlSerializer), sizeof(QAbstractXmlReceiver));
}

void tst_QXmlSerializer::setCodec() const
{
    QFile file(QLatin1String("dummy.xml"));
    file.open(QIODevice::WriteOnly);

    /* Ensure we can take a const pointer. */
    {
        QXmlQuery query;
        QXmlSerializer serializer(query, &file);
        serializer.setCodec(const_cast<const QTextCodec *>(QTextCodec::codecForName("UTF-8")));
    }

    /* Check that setting the codec has effect. */
    {
        QXmlQuery query;
        QXmlSerializer serializer(query, &file);
        serializer.setCodec(const_cast<const QTextCodec *>(QTextCodec::codecForName("UTF-16")));
        QCOMPARE(serializer.codec()->name(), QTextCodec::codecForName("UTF-16")->name());

        /* Set it back. */
        serializer.setCodec(const_cast<const QTextCodec *>(QTextCodec::codecForName("UTF-8")));
        QCOMPARE(serializer.codec()->name(), QTextCodec::codecForName("UTF-8")->name());
    }
}

void tst_QXmlSerializer::codec() const
{
    QFile file(QLatin1String("dummy.xml"));
    file.open(QIODevice::WriteOnly);

    /* Check default value. */
    {
        const QXmlQuery query;
        const QXmlSerializer serializer(query, &file);
        QCOMPARE(serializer.codec()->name(), QTextCodec::codecForName("UTF-8")->name());
    }
}

void tst_QXmlSerializer::outputDevice() const
{
    QFile file(QLatin1String("dummy.xml"));
    file.open(QIODevice::WriteOnly);

    /* Check default value. */
    {
        const QXmlQuery query;
        const QXmlSerializer serializer(query, &file);
        QCOMPARE(serializer.outputDevice(), static_cast< QIODevice *>(&file));
    }
}

void tst_QXmlSerializer::serializationError() const
{
    QFETCH(QString, queryString);
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(queryString);

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(query.isValid());

    QXmlSerializer serializer(query, &buffer);

    QEXPECT_FAIL("Two top elements", "Bug, this is not checked for", Continue);
    QVERIFY(!query.evaluateTo(&serializer));
}

void tst_QXmlSerializer::serializationError_data() const
{
    QTest::addColumn<QString>("queryString");

    QTest::newRow("Two top elements")
        << QString::fromLatin1("<e/>, <e/>");

    QTest::newRow("An attribute")
        << QString::fromLatin1("attribute name {'foo'}");
}

void tst_QXmlSerializer::cleanUpTestCase() const
{
    QVERIFY(QFile::remove(QLatin1String("dummy.xml")));
}

QTEST_MAIN(tst_QXmlSerializer)

#include "tst_qxmlserializer.moc"

// vim: et:ts=4:sw=4:sts=4
