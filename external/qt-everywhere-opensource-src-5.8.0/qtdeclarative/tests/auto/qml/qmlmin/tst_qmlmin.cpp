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
#include <QLibraryInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QQmlError>
#include <cstdlib>

class tst_qmlmin : public QObject
{
    Q_OBJECT
public:
    tst_qmlmin();

private slots:
    void initTestCase();
#if !defined(QTEST_CROSS_COMPILED) // sources not available when cross compiled
    void qmlMinify_data();
    void qmlMinify();
#endif

private:
    QString qmlminPath;
    QStringList excludedDirs;
    QStringList invalidFiles;

    QStringList findFiles(const QDir &);
    bool isInvalidFile(const QFileInfo &fileName) const;
};

tst_qmlmin::tst_qmlmin()
{
}

void tst_qmlmin::initTestCase()
{
    qmlminPath = QLibraryInfo::location(QLibraryInfo::BinariesPath) + QLatin1String("/qmlmin");
#ifdef Q_OS_WIN
    qmlminPath += QLatin1String(".exe");
#endif
    if (!QFileInfo(qmlminPath).exists()) {
        QString message = QString::fromLatin1("qmlmin executable not found (looked for %0)")
                .arg(qmlminPath);
        QFAIL(qPrintable(message));
    }

    // Add directories you want excluded here

    // These snippets are not expected to run on their own.
    excludedDirs << "doc/src/snippets/qml/visualdatamodel_rootindex";
    excludedDirs << "doc/src/snippets/qml/qtbinding";
    excludedDirs << "doc/src/snippets/qml/imports";
    excludedDirs << "doc/src/snippets/qtquick1/visualdatamodel_rootindex";
    excludedDirs << "doc/src/snippets/qtquick1/qtbinding";
    excludedDirs << "doc/src/snippets/qtquick1/imports";
    excludedDirs << "tests/manual/v4";
    excludedDirs << "tests/auto/qml/qmllint";

    // Add invalid files (i.e. files with syntax errors)
    invalidFiles << "tests/auto/quick/qquickloader/data/InvalidSourceComponent.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/signal.2.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/signal.3.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/signal.5.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/property.4.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/empty.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/missingObject.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/insertedSemicolon.1.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/nonexistantProperty.5.qml";
    invalidFiles << "tests/auto/qml/qqmllanguage/data/invalidRoot.1.qml";
    invalidFiles << "tests/auto/qml/qquickfolderlistmodel/data/dummy.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/qtbug_22843.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/qtbug_22843.library.js";
    invalidFiles << "tests/auto/qml/qquickworkerscript/data/script_error_onLoad.js";
    invalidFiles << "tests/auto/qml/parserstress/tests/ecma_3/Unicode/regress-352044-02-n.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/incrDecrSemicolon_error1.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedFileQualifier.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedFileQualifier.2.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedImport.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedModule.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedFile.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedModuleQualifier.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedModuleQualifier.2.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/malformedModuleVersion.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/missingFileQualifier.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/missingModuleQualifier.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/jsimportfail/missingModuleVersion.js";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.1.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.2.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.3.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.4.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.5.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/stringParsing_error.6.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/numberParsing_error.1.qml";
    invalidFiles << "tests/auto/qml/qqmlecmascript/data/numberParsing_error.2.qml";
}

QStringList tst_qmlmin::findFiles(const QDir &d)
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

bool tst_qmlmin::isInvalidFile(const QFileInfo &fileName) const
{
    foreach (const QString &invalidFile, invalidFiles) {
        if (fileName.absoluteFilePath().endsWith(invalidFile))
            return true;
    }
    return false;
}

/*
This test runs all the examples in the Qt QML UI source tree and ensures
that they start and exit cleanly.

Examples are any .qml files under the examples/ directory that start
with a lower case letter.
*/

#if !defined(QTEST_CROSS_COMPILED) // sources not available when cross compiled
void tst_qmlmin::qmlMinify_data()
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
void tst_qmlmin::qmlMinify()
{
    QFETCH(QString, file);

    QProcess qmlminify;

    // Restrict line width to 100 characters
    qmlminify.start(qmlminPath, QStringList() << QLatin1String("--verify-only") << QLatin1String("-w100") << file);
    qmlminify.waitForFinished();

    QCOMPARE(qmlminify.error(), QProcess::UnknownError);
    QCOMPARE(qmlminify.exitStatus(), QProcess::NormalExit);

    if (isInvalidFile(file))
        QCOMPARE(qmlminify.exitCode(), EXIT_FAILURE); // cannot minify files with syntax errors
    else
        QCOMPARE(qmlminify.exitCode(), 0);
}
#endif

QTEST_MAIN(tst_qmlmin)

#include "tst_qmlmin.moc"
