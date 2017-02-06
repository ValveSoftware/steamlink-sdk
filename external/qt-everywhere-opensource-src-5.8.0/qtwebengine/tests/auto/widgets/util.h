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

// Functions and macros that really need to be in QTestLib

#if 0
#pragma qt_no_master_include
#endif

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>
#include <qwebenginepage.h>

#if !defined(TESTS_SOURCE_DIR)
#define TESTS_SOURCE_DIR ""
#endif

/**
 * Starts an event loop that runs until the given signal is received.
 * Optionally the event loop
 * can return earlier on a timeout.
 *
 * \return \p true if the requested signal was received
 *         \p false on timeout
 */
static inline bool waitForSignal(QObject* obj, const char* signal, int timeout = 10000)
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

/**
 * Just like QSignalSpy but facilitates sync and async
 * signal emission. For example if you want to verify that
 * page->foo() emitted a signal, it could be that the
 * implementation decides to emit the signal asynchronously
 * - in which case we want to spin a local event loop until
 * emission - or that the call to foo() emits it right away.
 */
class SignalBarrier : private QSignalSpy
{
public:
    SignalBarrier(const QObject* obj, const char* aSignal)
        : QSignalSpy(obj, aSignal)
    { }

    bool ensureSignalEmitted()
    {
        bool result = count() > 0;
        if (!result)
            result = wait();
        clear();
        return result;
    }
};

template<typename T, typename R>
struct RefWrapper {
    R &ref;
    void operator()(const T& result) {
        ref(result);
    }
};

template<typename T>
class CallbackSpy {
public:
    CallbackSpy() : called(false) {
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    }

    T waitForResult() {
        if (!called) {
            timeoutTimer.start(10000);
            eventLoop.exec();
        }
        return result;
    }

    bool wasCalled() const {
        return called;
    }

    void operator()(const T &result) {
        this->result = result;
        called = true;
        eventLoop.quit();
    }

    // Cheap rip-off of boost/std::ref
    RefWrapper<T, CallbackSpy<T> > ref()
    {
        RefWrapper<T, CallbackSpy<T> > wrapper = {*this};
        return wrapper;
    }

private:
    Q_DISABLE_COPY(CallbackSpy)
    bool called;
    QTimer timeoutTimer;
    QEventLoop eventLoop;
    T result;
};

static inline QString toPlainTextSync(QWebEnginePage *page)
{
    CallbackSpy<QString> spy;
    page->toPlainText(spy.ref());
    return spy.waitForResult();
}

static inline QString toHtmlSync(QWebEnginePage *page)
{
    CallbackSpy<QString> spy;
    page->toHtml(spy.ref());
    return spy.waitForResult();
}

static inline bool findTextSync(QWebEnginePage *page, const QString &subString)
{
    CallbackSpy<bool> spy;
    page->findText(subString, 0, spy.ref());
    return spy.waitForResult();
}

static inline QVariant evaluateJavaScriptSync(QWebEnginePage *page, const QString &script)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript(script, spy.ref());
    return spy.waitForResult();
}

static inline QVariant evaluateJavaScriptSyncInWorld(QWebEnginePage *page, const QString &script, int worldId)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript(script, worldId, spy.ref());
    return spy.waitForResult();
}

static inline QUrl baseUrlSync(QWebEnginePage *page)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript("document.baseURI", spy.ref());
    return spy.waitForResult().toUrl();
}

#define W_QSKIP(a, b) QSKIP(a)

#define W_QTEST_MAIN(TestObject, params) \
QT_BEGIN_NAMESPACE \
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS \
QT_END_NAMESPACE \
int main(int argc, char *argv[]) \
{ \
    QVector<const char *> w_argv(argc); \
    for (int i = 0; i < argc; ++i) \
        w_argv[i] = argv[i]; \
    for (int i = 0; i < params.size(); ++i) \
        w_argv.append(params[i].data()); \
    int w_argc = w_argv.size(); \
    \
    QApplication app(w_argc, const_cast<char **>(w_argv.data())); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    QTEST_ADD_GPU_BLACKLIST_SUPPORT \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
