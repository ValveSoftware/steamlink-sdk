/*
    Copyright (C) 2015 The Qt Company Ltd.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>

#include <qwebenginepage.h>
#include <qwebenginescript.h>
#include <qwebenginescriptcollection.h>
#include <qwebengineview.h>
#include "../util.h"
#include <QWebChannel>

class tst_QWebEngineScript: public QObject {
    Q_OBJECT

private Q_SLOTS:
    void domEditing();
    void injectionPoint();
    void injectionPoint_data();
    void scriptWorld();
    void scriptModifications();
    void webChannel_data();
    void webChannel();
    void noTransportWithoutWebChannel();
};

void tst_QWebEngineScript::domEditing()
{
    QWebEnginePage page;
    QWebEngineView view;
    view.setPage(&page);
    QWebEngineScript s;
    s.setInjectionPoint(QWebEngineScript::DocumentReady);
    s.setWorldId(QWebEngineScript::ApplicationWorld + 1);
    s.setSourceCode("el = document.createElement(\"div\");\
                el.id = \"banner\";\
                el.style.position = \"absolute\";\
                el.style.width = \"100%\";\
                el.style.padding = \"1em\";\
                el.style.textAlign = \"center\";\
                el.style.top = \"0\";\
                el.style.left = \"0\";\
                el.style.backgroundColor = \"#80C342\";\
                el.innerText = \"Injected banner\";\
                document.body.appendChild(el);");
    page.scripts().insert(s);
    page.load(QUrl("about:blank"));
    view.show();
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "document.getElementById(\"banner\").innerText"), QVariant(QStringLiteral("Injected banner")));
    // elementFromPoint only works for exposed elements
    QTest::qWaitForWindowExposed(&view);
    QCOMPARE(evaluateJavaScriptSync(&page, "document.elementFromPoint(2, 2).id"), QVariant::fromValue(QStringLiteral("banner")));
}

void tst_QWebEngineScript::injectionPoint()
{
    QFETCH(int, injectionPoint);
    QFETCH(QString, testScript);

    QWebEngineScript s;
    s.setSourceCode("var foo = \"foobar\";");
    s.setInjectionPoint(static_cast<QWebEngineScript::InjectionPoint>(injectionPoint));
    s.setWorldId(QWebEngineScript::MainWorld);
    QWebEnginePage page;
    page.scripts().insert(s);
    page.setHtml(QStringLiteral("<html><head><script> var contents;") + testScript
                 + QStringLiteral("document.addEventListener(\"load\", setTimeout(function(event) {\
                                   document.body.innerText = contents;\
                                   }, 550));\
                                   </script></head><body></body></html>"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QTRY_COMPARE(evaluateJavaScriptSync(&page, "document.body.innerText"), QVariant::fromValue(QStringLiteral("SUCCESS")));
}

void tst_QWebEngineScript::injectionPoint_data()
{
    QTest::addColumn<int>("injectionPoint");
    QTest::addColumn<QString>("testScript");
    QTest::newRow("DocumentCreation") << static_cast<int>(QWebEngineScript::DocumentCreation)
                                      << QStringLiteral("var contents = (typeof(foo) == \"undefined\")? \"FAILURE\" : \"SUCCESS\";");
    QTest::newRow("DocumentReady") << static_cast<int>(QWebEngineScript::DocumentReady)
    // use a zero timeout to make sure the user script got a chance to run as the order is undefined.
                                   << QStringLiteral("document.addEventListener(\"DOMContentLoaded\", function() {\
                                                      setTimeout(function() {\
                                                      contents = (typeof(foo) == \"undefined\")? \"FAILURE\" : \"SUCCESS\";\
                                                      }, 0)});");
    QTest::newRow("Deferred") << static_cast<int>(QWebEngineScript::Deferred)
                              << QStringLiteral("document.addEventListener(\"load\", setTimeout(function(event) {\
                                                 contents = (typeof(foo) == \"undefined\")? \"FAILURE\" : \"SUCCESS\";\
                                                 }, 500));");
}

void tst_QWebEngineScript::scriptWorld()
{
    QWebEnginePage page;
    QWebEngineScript script;
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setSourceCode(QStringLiteral("var userScriptTest = 1;"));
    page.scripts().insert(script);
    page.load(QUrl("about:blank"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "typeof(userScriptTest) != \"undefined\" && userScriptTest == 1;"), QVariant::fromValue(true));
    QCOMPARE(evaluateJavaScriptSyncInWorld(&page, "typeof(userScriptTest) == \"undefined\"", QWebEngineScript::ApplicationWorld), QVariant::fromValue(true));
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    page.scripts().clear();
    page.scripts().insert(script);
    page.load(QUrl("about:blank"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "typeof(userScriptTest) == \"undefined\""), QVariant::fromValue(true));
    QCOMPARE(evaluateJavaScriptSyncInWorld(&page, "typeof(userScriptTest) != \"undefined\" && userScriptTest == 1;", QWebEngineScript::ApplicationWorld), QVariant::fromValue(true));
}

void tst_QWebEngineScript::scriptModifications()
{
    QWebEnginePage page;
    QWebEngineScript script;
    script.setName(QStringLiteral("String1"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setSourceCode("var foo = \"SUCCESS\";");
    page.scripts().insert(script);
    page.setHtml(QStringLiteral("<html><head><script>document.addEventListener(\"DOMContentLoaded\", function() {\
                                 document.body.innerText = foo;});\
                                 </script></head><body></body></html>"));
    QVERIFY(page.scripts().count() == 1);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "document.body.innerText"), QVariant::fromValue(QStringLiteral("SUCCESS")));
    script.setSourceCode("var foo = \"FAILURE\"");
    page.triggerAction(QWebEnginePage::ReloadAndBypassCache);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "document.body.innerText"), QVariant::fromValue(QStringLiteral("SUCCESS")));
    QVERIFY(page.scripts().count() == 1);
    QWebEngineScript s = page.scripts().findScript(QStringLiteral("String1"));
    QVERIFY(page.scripts().remove(s));
    QVERIFY(page.scripts().count() == 0);
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
public:
    TestObject(QObject *parent = 0) : QObject(parent) { }

    void setText(const QString &text)
    {
        if (text == m_text)
            return;
        m_text = text;
        emit textChanged(text);
    }

    QString text() const { return m_text; }

signals:
    void textChanged(const QString &text);

private:
    QString m_text;
};


void tst_QWebEngineScript::webChannel_data()
{
    QTest::addColumn<int>("worldId");
    QTest::addColumn<bool>("reloadFirst");
    QTest::newRow("MainWorld") << static_cast<int>(QWebEngineScript::MainWorld) << false;
    QTest::newRow("ApplicationWorld") << static_cast<int>(QWebEngineScript::ApplicationWorld) << false;
    QTest::newRow("MainWorldWithReload") << static_cast<int>(QWebEngineScript::MainWorld) << true;
    QTest::newRow("ApplicationWorldWithReload") << static_cast<int>(QWebEngineScript::ApplicationWorld) << true;
}

void tst_QWebEngineScript::webChannel()
{
    QFETCH(int, worldId);
    QFETCH(bool, reloadFirst);
    QWebEnginePage page;
    TestObject testObject;
    QScopedPointer<QWebChannel> channel(new QWebChannel(this));
    channel->registerObject(QStringLiteral("object"), &testObject);
    page.setWebChannel(channel.data(), worldId);

    QFile qwebchanneljs(":/qwebchannel.js");
    QVERIFY(qwebchanneljs.exists());
    qwebchanneljs.open(QFile::ReadOnly);
    QByteArray scriptSrc = qwebchanneljs.readAll();
    qwebchanneljs.close();
    QWebEngineScript script;
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(worldId);
    script.setSourceCode(QString::fromLatin1(scriptSrc));
    page.scripts().insert(script);
    page.setHtml(QStringLiteral("<html><body></body></html>"));
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    if (reloadFirst) {
        // Check that the transport is also reinstalled on navigation
        page.triggerAction(QWebEnginePage::Reload);
        waitForSignal(&page, SIGNAL(loadFinished(bool)));
    }
    page.runJavaScript(QLatin1String(
                                "new QWebChannel(qt.webChannelTransport,"
                                "  function(channel) {"
                                "    channel.objects.object.text = 'test';"
                                "  }"
                                ");"), worldId);
    waitForSignal(&testObject, SIGNAL(textChanged(QString)));
    QCOMPARE(testObject.text(), QStringLiteral("test"));

    if (worldId != QWebEngineScript::MainWorld)
        QCOMPARE(evaluateJavaScriptSync(&page, "qt.webChannelTransport"), QVariant(QVariant::Invalid));
}

void tst_QWebEngineScript::noTransportWithoutWebChannel()
{
    QWebEnginePage page;
    page.setHtml(QStringLiteral("<html><body></body></html>"));

    QCOMPARE(evaluateJavaScriptSync(&page, "qt.webChannelTransport"), QVariant(QVariant::Invalid));
    page.triggerAction(QWebEnginePage::Reload);
    waitForSignal(&page, SIGNAL(loadFinished(bool)));
    QCOMPARE(evaluateJavaScriptSync(&page, "qt.webChannelTransport"), QVariant(QVariant::Invalid));
}

QTEST_MAIN(tst_QWebEngineScript)

#include "tst_qwebenginescript.moc"
