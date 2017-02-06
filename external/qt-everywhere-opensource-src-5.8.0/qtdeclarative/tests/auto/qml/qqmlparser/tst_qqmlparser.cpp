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

#include <private/qqmljsengine_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsastvisitor_p.h>
#include <private/qqmljsast_p.h>

#include <qtest.h>
#include <QDir>
#include <QDebug>
#include <cstdlib>

class tst_qqmlparser : public QObject
{
    Q_OBJECT
public:
    tst_qqmlparser();

private slots:
    void initTestCase();
#if !defined(QTEST_CROSS_COMPILED) // sources not available when cross compiled
    void qmlParser_data();
    void qmlParser();
#endif

private:
    QStringList excludedDirs;

    QStringList findFiles(const QDir &);
};

namespace check {

using namespace QQmlJS;

class Check: public AST::Visitor
{
    QList<AST::Node *> nodeStack;

public:
    void operator()(AST::Node *node)
    {
        AST::Node::accept(node, this);
    }

    void checkNode(AST::Node *node)
    {
        if (! nodeStack.isEmpty()) {
            AST::Node *parent = nodeStack.last();
            const quint32 parentBegin = parent->firstSourceLocation().begin();
            const quint32 parentEnd = parent->lastSourceLocation().end();

            QVERIFY(node->firstSourceLocation().begin() >= parentBegin);
            QVERIFY(node->lastSourceLocation().end() <= parentEnd);
        }
    }

    virtual bool preVisit(AST::Node *node)
    {
        checkNode(node);
        nodeStack.append(node);
        return true;
    }

    virtual void postVisit(AST::Node *)
    {
        nodeStack.removeLast();
    }
};

}

tst_qqmlparser::tst_qqmlparser()
{
}

void tst_qqmlparser::initTestCase()
{
    // Add directories you want excluded here

    // These snippets are not expected to run on their own.
    excludedDirs << "doc/src/snippets/qml/visualdatamodel_rootindex";
    excludedDirs << "doc/src/snippets/qml/qtbinding";
    excludedDirs << "doc/src/snippets/qml/imports";
    excludedDirs << "doc/src/snippets/qtquick1/visualdatamodel_rootindex";
    excludedDirs << "doc/src/snippets/qtquick1/qtbinding";
    excludedDirs << "doc/src/snippets/qtquick1/imports";
}

QStringList tst_qqmlparser::findFiles(const QDir &d)
{
    for (int ii = 0; ii < excludedDirs.count(); ++ii) {
        QString s = excludedDirs.at(ii);
        if (d.absolutePath().endsWith(s))
            return QStringList();
    }

    QStringList rv;

    QStringList files = d.entryList(QStringList() << QLatin1String("*.qml") << QLatin1String("*.js"),
                                    QDir::Files);
    foreach (const QString &file, files) {
        rv << d.absoluteFilePath(file);
    }

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                   QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        QDir sub = d;
        sub.cd(dir);
        rv << findFiles(sub);
    }

    return rv;
}

/*
This test checks all the qml and js files in the QtQml UI source tree
and ensures that the subnode's source locations are inside parent node's source locations
*/

#if !defined(QTEST_CROSS_COMPILED) // sources not available when cross compiled
void tst_qqmlparser::qmlParser_data()
{
    QTest::addColumn<QString>("file");

    QString examples = QLatin1String(SRCDIR) + "/../../../../examples/";
    QString tests = QLatin1String(SRCDIR) + "/../../../../tests/";

    QStringList files;
    files << findFiles(QDir(examples));
    files << findFiles(QDir(tests));

    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;
}
#endif

#if !defined(QTEST_CROSS_COMPILED) // sources not available when cross compiled
void tst_qqmlparser::qmlParser()
{
    QFETCH(QString, file);

    using namespace QQmlJS;

    QString code;

    QFile f(file);
    if (f.open(QFile::ReadOnly))
        code = QString::fromUtf8(f.readAll());

    const bool qmlMode = file.endsWith(QLatin1String(".qml"));

    Engine engine;
    Lexer lexer(&engine);
    lexer.setCode(code, 1, qmlMode);
    Parser parser(&engine);
    if (qmlMode)
        parser.parse();
    else
        parser.parseProgram();

    check::Check chk;
    chk(parser.rootNode());
}
#endif

QTEST_MAIN(tst_qqmlparser)

#include "tst_qqmlparser.moc"
