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

#ifndef UTIL_H
#define UTIL_H

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QtTest>
#include <private/qquickwebengineview_p.h>
#include <private/qquickwebengineloadrequest_p.h>

#if !defined(TESTS_SOURCE_DIR)
#define TESTS_SOURCE_DIR ""
#endif

class LoadSpy : public QEventLoop {
    Q_OBJECT

public:
    LoadSpy(QQuickWebEngineView *webEngineView)
    {
        connect(webEngineView, SIGNAL(loadingChanged(QQuickWebEngineLoadRequest*)), SLOT(onLoadingChanged(QQuickWebEngineLoadRequest*)));
    }

    ~LoadSpy() { }

Q_SIGNALS:
    void loadSucceeded();
    void loadFailed();

private Q_SLOTS:
    void onLoadingChanged(QQuickWebEngineLoadRequest *loadRequest)
    {
        if (loadRequest->status() == QQuickWebEngineView::LoadSucceededStatus)
            emit loadSucceeded();
        else if (loadRequest->status() == QQuickWebEngineView::LoadFailedStatus)
            emit loadFailed();
    }
};

class LoadStartedCatcher : public QObject {
    Q_OBJECT

public:
    LoadStartedCatcher(QQuickWebEngineView *webEngineView)
        : m_webEngineView(webEngineView)
    {
        connect(m_webEngineView, SIGNAL(loadingChanged(QQuickWebEngineLoadRequest*)), this, SLOT(onLoadingChanged(QQuickWebEngineLoadRequest*)));
    }

    virtual ~LoadStartedCatcher() { }

public Q_SLOTS:
    void onLoadingChanged(QQuickWebEngineLoadRequest *loadRequest)
    {
        if (loadRequest->status() == QQuickWebEngineView::LoadStartedStatus)
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

Q_SIGNALS:
    void finished();

private:
    QQuickWebEngineView *m_webEngineView;
};

inline bool waitForLoadSucceeded(QQuickWebEngineView *webEngineView, int timeout = 10000)
{
    LoadSpy loadSpy(webEngineView);
    QSignalSpy spy(&loadSpy, &LoadSpy::loadSucceeded);
    return spy.wait(timeout);
}

inline bool waitForLoadFailed(QQuickWebEngineView *webEngineView, int timeout = 20000)
{
    LoadSpy loadSpy(webEngineView);
    QSignalSpy spy(&loadSpy, &LoadSpy::loadFailed);
    return spy.wait(timeout);
}

inline bool waitForViewportReady(QQuickWebEngineView *webEngineView, int timeout = 10000)
{
#ifdef ENABLE_QML_TESTSUPPORT_API
    QSignalSpy spy(reinterpret_cast<QObject *>(webEngineView->testSupport()), SIGNAL(loadVisuallyCommitted()));
    return spy.wait(timeout);
#else
    Q_UNUSED(webEngineView)
    Q_UNUSED(timeout)
    qFatal("Test Support API is disabled. The result is not reliable.\
            Use the following command to build Test Support module and rebuild WebEngineView API:\
            qmake -r WEBENGINE_CONFIG+=testsupport && make");
    return false;
#endif
}

inline QString bodyInnerText(QQuickWebEngineView *webEngineView)
{
    qRegisterMetaType<QQuickWebEngineView::JavaScriptConsoleMessageLevel>("JavaScriptConsoleMessageLevel");
    QSignalSpy consoleMessageSpy(webEngineView, &QQuickWebEngineView::javaScriptConsoleMessage);

    webEngineView->runJavaScript(
                "if (document.body == null)"
                "   console.log('');"
                "else"
                "   console.log(document.body.innerText);"
    );

    if (!consoleMessageSpy.wait())
        return QString();

    QList<QVariant> arguments = consoleMessageSpy.takeFirst();
    if (static_cast<QQuickWebEngineView::JavaScriptConsoleMessageLevel>(arguments.at(0).toInt()) != QQuickWebEngineView::InfoMessageLevel)
        return QString();

    return arguments.at(1).toString();
}

#endif /* UTIL_H */
