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
#include <qdeclarativedatatest.h>
#include <qdir.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QDebug>
#include <QtCore/qlibraryinfo.h>

#include "../shared/testhttpserver.h"

#define SERVER_ADDR "http://127.0.0.1:14450"
#define SERVER_PORT 14450


class tst_qdeclarativemoduleplugin : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativemoduleplugin()
    {
    }

private slots:
    void importsPlugin();
    void importsPlugin2();
    void importsPlugin21();
    void importsMixedQmlCppPlugin();
    void incorrectPluginCase();
    void importPluginWithQmlFile();
    void remoteImportWithQuotedUrl();
    void remoteImportWithUnquotedUri();
    void versionNotInstalled();
    void versionNotInstalled_data();
};

#define VERIFY_ERRORS(errorfile) \
    if (!errorfile) { \
        if (qgetenv("DEBUG") != "" && !component.errors().isEmpty()) \
            qWarning() << "Unexpected Errors:" << component.errors(); \
        QVERIFY(!component.isError()); \
        QVERIFY(component.errors().isEmpty()); \
    } else { \
        QFile file(testFile(errorfile)); \
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text)); \
        QByteArray data = file.readAll(); \
        file.close(); \
        QList<QByteArray> expected = data.split('\n'); \
        expected.removeAll(QByteArray("")); \
        QList<QDeclarativeError> errors = component.errors(); \
        QList<QByteArray> actual; \
        for (int ii = 0; ii < errors.count(); ++ii) { \
            const QDeclarativeError &error = errors.at(ii); \
            QByteArray errorStr = QByteArray::number(error.line()) + ":" +  \
                                  QByteArray::number(error.column()) + ":" + \
                                  error.description().toUtf8(); \
            actual << errorStr; \
        } \
        if (qgetenv("DEBUG") != "" && expected != actual) \
            qWarning() << "Expected:" << expected << "Actual:" << actual;  \
        if (qgetenv("QDECLARATIVELANGUAGE_UPDATEERRORS") != "" && expected != actual) {\
            QFile file(testFile(errorfile)); \
            QVERIFY(file.open(QIODevice::WriteOnly)); \
            for (int ii = 0; ii < actual.count(); ++ii) { \
                file.write(actual.at(ii)); file.write("\n"); \
            } \
            file.close(); \
        } else { \
            QCOMPARE(expected, actual); \
        } \
    }

void tst_qdeclarativemoduleplugin::importsPlugin()
{
    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());
    QTest::ignoreMessage(QtWarningMsg, "plugin created");
    QTest::ignoreMessage(QtWarningMsg, "import worked");
    QDeclarativeComponent component(&engine, testFileUrl("works.qml"));
    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("value").toInt(),123);
    delete object;
}

void tst_qdeclarativemoduleplugin::importsPlugin2()
{
    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());
    QTest::ignoreMessage(QtWarningMsg, "plugin2 created");
    QTest::ignoreMessage(QtWarningMsg, "import2 worked");
    QDeclarativeComponent component(&engine, testFileUrl("works2.qml"));
    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("value").toInt(),123);
    delete object;
}

void tst_qdeclarativemoduleplugin::importsPlugin21()
{
    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());
    QTest::ignoreMessage(QtWarningMsg, "plugin2.1 created");
    QTest::ignoreMessage(QtWarningMsg, "import2.1 worked");
    QDeclarativeComponent component(&engine, testFileUrl("works21.qml"));
    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("value").toInt(),123);
    delete object;
}

void tst_qdeclarativemoduleplugin::incorrectPluginCase()
{
    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());

    QDeclarativeComponent component(&engine, testFileUrl("incorrectCase.qml"));

    QList<QDeclarativeError> errors = component.errors();
    QCOMPARE(errors.count(), 1);

    QString expectedError = QLatin1String("module \"org.qtproject.WrongCase\" plugin \"PluGin\" not found");

#if defined(Q_OS_MAC) || defined(Q_OS_WIN32)
    bool caseSensitive = true;
#if defined(Q_OS_MAC)
    caseSensitive = pathconf(QDir::currentPath().toLatin1().constData(), _PC_CASE_SENSITIVE);
    QString libname = "libPluGin.dylib";
#elif defined(Q_OS_WIN32)
    caseSensitive = false;
    QString libname = "PluGin.dll";
#endif
    if (!caseSensitive)
        expectedError = QLatin1String("plugin cannot be loaded for module \"org.qtproject.WrongCase\": File name case mismatch for \"") + QDir(importsDirectory()).filePath("org/qtproject/WrongCase/" + libname) + QLatin1String("\"");
#endif

    QCOMPARE(errors.at(0).description(), expectedError);
}

void tst_qdeclarativemoduleplugin::importPluginWithQmlFile()
{
    QString path = importsDirectory();

    // QTBUG-16885: adding an import path with a lower-case "c:" causes assert failure
    // (this only happens if the plugin includes pure QML files)
    #ifdef Q_OS_WIN
    if (path.size() > 1 && path.at(0).isUpper() && path.at(1) == QLatin1Char(':'))
        path = path.at(0).toLower() + path.mid(1);
    #endif

    QDeclarativeEngine engine;
    engine.addImportPath(path);

    QDeclarativeComponent component(&engine, testFileUrl("pluginWithQmlFile.qml"));
    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    delete object;
}

void tst_qdeclarativemoduleplugin::remoteImportWithQuotedUrl()
{
    TestHTTPServer server(SERVER_PORT);
    QVERIFY(server.isValid());
    server.serveDirectory(importsDirectory());

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData("import \"http://127.0.0.1:14450/org/qtproject/PureQmlModule\" \nComponentA { width: 300; ComponentB{} }", QUrl());

    QTRY_COMPARE(component.status(), QDeclarativeComponent::Ready);
    QObject *object = component.create();
    QCOMPARE(object->property("width").toInt(), 300);
    QVERIFY(object != 0);
    delete object;

    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
}

void tst_qdeclarativemoduleplugin::remoteImportWithUnquotedUri()
{
    TestHTTPServer server(SERVER_PORT);
    QVERIFY(server.isValid());
    server.serveDirectory(directory() + QStringLiteral("/imports"));

    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());
    QDeclarativeComponent component(&engine);
    component.setData("import org.qtproject.PureQmlModule 1.0 \nComponentA { width: 300; ComponentB{} }", QUrl());


    QTRY_COMPARE(component.status(), QDeclarativeComponent::Ready);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("width").toInt(), 300);
    delete object;

    foreach (QDeclarativeError err, component.errors())
        qWarning() << err;
    VERIFY_ERRORS(0);
}

// QTBUG-17324
void tst_qdeclarativemoduleplugin::importsMixedQmlCppPlugin()
{
    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());

    {
    QDeclarativeComponent component(&engine, testFileUrl("importsMixedQmlCppPlugin.qml"));

    QObject *o = component.create();
    QVERIFY2(o, msgComponentError(component, &engine).constData());
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("importsMixedQmlCppPlugin.2.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    delete o;
    }


}

void tst_qdeclarativemoduleplugin::versionNotInstalled_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");

    QTest::newRow("versionNotInstalled") << "versionNotInstalled.qml" << "versionNotInstalled.errors.txt";
    QTest::newRow("versionNotInstalled") << "versionNotInstalled.2.qml" << "versionNotInstalled.2.errors.txt";
}

void tst_qdeclarativemoduleplugin::versionNotInstalled()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);

    QDeclarativeEngine engine;
    engine.addImportPath(importsDirectory());

    QDeclarativeComponent component(&engine, testFileUrl(file));
    VERIFY_ERRORS(errorFile.toLatin1().constData());
}


QTEST_MAIN(tst_qdeclarativemoduleplugin)

#include "tst_qdeclarativemoduleplugin.moc"
