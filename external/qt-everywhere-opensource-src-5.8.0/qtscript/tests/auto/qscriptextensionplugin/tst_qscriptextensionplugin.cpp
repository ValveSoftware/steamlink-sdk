/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#include <QtScript/qscriptengine.h>

class tst_QScriptExtensionPlugin : public QObject
{
    Q_OBJECT

public:
    tst_QScriptExtensionPlugin();

private slots:
    void initTestCase();
    void importSimplePlugin();
    void importStaticPlugin();

private:
    const QString m_pluginsDirectory;

};

tst_QScriptExtensionPlugin::tst_QScriptExtensionPlugin() :
    m_pluginsDirectory(QFINDTESTDATA("plugins"))
{
}

void tst_QScriptExtensionPlugin::initTestCase()
{
    QVERIFY2(!m_pluginsDirectory.isEmpty(), "'plugins' directory not found");
}

void tst_QScriptExtensionPlugin::importSimplePlugin()
{
    QScriptEngine eng;
    QCoreApplication::addLibraryPath(m_pluginsDirectory);

    QVERIFY(eng.importedExtensions().isEmpty());

    QStringList available = eng.availableExtensions();
    QVERIFY(available.contains("simple"));
    QVERIFY(available.contains("simple.foo"));
    QVERIFY(available.contains("simple.foo.bar"));

    QScriptValue extensionObject;
    {
        QVERIFY(eng.importExtension("simple").isUndefined());
        QCOMPARE(eng.importedExtensions().size(), 1);
        QCOMPARE(eng.importedExtensions().at(0), QString::fromLatin1("simple"));
        QVERIFY(eng.availableExtensions().contains("simple"));
        QVERIFY(eng.globalObject().property("pluginKey").equals("simple"));
        QVERIFY(eng.globalObject().property("package").isObject());
        extensionObject = eng.globalObject().property("simple");
        QVERIFY(extensionObject.isObject());
        QVERIFY(extensionObject.equals(eng.globalObject().property("package")));
    }

    {
        QVERIFY(eng.importExtension("simple.foo").isUndefined());
        QCOMPARE(eng.importedExtensions().size(), 2);
        QCOMPARE(eng.importedExtensions().at(1), QString::fromLatin1("simple.foo"));
        QVERIFY(eng.availableExtensions().contains("simple.foo"));
        QVERIFY(eng.globalObject().property("pluginKey").equals("simple.foo"));
        QVERIFY(eng.globalObject().property("package").isObject());
        QVERIFY(!extensionObject.equals(eng.globalObject().property("package")));
        QVERIFY(extensionObject.equals(eng.globalObject().property("simple")));
        QVERIFY(extensionObject.property("foo").isObject());
        QVERIFY(extensionObject.property("foo").equals(eng.globalObject().property("package")));
    }

    {
        QVERIFY(eng.importExtension("simple.foo.bar").isUndefined());
        QCOMPARE(eng.importedExtensions().size(), 3);
        QCOMPARE(eng.importedExtensions().at(2), QString::fromLatin1("simple.foo.bar"));
        QVERIFY(eng.availableExtensions().contains("simple.foo.bar"));
        QVERIFY(eng.globalObject().property("pluginKey").equals("simple.foo.bar"));
        QVERIFY(eng.globalObject().property("package").isObject());
        QVERIFY(!extensionObject.equals(eng.globalObject().property("package")));
        QVERIFY(extensionObject.equals(eng.globalObject().property("simple")));
        QVERIFY(extensionObject.property("foo").property("bar").isObject());
        QVERIFY(extensionObject.property("foo").property("bar").equals(eng.globalObject().property("package")));
    }

    // Extensions can't be imported multiple times.
    eng.globalObject().setProperty("pluginKey", QScriptValue());
    QVERIFY(eng.importExtension("simple").isUndefined());
    QCOMPARE(eng.importedExtensions().size(), 3);
    QVERIFY(!eng.globalObject().property("pluginKey").isValid());

    QVERIFY(eng.importExtension("simple.foo").isUndefined());
    QCOMPARE(eng.importedExtensions().size(), 3);
    QVERIFY(!eng.globalObject().property("pluginKey").isValid());

    QVERIFY(eng.importExtension("simple.foo.bar").isUndefined());
    QCOMPARE(eng.importedExtensions().size(), 3);
    QVERIFY(!eng.globalObject().property("pluginKey").isValid());
}

void tst_QScriptExtensionPlugin::importStaticPlugin()
{
    Q_INIT_RESOURCE(staticplugin);
    QScriptEngine eng;
    QVERIFY(eng.availableExtensions().contains("static"));
    QVERIFY(eng.importExtension("static").isUndefined());
    QCOMPARE(eng.importedExtensions().size(), 1);
    QCOMPARE(eng.importedExtensions().at(0), QString::fromLatin1("static"));
    QVERIFY(eng.availableExtensions().contains("static"));
    QVERIFY(eng.globalObject().property("pluginKey").equals("static"));

    // Verify that :/qtscriptextension/static/__init__.js was evaluated.
    QVERIFY(eng.evaluate("spy").isObject());
    QVERIFY(eng.evaluate("spy.extension").equals("static"));
    QVERIFY(eng.evaluate("spy.setupPackage").isFunction());
    QVERIFY(eng.evaluate("spy.postInit").isUndefined());

    QVERIFY(eng.evaluate("postInitWasCalled").isBool());
    QVERIFY(eng.evaluate("postInitWasCalled").toBool());

    // Extensions can't be imported multiple times.
    eng.globalObject().setProperty("pluginKey", QScriptValue());
    QVERIFY(eng.importExtension("static").isUndefined());
    QCOMPARE(eng.importedExtensions().size(), 1);
    QVERIFY(!eng.globalObject().property("pluginKey").isValid());
}

Q_IMPORT_PLUGIN(StaticPlugin)

QTEST_MAIN(tst_QScriptExtensionPlugin)
#include "tst_qscriptextensionplugin.moc"
