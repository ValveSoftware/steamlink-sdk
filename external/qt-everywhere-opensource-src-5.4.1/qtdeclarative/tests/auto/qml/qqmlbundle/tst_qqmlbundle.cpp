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

// Lookup of libraries
// Test bundle as a qmldir

#include <qtest.h>
#include <QDebug>
#include <QQmlEngine>
#include <QQmlComponent>
#include "../../shared/util.h"
#include <private/qqmlbundle_p.h>

class tst_qqmlbundle : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlbundle() {}

private slots:
    void initTestCase();

    void componentFromBundle();
    void relativeResolution();
    void bundleImport();
    void relativeQmldir();

    void import();

private:
    QStringList findFiles(const QDir &d);
    bool makeBundle(const QString &path, const QString &name);
};

void tst_qqmlbundle::initTestCase()
{
    QQmlDataTest::initTestCase();
}

// Test we create a QQmlComponent for a file inside a bundle
void tst_qqmlbundle::componentFromBundle()
{
    QVERIFY(makeBundle(testFile("componentFromBundle"), "my.bundle"));

    QQmlEngine engine;
    engine.addNamedBundle("mybundle", testFile("componentFromBundle/my.bundle"));

    QQmlComponent component(&engine, QUrl("bundle://mybundle/test.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toInt(), 11);
    QCOMPARE(o->property("test2").toBool(), true);

    delete o;
}

// Tests that relative QML components are resolved without a qmldir
void tst_qqmlbundle::relativeResolution()
{
    // Root of the bundle
    {
    QVERIFY(makeBundle(testFile("relativeResolution.1"), "my.bundle"));

    QQmlEngine engine;
    engine.addNamedBundle("mybundle", testFile("relativeResolution.1/my.bundle"));

    QQmlComponent component(&engine, QUrl("bundle://mybundle/test.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toInt(), 11);
    QCOMPARE(o->property("test2").toBool(), true);

    delete o;
    }

    // Non-root of the bundle
    {
    QVERIFY(makeBundle(testFile("relativeResolution.2"), "my.bundle"));

    QQmlEngine engine;
    engine.addNamedBundle("mybundle", testFile("relativeResolution.2/my.bundle"));

    QQmlComponent component(&engine, QUrl("bundle://mybundle/subdir/test.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toInt(), 11);
    QCOMPARE(o->property("test2").toBool(), true);

    delete o;
    }
}

// Test that a bundle can be imported explicitly from outside a bundle
void tst_qqmlbundle::bundleImport()
{
    QVERIFY(makeBundle(testFile("bundleImport"), "my.bundle"));

    QQmlEngine engine;
    engine.addNamedBundle("mybundle", testFile("bundleImport/my.bundle"));

    {
    QQmlComponent component(&engine, testFileUrl("bundleImport/bundleImport.1.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toReal(), qreal(1918));
    QCOMPARE(o->property("test2").toString(), QString("Hello world!"));

    delete o;
    }

    {
    QQmlComponent component(&engine, testFileUrl("bundleImport/bundleImport.2.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toReal(), qreal(1432));
    QCOMPARE(o->property("test2").toString(), QString("Jeronimo"));

    delete o;
    }
}

// Test a relative import inside a bundle uses qmldir
void tst_qqmlbundle::relativeQmldir()
{
    QVERIFY(makeBundle(testFile("relativeQmldir"), "my.bundle"));

    QQmlEngine engine;
    engine.addNamedBundle("mybundle", testFile("relativeQmldir/my.bundle"));

    QQmlComponent component(&engine, QUrl("bundle://mybundle/test.qml"));
    QVERIFY(component.isReady());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toReal(), qreal(67));
    QCOMPARE(o->property("test2").toReal(), qreal(88));

    delete o;
}

// Test C++ plugins are resolved relative to the bundle container file
void tst_qqmlbundle::import()
{
    QVERIFY(makeBundle(testFile("imports/bundletest"), "qmldir"));

    QQmlEngine engine;
    engine.addImportPath(testFile("imports"));

    QQmlComponent component(&engine, testFileUrl("import.qml"));
    QVERIFY2(component.isReady(), QQmlDataTest::msgComponentError(component, &engine));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("value").toInt(), 32);

    delete o;
}

// Transform the data available under <path>/bundledata to a bundle named <path>/<name>
bool tst_qqmlbundle::makeBundle(const QString &path, const QString &name)
{
    QDir dir(path);
    dir.remove(name);

    QDir bundleDir = dir;
    if (!bundleDir.cd("bundledata"))
        return false;

    QStringList fileNames = findFiles(bundleDir);

    QString bundleFile = dir.absolutePath() + QDir::separator() + name;

    QQmlBundle bundle(bundleFile);
    if (!bundle.open(QFile::WriteOnly))
        return false;

    foreach (const QString &fileName, fileNames) {
        QString shortFileName = fileName.mid(bundleDir.absolutePath().length() + 1);
        bundle.add(shortFileName, fileName);
    }

    return true;
}

QStringList tst_qqmlbundle::findFiles(const QDir &d)
{
    QStringList rv;

    QStringList files = d.entryList(QDir::Files);
    foreach (const QString &file, files)
        rv << d.absoluteFilePath(file);

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        QDir sub = d;
        sub.cd(dir);
        rv << findFiles(sub);
    }

    return rv;
}

QTEST_MAIN(tst_qqmlbundle)

#include "tst_qqmlbundle.moc"
