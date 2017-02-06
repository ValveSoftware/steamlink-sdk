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

#include "../../shared/util.h"

#include <qtest.h>
#include <QObject>
#include <QQmlEngine>
#include <QQmlComponent>
#include <private/qqmldirparser_p.h>
#include <QDebug>

#include <algorithm>

// Test the parsing of qmldir files

class tst_qqmldirparser : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmldirparser();

private slots:
    void parse_data();
    void parse();
};

tst_qqmldirparser::tst_qqmldirparser()
{
}

namespace {

    QStringList toStringList(const QList<QQmlError> &errors)
    {
        QStringList rv;

        foreach (const QQmlError &e, errors)
            rv.append(e.toString());

        return rv;
    }

    QString toString(const QQmlDirParser::Plugin &p)
    {
        return p.name + QLatin1Char('|') + p.path;
    }

    QStringList toStringList(const QList<QQmlDirParser::Plugin> &plugins)
    {
        QStringList rv;

        foreach (const QQmlDirParser::Plugin &p, plugins)
            rv.append(toString(p));

        return rv;
    }

    QString toString(const QQmlDirParser::Component &c)
    {
        return c.typeName + QLatin1Char('|') + c.fileName + QLatin1Char('|')
            + QString::number(c.majorVersion) + QLatin1Char('|') + QString::number(c.minorVersion)
            + QLatin1Char('|') + (c.internal ? "true" : "false");
    }

    QStringList toStringList(const QQmlDirComponents &components)
    {
        QStringList rv;

        foreach (const QQmlDirParser::Component &c, components.values())
            rv.append(toString(c));

        std::sort(rv.begin(), rv.end());
        return rv;
    }

    QString toString(const QQmlDirParser::Script &s)
    {
        return s.nameSpace + QLatin1Char('|') + s.fileName + QLatin1Char('|')
            + QString::number(s.majorVersion) + '|' + QString::number(s.minorVersion);
    }

    QStringList toStringList(const QList<QQmlDirParser::Script> &scripts)
    {
        QStringList rv;

        foreach (const QQmlDirParser::Script &s, scripts)
            rv.append(toString(s));

        return rv;
    }
}

void tst_qqmldirparser::parse_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QStringList>("errors");
    QTest::addColumn<QStringList>("plugins");
    QTest::addColumn<QStringList>("components");
    QTest::addColumn<QStringList>("scripts");
    QTest::addColumn<QStringList>("dependencies");
    QTest::addColumn<bool>("designerSupported");

    QTest::newRow("empty")
        << "empty/qmldir"
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("no-content")
        << "no-content/qmldir"
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("one-section")
        << "one-section/qmldir"
        << (QStringList() << "qmldir:1: a component declaration requires two or three arguments, but 1 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("four-sections")
        << "four-sections/qmldir"
        << (QStringList() << "qmldir:1: a component declaration requires two or three arguments, but 4 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("incomplete-module")
        << "incomplete-module/qmldir"
        << (QStringList() << "qmldir:1: module identifier directive requires one argument, but 0 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("excessive-module")
        << "excessive-module/qmldir"
        << (QStringList() << "qmldir:1: module identifier directive requires one argument, but 2 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("repeated-module")
        << "repeated-module/qmldir"
        << (QStringList() << "qmldir:2: only one module identifier directive may be defined in a qmldir file")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("non-first-module")
        << "non-first-module/qmldir"
        << (QStringList() << "qmldir:2: module identifier directive must be the first directive in a qmldir file")
        << (QStringList() << "foo|")
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("incomplete-plugin")
        << "incomplete-plugin/qmldir"
        << (QStringList() << "qmldir:1: plugin directive requires one or two arguments, but 0 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("excessive-plugin")
        << "excessive-plugin/qmldir"
        << (QStringList() << "qmldir:1: plugin directive requires one or two arguments, but 3 were provided")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("name-plugin")
        << "name-plugin/qmldir"
        << QStringList()
        << (QStringList() << "foo|")
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("name-path-plugin")
        << "name-path-plugin/qmldir"
        << QStringList()
        << (QStringList() << "foo|bar")
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("unversioned-component")
        << "unversioned-component/qmldir"
        << QStringList()
        << QStringList()
        << (QStringList() << "foo|bar|-1|-1|false")
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("invalid-versioned-component")
        << "invalid-versioned-component/qmldir"
        << (QStringList() << "qmldir:1: invalid version 100, expected <major>.<minor>")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("versioned-component")
        << "versioned-component/qmldir"
        << QStringList()
        << QStringList()
        << (QStringList() << "foo|bar|33|66|false")
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("invalid-versioned-script")
        << "invalid-versioned-script/qmldir"
        << (QStringList() << "qmldir:1: invalid version 100, expected <major>.<minor>")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("versioned-script")
        << "versioned-script/qmldir"
        << QStringList()
        << QStringList()
        << QStringList()
        << (QStringList() << "foo|bar.js|33|66")
        << QStringList()
        << false;

    QTest::newRow("multiple")
        << "multiple/qmldir"
        << QStringList()
        << (QStringList() << "PluginA|plugina.so")
        << (QStringList() << "ComponentA|componenta-1_0.qml|1|0|false"
                          << "ComponentA|componenta-1_5.qml|1|5|false"
                          << "ComponentB|componentb-1_5.qml|1|5|false")
        << (QStringList() << "ScriptA|scripta-1_0.js|1|0")
        << QStringList()
        << false;

    QTest::newRow("designersupported-yes")
        << "designersupported-yes/qmldir"
        << QStringList()
        << (QStringList() << "foo|")
        << QStringList()
        << QStringList()
        << QStringList()
        << true;

    QTest::newRow("designersupported-no")
        << "designersupported-no/qmldir"
        << QStringList()
        << (QStringList() << "foo|")
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("invalid-versioned-dependency")
        << "invalid-versioned-dependency/qmldir"
        << (QStringList() << "qmldir:1: invalid version 100, expected <major>.<minor>")
        << QStringList()
        << QStringList()
        << QStringList()
        << QStringList()
        << false;

    QTest::newRow("dependency")
            << "dependency/qmldir"
            << QStringList()
            << (QStringList() << "foo|")
            << QStringList()
            << QStringList()
            << (QStringList() << "bar||1|0|true")
            << false;
}

void tst_qqmldirparser::parse()
{
    QFETCH(QString, file);
    QFETCH(QStringList, errors);
    QFETCH(QStringList, plugins);
    QFETCH(QStringList, components);
    QFETCH(QStringList, scripts);
    QFETCH(QStringList, dependencies);
    QFETCH(bool, designerSupported);

    QFile f(testFile(file));
    f.open(QIODevice::ReadOnly);

    QQmlDirParser p;
    p.parse(f.readAll());

    if (errors.isEmpty()) {
        QCOMPARE(p.hasError(), false);
    } else {
        QCOMPARE(p.hasError(), true);
        QCOMPARE(toStringList(p.errors("qmldir")), errors);
    }

    QCOMPARE(toStringList(p.plugins()), plugins);
    QCOMPARE(toStringList(p.components()), components);
    QCOMPARE(toStringList(p.scripts()), scripts);
    QCOMPARE(toStringList(p.dependencies()), dependencies);

    QCOMPARE(p.designerSupported(), designerSupported);
}

QTEST_MAIN(tst_qqmldirparser)

#include "tst_qqmldirparser.moc"
