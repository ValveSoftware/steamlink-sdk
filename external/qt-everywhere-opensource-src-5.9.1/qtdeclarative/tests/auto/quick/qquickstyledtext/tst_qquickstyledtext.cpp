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
#include <QtTest/QtTest>
#include <QtGui/QTextLayout>
#include <QtCore/QList>
#include <QtQuick/private/qquickstyledtext_p.h>

class tst_qquickstyledtext : public QObject
{
    Q_OBJECT
public:
    tst_qquickstyledtext()
    {
    }

    struct Format {
        enum Type {
            Bold = 0x01,
            Underline = 0x02,
            Italic = 0x04,
            Anchor = 0x08
        };
        Format(int t, int s, int l)
            : type(t), start(s), length(l) {}
        int type;
        int start;
        int length;
    };
    typedef QList<Format> FormatList;

    static const QChar bullet;
    static const QChar disc;
    static const QChar square;

private slots:
    void textOutput();
    void textOutput_data();
    void anchors();
    void anchors_data();
    void longString();
};

Q_DECLARE_METATYPE(tst_qquickstyledtext::FormatList);

const QChar tst_qquickstyledtext::bullet(0x2022);
const QChar tst_qquickstyledtext::disc(0x25e6);
const QChar tst_qquickstyledtext::square(0x25a1);

// For malformed input all we test is that we get the expected text and format out.
//
void tst_qquickstyledtext::textOutput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("output");
    QTest::addColumn<FormatList>("formats");
    QTest::addColumn<bool>("modifiesFontSize");

    QTest::newRow("empty") << "" << "" << FormatList() << false;
    QTest::newRow("empty tag") << "<>test</>" << "test" << FormatList() << false;
    QTest::newRow("nest opening") << "<b<b>>test</b>" << ">test" << FormatList() << false;
    QTest::newRow("nest closing") << "<b>test<</b>/b>" << "test/b>" << (FormatList() << Format(Format::Bold, 0, 7)) << false;
    QTest::newRow("bold") << "<b>bold</b>" << "bold" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("bold 2") << "<b>>>>>bold</b>" << ">>>>bold" << (FormatList() << Format(Format::Bold, 0, 8)) << false;
    QTest::newRow("bold 3") << "<b>bold<>/b>" << "bold/b>" << (FormatList() << Format(Format::Bold, 0, 7)) << false;
    QTest::newRow("italic") << "<i>italic</i>" << "italic" << (FormatList() << Format(Format::Italic, 0, 6)) << false;
    QTest::newRow("underline") << "<u>underline</u>" << "underline" << (FormatList() << Format(Format::Underline, 0, 9)) << false;
    QTest::newRow("strong") << "<strong>strong</strong>" << "strong" << (FormatList() << Format(Format::Bold, 0, 6)) << false;
    QTest::newRow("underline") << "<u>underline</u>" << "underline" << (FormatList() << Format(Format::Underline, 0, 9)) << false;
    QTest::newRow("missing >") << "<b>text</b" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("missing b>") << "<b>text</" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("missing /b>") << "<b>text<" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("missing </b>") << "<b>text" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("nested") << "<b>text <i>italic</i> bold</b>" << "text italic bold" << (FormatList() << Format(Format::Bold, 0, 5) << Format(Format::Bold | Format::Italic, 5, 6) << Format(Format::Bold, 11, 5)) << false;
    QTest::newRow("bad nest") << "<b>text <i>italic</b></i>" << "text italic" << (FormatList() << Format(Format::Bold, 0, 5) << Format(Format::Bold | Format::Italic, 5, 6)) << false;
    QTest::newRow("font color") << "<font color=\"red\">red text</font>" << "red text" << (FormatList() << Format(0, 0, 8)) << false;
    QTest::newRow("font color: single quote") << "<font color='red'>red text</font>" << "red text" << (FormatList() << Format(0, 0, 8)) << false;
    QTest::newRow("font size") << "<font size=\"1\">text</font>" << "text" << (FormatList() << Format(0, 0, 4)) << true;
    QTest::newRow("font empty") << "<font>text</font>" << "text" << FormatList() << false;
    QTest::newRow("font bad 1") << "<font ezis=\"blah\">text</font>" << "text" << FormatList() << false;
    QTest::newRow("font bad 2") << "<font size=\"1>text</font>" << "" << FormatList() << false;
    QTest::newRow("extra close") << "<b>text</b></b>" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("extra space") << "<b >text</b>" << "text" << (FormatList() << Format(Format::Bold, 0, 4)) << false;
    QTest::newRow("entities") << "&lt;b&gt;&quot;this&quot; &amp; that&lt;/b&gt;" << "<b>\"this\" & that</b>" << FormatList() << false;
    QTest::newRow("newline") << "text<br>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("self-closing newline") << "text<br/>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("paragraph") << "text<p>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("paragraph closed") << "text<p>more text</p>more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text")  + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("paragraph closed bold") << "<b>text<p>more text</p>more text</b>" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text")  + QChar(QChar::LineSeparator) + QLatin1String("more text") << (FormatList() << Format(Format::Bold, 0, 24)) << false;
    QTest::newRow("unknown tag") << "<a href='#'><foo>underline</foo></a> not" << "underline not" << (FormatList() << Format(Format::Underline, 0, 9)) << false;
    QTest::newRow("ordered list") << "<ol><li>one<li>two" << QString(6, QChar::Nbsp) + QLatin1String("1.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("2.") + QString(2, QChar::Nbsp) + QLatin1String("two") << FormatList() << false;
    QTest::newRow("ordered list closed") << "<ol><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("1.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("2.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("ordered list alpha") << "<ol type=\"a\"><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("a.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("b.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("ordered list upper alpha") << "<ol type=\"A\"><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("A.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("B.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("ordered list roman") << "<ol type=\"i\"><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("i.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("ii.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("ordered list upper roman") << "<ol type=\"I\"><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("I.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("II.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("ordered list bad") << "<ol type=\"z\"><li>one</li><li>two</li></ol>" << QString(6, QChar::Nbsp) + QLatin1String("1.") + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + QLatin1String("2.") + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("unordered list") << "<ul><li>one<li>two" << QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("two") << FormatList() << false;
    QTest::newRow("unordered list closed") << "<ul><li>one</li><li>two</li></ul>" << QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("unordered list disc") << "<ul type=\"disc\"><li>one</li><li>two</li></ul>" << QString(6, QChar::Nbsp) + disc + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + disc + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("unordered list square") << "<ul type=\"square\"><li>one</li><li>two</li></ul>" << QString(6, QChar::Nbsp) + square + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + square + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("unordered list bad") << "<ul type=\"bad\"><li>one</li><li>two</li></ul>" << QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("one") + QChar(QChar::LineSeparator) + QString(6, QChar::Nbsp) + bullet + QString(2, QChar::Nbsp) + QLatin1String("two") + QChar(QChar::LineSeparator) << FormatList() << false;
    QTest::newRow("header close") << "<h1>head</h1>more" << QLatin1String("head") + QChar(QChar::LineSeparator) + QLatin1String("more") << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h0") << "<h0>head" << "head" << FormatList() << false;
    QTest::newRow("h1") << "<h1>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h2") << "<h2>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h3") << "<h3>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h4") << "<h4>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h5") << "<h5>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h6") << "<h6>head" << "head" << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("h7") << "<h7>head" << "head" << FormatList() << false;
    QTest::newRow("pre") << "normal<pre>pre text</pre>normal" << QLatin1String("normal") + QChar(QChar::LineSeparator) + QLatin1String("pre") + QChar(QChar::Nbsp) + QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("normal") << (FormatList() << Format(0, 6, 9)) << false;
    QTest::newRow("pre lb") << "normal<pre>pre\n text</pre>normal" << QLatin1String("normal") + QChar(QChar::LineSeparator) + QLatin1String("pre") + QChar(QChar::LineSeparator) + QChar(QChar::Nbsp) + QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("normal") << (FormatList() << Format(0, 6, 10)) << false;
    QTest::newRow("line feed") << "line\nfeed" << "line feed" << FormatList() << false;
    QTest::newRow("leading whitespace") << " leading whitespace" << "leading whitespace" << FormatList() << false;
    QTest::newRow("trailing whitespace") << "trailing whitespace " << "trailing whitespace" << FormatList() << false;
    QTest::newRow("consecutive whitespace") << " consecutive  \t \n  whitespace" << "consecutive whitespace" << FormatList() << false;
    QTest::newRow("space after newline") << "text<br/> more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("space after paragraph") << "text<p> more text</p> more text" << QLatin1String("text") + QChar(QChar::LineSeparator) + QLatin1String("more text") + QChar(QChar::LineSeparator) + QLatin1String("more text") << FormatList() << false;
    QTest::newRow("space in header") << "<h1> head</h1> " << QLatin1String("head") + QChar(QChar::LineSeparator) << (FormatList() << Format(Format::Bold, 0, 4)) << true;
    QTest::newRow("space before bold") << "this is <b>bold</b>" << "this is bold" << (FormatList() << Format(Format::Bold, 8, 4)) << false;
    QTest::newRow("space leading bold") << "this is<b> bold</b>" << "this is bold" << (FormatList() << Format(Format::Bold, 7, 5)) << false;
    QTest::newRow("space trailing bold") << "this is <b>bold </b>" << "this is bold " << (FormatList() << Format(Format::Bold, 8, 5)) << false;
    QTest::newRow("img") << "a<img src=\"blah.png\"/>b" << "a  b" << FormatList() << false;
    QTest::newRow("tag mix") << "<f6>ds<b></img><pro>gfh</b><w><w>ghj</stron><ql><sl><pl>dfg</j6><img><bol><r><prp>dfg<bkj></b><up><string>ewrq</al><bl>jklhj<zl>" << "dsgfhghjdfgdfgewrqjklhj" << (FormatList() << Format(Format::Bold, 2, 3)) << false;
    QTest::newRow("named html entities") << "&gt; &lt; &amp; &quot; &nbsp;" << QLatin1String("> < & \" ") + QChar(QChar::Nbsp) << FormatList() << false;
    QTest::newRow("invalid html entities") << "a &hello & a &goodbye;" << "a &hello & a " << FormatList() << false;
}

void tst_qquickstyledtext::textOutput()
{
    QFETCH(QString, input);
    QFETCH(QString, output);
    QFETCH(FormatList, formats);
    QFETCH(bool, modifiesFontSize);

    QTextLayout layout;
    QList<QQuickStyledTextImgTag*> imgTags;
    bool fontSizeModified = false;
    QQuickStyledText::parse(input, layout, imgTags, QUrl(), 0, false, &fontSizeModified);

    QCOMPARE(layout.text(), output);

    const QVector<QTextLayout::FormatRange> layoutFormats = layout.formats();

    QCOMPARE(layoutFormats.count(), formats.count());
    for (int i = 0; i < formats.count(); ++i) {
        QCOMPARE(layoutFormats.at(i).start, formats.at(i).start);
        QCOMPARE(layoutFormats.at(i).length, formats.at(i).length);
        if (formats.at(i).type & Format::Bold)
            QCOMPARE(layoutFormats.at(i).format.fontWeight(), int(QFont::Bold));
        else
            QCOMPARE(layoutFormats.at(i).format.fontWeight(), int(QFont::Normal));
        QVERIFY(layoutFormats.at(i).format.fontItalic() == bool(formats.at(i).type & Format::Italic));
        QVERIFY(layoutFormats.at(i).format.fontUnderline() == bool(formats.at(i).type & Format::Underline));
    }
    QCOMPARE(fontSizeModified, modifiesFontSize);
}

void tst_qquickstyledtext::anchors()
{
    QFETCH(QString, input);
    QFETCH(QString, output);
    QFETCH(FormatList, formats);

    QTextLayout layout;
    QList<QQuickStyledTextImgTag*> imgTags;
    bool fontSizeModified = false;
    QQuickStyledText::parse(input, layout, imgTags, QUrl(), 0, false, &fontSizeModified);

    QCOMPARE(layout.text(), output);

    const QVector<QTextLayout::FormatRange> layoutFormats = layout.formats();

    QCOMPARE(layoutFormats.count(), formats.count());
    for (int i = 0; i < formats.count(); ++i) {
        QCOMPARE(layoutFormats.at(i).start, formats.at(i).start);
        QCOMPARE(layoutFormats.at(i).length, formats.at(i).length);
        QVERIFY(layoutFormats.at(i).format.isAnchor() == bool(formats.at(i).type & Format::Anchor));
    }
}

void tst_qquickstyledtext::anchors_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("output");
    QTest::addColumn<FormatList>("formats");

    QTest::newRow("empty 1") << "Test string with <a href=>url</a>." << "Test string with url." << FormatList();
    QTest::newRow("empty 2") << "Test string with <a href="">url</a>." << "Test string with url." << FormatList();
    QTest::newRow("unknown attr") << "Test string with <a hfre=\"http://strange<username>@ok-hostname\">url</a>." << "Test string with url." << FormatList();
    QTest::newRow("close") << "Test string with <a href=\"http://strange<username>@ok-hostname\"/>url." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 4));
    QTest::newRow("username") << "Test string with <a href=\"http://strange<username>@ok-hostname\">url</a>." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 3));
    QTest::newRow("query") << "Test string with <a href=\"http://www.foo.bar?hello=world\">url</a>." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 3));
    QTest::newRow("ipv6") << "Test string with <a href=\"//user:pass@[56::56:56:56:127.0.0.1]:99\">url</a>." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 3));
    QTest::newRow("uni") << "Test string with <a href=\"data:text/javascript,d5%20%3D%20'five\\u0027s'%3B\">url</a>." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 3));
    QTest::newRow("utf8") << "Test string with <a href=\"http://www.räksmörgås.se/pub?a=b&a=dø&a=f#vræl\">url</a>." << "Test string with url." << (FormatList() << Format(Format::Anchor, 17, 3));
}

void tst_qquickstyledtext::longString()
{
    QTextLayout layout;
    QList<QQuickStyledTextImgTag*> imgTags;
    bool fontSizeModified = false;

    QString input(9999999, QChar('.'));
    QQuickStyledText::parse(input, layout, imgTags, QUrl(), 0, false, &fontSizeModified);
    QCOMPARE(layout.text(), input);

    input = QString(9999999, QChar('\t')); // whitespace
    QQuickStyledText::parse(input, layout, imgTags, QUrl(), 0, false, &fontSizeModified);
    QCOMPARE(layout.text(), QString(""));
}

QTEST_MAIN(tst_qquickstyledtext)

#include "tst_qquickstyledtext.moc"
