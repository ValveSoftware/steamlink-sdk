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

#include <QtTest/QtTest>

#include "XMLWriter.h"

#include "XMLWriterTest.h"
#include "XMLWriterTest.moc"

using namespace QPatternistSDK;

QTEST_MAIN(XMLWriterTest)

void XMLWriterTest::serialize()
{
    QFETCH(QString, input);
    QFETCH(QString, expectedResult);

    QByteArray result;
    QBuffer returnBuffer(&result);

    XMLWriter writer(&returnBuffer);

    QXmlInputSource inputSource;
    inputSource.setData(input);

    QXmlSimpleReader reader;
    reader.setContentHandler(&writer);

    const bool parseSuccess = reader.parse(inputSource);
    Q_ASSERT_X(parseSuccess, Q_FUNC_INFO,
               "XMLWriter reported an error while serializing the input.");

    QCOMPARE(QString::fromLatin1(result), expectedResult);
}

void XMLWriterTest::serialize_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedResult");

    /* ------------------- Elements ------------------- */
    QTest::newRow("Only an document element")
        << "<doc></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc/>";

    QTest::newRow("Document element containing a short closed element")
        << "<doc><f/></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><f/></doc>";
    QTest::newRow("Complex nested elements")
        << "<doc><a/><b/><c><d/><e><f><x/></f></e></c></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><a/><b/><c><d/><e><f><x/></f></e></c></doc>";
    /* ------------------------------------------------- */

    /* ---------------- Element Content ---------------- */
    QTest::newRow("Element content with simple content")
        << "<doc>content</doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc>content</doc>";

    QTest::newRow("Element content with tricky to escape content")
        << "<doc>>>&amp;'\"''/></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc>>>&amp;'\"''/></doc>";
    /* ------------------------------------------------- */

    /* ----------- Processing Instructions ------------- */
    QTest::newRow("Simple processing instruction.")
        << "<doc><?php content?></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><?php content?></doc>";
    /* ------------------------------------------------- */

    /* --------------- 'xml' attributes ---------------- */
    QTest::newRow("Simple xml:space attribute.")
        << "<doc xml:space='preserve'>content</doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc xml:space=\"preserve\">content</doc>";

    QTest::newRow("Many 'xml' attributes.")
        << "<doc xml:space='preserve' xml:foo='3' xml:s2='3'>content</doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<doc xml:space=\"preserve\" xml:foo=\"3\" xml:s2=\"3\">content</doc>";
    /* ------------------------------------------------- */

    /* ------------ namespace declarations ------------- */
    QTest::newRow("One simple namespace declaration.")
        << "<doc xmlns:f='example.org/'/>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<doc xmlns:f=\"example.org/\"/>";

    QTest::newRow("Two simple namespace declarations.")
        << "<doc xmlns:f='example.org/' xmlns:e='example.org/'/>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<doc xmlns:f=\"example.org/\" xmlns:e=\"example.org/\"/>";

    QTest::newRow("A simple default namespace.")
        << "<doc xmlns='example.org/'/>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<doc xmlns=\"example.org/\"/>";
    /* ------------------------------------------------- */

    /* -------- namespace declarations in use ---------- */
    QTest::newRow("Simple use of a namespace declaration.")
        << "<doc xmlns:f='example.org/' f:attr='chars'><n/><f:name f:attr='chars'/></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<doc xmlns:f=\"example.org/\" f:attr=\"chars\"><n/><f:name f:attr=\"chars\"/></doc>";
    /* ------------------------------------------------- */
}

void XMLWriterTest::cdata()
{
    /*
    QTest::newRow("Simple CDATA")
        << "<doc><![CDATA[content]]></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><![CDATA[content]]></doc>";

    QTest::newRow("Complex CDATA")
        << "<doc><![CDATA[<<>>&'\";&987;]]></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><![CDATA[<<>>&'\";&123;]]></doc>";
        */
}

void XMLWriterTest::comments()
{
    /*
    QTest::newRow("Simple comment")
        << "<doc><!-- comment --></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><!-- comment --></doc>";
    QTest::newRow("Comment")
        << "<doc><!--- comment --></doc>"
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<doc><!--- comment --></doc>";
        */
}

void XMLWriterTest::doxygenExample()
{
#include "../docs/XMLWriterExample.cpp"

    /* When changing, remember to update the Doxygen in XMLWriter.h */
    const QByteArray expectedResult(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
        "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body><p>Hello World!</p></body></html>"
        );

    QCOMPARE(QString::fromLatin1(result), QString::fromLatin1(expectedResult));
}

// vim: et:ts=4:sw=4:sts=4
