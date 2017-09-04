/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>
#include <QQuickWebEngineProfile>
#include <private/qquickwebengineview_p.h>

#define INSPECTOR_SERVER_PORT "23654"
static const QUrl s_inspectorServerHttpBaseUrl("http://localhost:" INSPECTOR_SERVER_PORT);

class tst_InspectorServer : public QObject {
    Q_OBJECT
public:
    tst_InspectorServer();

private Q_SLOTS:
    void init();
    void cleanup();

    void testPageList();
    void testRemoteDebuggingMessage();
    void openRemoteDebuggingSession();
private:
    void prepareWebViewComponent();
    inline QQuickWebEngineView* newWebView();
    inline QQuickWebEngineView* webView() const;
    QJsonArray fetchPageList() const;
    QScopedPointer<QQuickWebEngineView> m_webView;
    QScopedPointer<QQmlComponent> m_component;
};

tst_InspectorServer::tst_InspectorServer()
{
    qputenv("QTWEBENGINE_REMOTE_DEBUGGING", INSPECTOR_SERVER_PORT);
    QtWebEngine::initialize();
    QQuickWebEngineProfile::defaultProfile()->setOffTheRecord(true);
    prepareWebViewComponent();
}

void tst_InspectorServer::prepareWebViewComponent()
{
    static QQmlEngine* engine = new QQmlEngine(this);
    engine->addImportPath(QString::fromUtf8(IMPORT_DIR));

    m_component.reset(new QQmlComponent(engine, this));

    m_component->setData(QByteArrayLiteral("import QtQuick 2.0\n"
                                           "import QtWebEngine 1.2\n"
                                           "WebEngineView { }")
                        , QUrl());
}

QQuickWebEngineView* tst_InspectorServer::newWebView()
{
    QObject* viewInstance = m_component->create();

    return qobject_cast<QQuickWebEngineView*>(viewInstance);
}

void tst_InspectorServer::init()
{
    m_webView.reset(newWebView());
}

void tst_InspectorServer::cleanup()
{
    m_webView.reset();
}

inline QQuickWebEngineView* tst_InspectorServer::webView() const
{
    return m_webView.data();
}

QJsonArray tst_InspectorServer::fetchPageList() const
{
    QNetworkAccessManager qnam;
    QScopedPointer<QNetworkReply> reply(qnam.get(QNetworkRequest(s_inspectorServerHttpBaseUrl.resolved(QUrl("json/list")))));
    QSignalSpy(reply.data(), SIGNAL(finished())).wait();
    return QJsonDocument::fromJson(reply->readAll()).array();
}

void tst_InspectorServer::testPageList()
{
    const QUrl testPageUrl = QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));
    QSignalSpy loadSpy(webView(), SIGNAL(loadingChanged(QQuickWebEngineLoadRequest*)));
    webView()->setUrl(testPageUrl);
    QTRY_VERIFY(loadSpy.size() && !webView()->isLoading());

    // Our page has developerExtrasEnabled and should be the only one in the list.
    QJsonArray pageList = fetchPageList();
    QCOMPARE(pageList.size(), 1);
    QCOMPARE(testPageUrl.toString(), pageList.at(0).toObject().value("url").toString());
}

void tst_InspectorServer::testRemoteDebuggingMessage()
{
    QJsonArray pageList = fetchPageList();
    QCOMPARE(pageList.size(), 1);
    QVERIFY(pageList.at(0).toObject().contains("webSocketDebuggerUrl"));

    // Test sending a raw remote debugging message through our web socket server.
    // For this specific message see: http://code.google.com/chrome/devtools/docs/protocol/tot/runtime.html#command-evaluate
    QLatin1String jsExpression("2 + 2");
    QLatin1String jsExpressionResult("4");
    QScopedPointer<QQuickWebEngineView> webSocketQueryWebView(newWebView());
    webSocketQueryWebView->loadHtml(QString(
        "<script type=\"text/javascript\">\n"
        "var socket = new WebSocket('%1');\n"
        "socket.onmessage = function(message) {\n"
            "var response = JSON.parse(message.data);\n"
            "if (response.id === 1)\n"
                "document.title = response.result.result.value;\n"
        "}\n"
        "socket.onopen = function() {\n"
            "socket.send('{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": {\"expression\": \"%2\" } }');\n"
        "}\n"
        "</script>")
        .arg(pageList.at(0).toObject().value("webSocketDebuggerUrl").toString())
        .arg(jsExpression));

    QTRY_COMPARE(webSocketQueryWebView->title(), jsExpressionResult);
}

void tst_InspectorServer::openRemoteDebuggingSession()
{
    QJsonArray pageList = fetchPageList();
    QCOMPARE(pageList.size(), 1);
    QVERIFY(pageList.at(0).toObject().contains("devtoolsFrontendUrl"));

    QScopedPointer<QQuickWebEngineView> inspectorWebView(newWebView());
    inspectorWebView->setUrl(s_inspectorServerHttpBaseUrl.resolved(QUrl(pageList.at(0).toObject().value("devtoolsFrontendUrl").toString())));

    // To test the whole pipeline this exploits a behavior of the inspector front-end which won't provide any title unless the
    // debugging session was established correctly through web socket.
    // So this test case will fail if:
    // - The page list didn't return a valid inspector URL
    // - Or the front-end couldn't be loaded through the inspector HTTP server
    // - Or the web socket connection couldn't be established between the front-end and the page through the inspector server
    QTRY_VERIFY(inspectorWebView->title().startsWith("Developer Tools -"));
}

QTEST_MAIN(tst_InspectorServer)

#include "tst_inspectorserver.moc"
