/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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

/**
 * Starts an event loop that runs until the given signal is received.
 * Optionally the event loop
 * can return earlier on a timeout.
 *
 * \return \p true if the requested signal was received
 *         \p false on timeout
 */
inline bool waitForSignal(QObject *obj, const char *signal, int timeout = 10000)
{
    QEventLoop loop;
    QObject::connect(obj, signal, &loop, SLOT(quit()));
    QTimer timer;
    QSignalSpy timeoutSpy(&timer, SIGNAL(timeout()));
    if (timeout > 0) {
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
    }
    loop.exec();
    return timeoutSpy.isEmpty();
}

inline bool waitForLoadSucceeded(QQuickWebEngineView *webEngineView, int timeout = 10000)
{
    LoadSpy loadSpy(webEngineView);
    return waitForSignal(&loadSpy, SIGNAL(loadSucceeded()), timeout);
}

inline bool waitForLoadFailed(QQuickWebEngineView *webEngineView, int timeout = 10000)
{
    LoadSpy loadSpy(webEngineView);
    return waitForSignal(&loadSpy, SIGNAL(loadFailed()), timeout);
}

inline bool waitForViewportReady(QQuickWebEngineView *webEngineView, int timeout = 10000)
{
    return waitForSignal(reinterpret_cast<QObject *>(webEngineView->experimental()), SIGNAL(loadVisuallyCommitted()), timeout);
}

#endif /* UTIL_H */
