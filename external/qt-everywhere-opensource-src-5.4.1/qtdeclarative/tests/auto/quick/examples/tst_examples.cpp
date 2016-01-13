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
#include <QLibraryInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>

static QtMessageHandler testlibMsgHandler = 0;
void msgHandlerFilter(QtMsgType type, const QMessageLogContext &ctxt, const QString &msg)
{
    if (type == QtCriticalMsg || type == QtFatalMsg)
        (*testlibMsgHandler)(type, ctxt, msg);
}

class tst_examples : public QObject
{
    Q_OBJECT
public:
    tst_examples();
    ~tst_examples();

private slots:
    void init();
    void cleanup();

    void sgexamples_data();
    void sgexamples();
    void sgsnippets_data();
    void sgsnippets();

    void namingConvention();
private:
    QStringList excludedDirs;
    QStringList excludedFiles;

    void namingConvention(const QDir &);
    QStringList findQmlFiles(const QDir &);

    QQmlEngine engine;
};

tst_examples::tst_examples()
{
    // Add files to exclude here
    excludedFiles << "examples/quick/canvas/tiger/tiger.qml"; // QTBUG-26528
    excludedFiles << "snippets/qml/listmodel/listmodel.qml"; //Just a ListModel, no root QQuickItem
    excludedFiles << "examples/quick/demos/photosurface/photosurface.qml"; // root item is Window rather than Item

    // Add directories you want excluded here (don't add examples/, because they install to examples/qtdeclarative/)
    excludedDirs << "shared"; //Not an example
    excludedDirs << "quick/text/fonts"; // QTBUG-29004
    excludedDirs << "snippets/qml/path"; //No root QQuickItem
    excludedDirs << "tutorials/gettingStartedQml"; //C++ example, but no cpp files in root dir

    // These snippets are not expected to run on their own.
    excludedDirs << "snippets/qml/visualdatamodel_rootindex";
    excludedDirs << "snippets/qml/qtbinding";
    excludedDirs << "snippets/qml/imports";

#ifdef QT_NO_WEBKIT
    excludedDirs << "qtquick/modelviews/webview";
    excludedDirs << "demos/webbrowser";
    excludedDirs << "doc/src/snippets/qml/webview";
#endif

#ifdef QT_NO_XMLPATTERNS
    excludedDirs << "demos/twitter";
    excludedDirs << "demos/flickr";
    excludedDirs << "demos/photoviewer";
    excludedDirs << "snippets/qml/xmlrole.qml";
#endif

}

tst_examples::~tst_examples()
{
}

void tst_examples::init()
{
    if (!qstrcmp(QTest::currentTestFunction(), "sgsnippets"))
        testlibMsgHandler = qInstallMessageHandler(msgHandlerFilter);
}

void tst_examples::cleanup()
{
    if (!qstrcmp(QTest::currentTestFunction(), "sgsnippets"))
        qInstallMessageHandler(testlibMsgHandler);
}

/*
This tests that the examples follow the naming convention required
to have them tested by the examples() test.
*/
void tst_examples::namingConvention(const QDir &d)
{
    for (int ii = 0; ii < excludedDirs.count(); ++ii) {
        QString s = excludedDirs.at(ii);
        if (d.absolutePath().endsWith(s))
            return;
    }

    QStringList files = d.entryList(QStringList() << QLatin1String("*.qml"),
                                    QDir::Files);

    bool seenQml = !files.isEmpty();
    bool seenLowercase = false;

    foreach (const QString &file, files) {
        if (file.at(0).isLower())
            seenLowercase = true;
    }

    if (!seenQml) {
        QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                QDir::NoSymLinks);
        foreach (const QString &dir, dirs) {
            QDir sub = d;
            sub.cd(dir);
            namingConvention(sub);
        }
    } else if(!seenLowercase) {
        // QTBUG-28271 don't fail, but rather warn only
        qWarning() << QString(
            "Directory %1 violates naming convention; expected at least one qml file "
            "starting with lower case, got: %2"
        ).arg(d.absolutePath()).arg(files.join(","));

//        QFAIL(qPrintable(QString(
//            "Directory %1 violates naming convention; expected at least one qml file "
//            "starting with lower case, got: %2"
//        ).arg(d.absolutePath()).arg(files.join(","))));
    }
}

void tst_examples::namingConvention()
{
    QStringList examplesLocations;
    examplesLocations << QLibraryInfo::location(QLibraryInfo::ExamplesPath) + QLatin1String("/qtdeclarative");
    examplesLocations << QLibraryInfo::location(QLibraryInfo::ExamplesPath) + QLatin1String("/qtquick");
    examplesLocations << QLibraryInfo::location(QLibraryInfo::ExamplesPath) + QLatin1String("/qtqml");

    foreach(const QString &examples, examplesLocations) {
        QDir d(examples);
        if (d.exists())
            namingConvention(d);
    }
}

QStringList tst_examples::findQmlFiles(const QDir &d)
{
    for (int ii = 0; ii < excludedDirs.count(); ++ii) {
        QString s = excludedDirs.at(ii);
        if (d.absolutePath().endsWith(s))
            return QStringList();
    }

    QStringList rv;

    QStringList cppfiles = d.entryList(QStringList() << QLatin1String("*.cpp"), QDir::Files);
    if (cppfiles.isEmpty()) {
        QStringList files = d.entryList(QStringList() << QLatin1String("*.qml"),
                                        QDir::Files);
        foreach (const QString &file, files) {
            if (file.at(0).isLower()) {
                bool superContinue = false;
                for (int ii = 0; ii < excludedFiles.count(); ++ii) {
                    QString e = excludedFiles.at(ii);
                    if (d.absoluteFilePath(file).endsWith(e)) {
                        superContinue = true;
                        break;
                    }
                }
                if (superContinue)
                    continue;
                rv << d.absoluteFilePath(file);
            }
        }
    }


    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                   QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        QDir sub = d;
        sub.cd(dir);
        rv << findQmlFiles(sub);
    }

    return rv;
}

/*
This test runs all the examples in the QtQml UI source tree and ensures
that they start and exit cleanly.

Examples are any .qml files under the examples/ directory that start
with a lower case letter.
*/
void tst_examples::sgexamples_data()
{
    QTest::addColumn<QString>("file");

    QString examples = QLatin1String(SRCDIR) + "/../../../../examples/";

    QStringList files;
    files << findQmlFiles(QDir(examples));

    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;
}

void tst_examples::sgexamples()
{
    QFETCH(QString, file);
    QQuickWindow window;
    window.setPersistentOpenGLContext(true);
    window.setPersistentSceneGraph(true);

    QQmlComponent component(&engine, QUrl::fromLocalFile(file));
    if (component.status() == QQmlComponent::Error)
        qWarning() << component.errors();
    QCOMPARE(component.status(), QQmlComponent::Ready);

    QScopedPointer<QObject> object(component.beginCreate(engine.rootContext()));
    QQuickItem *root = qobject_cast<QQuickItem *>(object.data());
    if (!root)
        component.completeCreate();
    QVERIFY(root);

    window.resize(240, 320);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    root->setParentItem(window.contentItem());
    component.completeCreate();

    qApp->processEvents();
}

void tst_examples::sgsnippets_data()
{
    QTest::addColumn<QString>("file");

    QString snippets = QLatin1String(SRCDIR) + "/../../../../src/qml/doc/snippets/qml";
    QStringList files;
    files << findQmlFiles(QDir(snippets));
    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;

    snippets = QLatin1String(SRCDIR) + "/../../../../src/quick/doc/snippets/qml";
    files.clear();
    files << findQmlFiles(QDir(snippets));
    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;
}

void tst_examples::sgsnippets()
{
    QQuickWindow window;

    QFETCH(QString, file);

    QQmlComponent component(&engine, QUrl::fromLocalFile(file));
    if (component.status() == QQmlComponent::Error)
        qWarning() << component.errors();
    QCOMPARE(component.status(), QQmlComponent::Ready);

    QScopedPointer<QObject> object(component.beginCreate(engine.rootContext()));
    QQuickItem *root = qobject_cast<QQuickItem *>(object.data());
    if (!root)
        component.completeCreate();
    QVERIFY(root);

    window.resize(240, 320);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    root->setParentItem(window.contentItem());
    component.completeCreate();

    qApp->processEvents();
}

QTEST_MAIN(tst_examples)

#include "tst_examples.moc"
