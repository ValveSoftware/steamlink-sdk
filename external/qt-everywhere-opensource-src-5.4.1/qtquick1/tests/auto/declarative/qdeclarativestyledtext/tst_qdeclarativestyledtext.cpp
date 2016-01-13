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
#include <QtTest/QtTest>
#include <QtGui/QTextLayout>
#include <private/qdeclarativestyledtext_p.h>

class tst_qdeclarativestyledtext : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativestyledtext()
    {
    }

private slots:
    void textOutput();
    void textOutput_data();
};

// For malformed input all we test is that we get the expected text out.
//
void tst_qdeclarativestyledtext::textOutput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("output");

    QTest::newRow("bold") << "<b>bold</b>" << "bold";
    QTest::newRow("italic") << "<b>italic</b>" << "italic";
    QTest::newRow("missing >") << "<b>text</b" << "text";
    QTest::newRow("missing b>") << "<b>text</" << "text";
    QTest::newRow("missing /b>") << "<b>text<" << "text";
    QTest::newRow("missing </b>") << "<b>text" << "text";
    QTest::newRow("bad nest") << "<b>text <i>italic</b></i>" << "text italic";
    QTest::newRow("font color") << "<font color=\"red\">red text</font>" << "red text";
    QTest::newRow("font color: single quote") << "<font color='red'>red text</font>" << "red text";
    QTest::newRow("font size") << "<font size=\"1\">text</font>" << "text";
    QTest::newRow("font empty") << "<font>text</font>" << "text";
    QTest::newRow("font bad 1") << "<font ezis=\"blah\">text</font>" << "text";
    QTest::newRow("font bad 2") << "<font size=\"1>text</font>" << "";
    QTest::newRow("extra close") << "<b>text</b></b>" << "text";
    QTest::newRow("extra space") << "<b >text</b>" << "text";
    QTest::newRow("entities") << "&lt;b&gt;this &amp; that&lt;/b&gt;" << "<b>this & that</b>";
    QTest::newRow("newline") << "text<br>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text")  ;
    QTest::newRow("self-closing newline") << "text<br/>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text")  ;
    QTest::newRow("empty") << "" << "";
}

void tst_qdeclarativestyledtext::textOutput()
{
    QFETCH(QString, input);
    QFETCH(QString, output);

    QTextLayout layout;
    QDeclarativeStyledText::parse(input, layout);

    QCOMPARE(layout.text(), output);
}


QTEST_MAIN(tst_qdeclarativestyledtext)

#include "tst_qdeclarativestyledtext.moc"
