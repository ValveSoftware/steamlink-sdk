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


//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qapplicationargumentparser_p.h"
#include "qapplicationargument_p.h"
#include <QtTest/QtTest>

Q_DECLARE_METATYPE(QList<QApplicationArgument>);
Q_DECLARE_METATYPE(QApplicationArgumentParser::ExitCode);

/*!
  \class tst_QApplicationArgumentParser
  \brief The class tst_QApplicationArgumentParser tests class QApplicationArgumentParser.
  \internal
  \since 4.5

*/
class tst_QApplicationArgumentParser : public QObject
{
    Q_OBJECT
private slots:
    void negativeTest() const;
    void negativeTest_data() const;
    void mandatoryArguments() const;
    void mandatoryArguments_data() const;
};

/*
Comments from notes.txt:

Different arg types:
* -name <mandatory value>
* -name <no value, it's a switch>

Both of these types in addition have a cardinality. For instance:
-name -name -name
-name value1 -name value2 -name value3

Possible Tests
-------------------
    ./foo -ab -cd -
    ./foo -ab -cd - - -
    ./foo -ab -cd - input1 input
    ./foo -help -
    ./foo - -help


    // -switch has upper limit of 2
    ./foo -switch -switch -switch

    // -switch has upper limit of 1
    ./foo -switch -switch

    // -switch has lower limit of 1
    ./foo

    ./foo -switch cruft -switch
    ./foo -option value1 cruft -option value2
    ./foo -option value1 cruft cruft -option value2
    ./foo -option value1 cruft cruft cruft -option value2
    ./foo -option -option -option2 -option2
    ./foo -option
    ./foo -option -option -option2
    ./foo -option -
    ./foo -option - -
*/

void tst_QApplicationArgumentParser::negativeTest() const
{
}

void tst_QApplicationArgumentParser::negativeTest_data() const
{
    QTest::addColumn<QStringList>("inputArgs");
    QTest::addColumn<QApplicationArgumentParser::ExitCode>("expectedExitCode");
    QTest::addColumn<QString>("expectedStderr");
    QTest::addColumn<QList<QApplicationArgument> >("declarations");
}

void tst_QApplicationArgumentParser::mandatoryArguments() const
{
   QFETCH(QStringList, inputArgs);
   QFETCH(QList<QApplicationArgument>, inputDecls);

   QApplicationArgumentParser parser(inputArgs);
   parser.setDeclaredArguments(inputDecls);

   QVERIFY(!parser.parse());
   QCOMPARE(parser.exitCode(), QApplicationArgumentParser::ParseError);
}

void tst_QApplicationArgumentParser::mandatoryArguments_data() const
{
    QTest::addColumn<QStringList>("inputArgs");
    QTest::addColumn<QList<QApplicationArgument> >("inputDecls");

    {
        QStringList in;
        in << "./appName";

        QList<QApplicationArgument> decls;
        QApplicationArgument arg1("name", QString(), QVariant::String);
        arg1.setMinimumOccurrence(1);
        decls.append(arg1);

        QTest::newRow("A single, named, argument") << in << decls;
    }
}

QTEST_APPLESS_MAIN(tst_QApplicationArgumentParser)
#include "tst_qapplicationargumentparser.moc"
