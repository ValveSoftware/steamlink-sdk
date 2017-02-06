/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebView module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwebview_android_p.h"
#include "qwebview_p.h"
#include "qwebviewloadrequest_p.h" // TODO:
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/private/qjni_p.h>

#include <QtCore/qmap.h>
#include <android/bitmap.h>
#include <QtGui/qguiapplication.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qurl.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QWebViewPrivate *QWebViewPrivate::create(QWebView *q)
{
    return new QAndroidWebViewPrivate(q);
}

static const char qtAndroidWebViewControllerClass[] = "org/qtproject/qt5/android/view/QtAndroidWebViewController";

//static bool favIcon(JNIEnv *env, jobject icon, QImage *image)
//{
//    // TODO:
//    AndroidBitmapInfo bitmapInfo;
//    if (AndroidBitmap_getInfo(env, icon, &bitmapInfo) != ANDROID_BITMAP_RESULT_SUCCESS)
//        return false;

//    void *pixelData;
//    if (AndroidBitmap_lockPixels(env, icon, &pixelData) != ANDROID_BITMAP_RESULT_SUCCESS)
//        return false;

//    *image = QImage::fromData(static_cast<const uchar *>(pixelData), bitmapInfo.width * bitmapInfo.height);
//    AndroidBitmap_unlockPixels(env, icon);

//    return true;
//}

typedef QMap<quintptr, QAndroidWebViewPrivate *> WebViews;
Q_GLOBAL_STATIC(WebViews, g_webViews)

QAndroidWebViewPrivate::QAndroidWebViewPrivate(QObject *p)
    : QWebViewPrivate(p)
    , m_id(reinterpret_cast<quintptr>(this))
    , m_callbackId(0)
    , m_window(0)
{
    m_viewController = QJNIObjectPrivate(qtAndroidWebViewControllerClass,
                                         "(Landroid/app/Activity;J)V",
                                         QtAndroidPrivate::activity(),
                                         m_id);
    m_webView = m_viewController.callObjectMethod("getWebView",
                                                  "()Landroid/webkit/WebView;");

    m_window = QWindow::fromWinId(reinterpret_cast<WId>(m_webView.object()));
    g_webViews->insert(m_id, this);
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, &QAndroidWebViewPrivate::onApplicationStateChanged);
}

QAndroidWebViewPrivate::~QAndroidWebViewPrivate()
{
    g_webViews->take(m_id);
    if (m_window != 0) {
        m_window->setVisible(false);
        m_window->setParent(0);
        delete m_window;
    }

    m_viewController.callMethod<void>("destroy");
}

QUrl QAndroidWebViewPrivate::url() const
{
    return QUrl::fromUserInput(m_viewController.callObjectMethod<jstring>("getUrl").toString());
}

void QAndroidWebViewPrivate::setUrl(const QUrl &url)
{
    m_viewController.callMethod<void>("loadUrl",
                                      "(Ljava/lang/String;)V",
                                      QJNIObjectPrivate::fromString(url.toString()).object());
}

void QAndroidWebViewPrivate::loadHtml(const QString &html, const QUrl &baseUrl)
{
    const QJNIObjectPrivate &htmlString = QJNIObjectPrivate::fromString(html);
    const QJNIObjectPrivate &mimeTypeString = QJNIObjectPrivate::fromString(QLatin1String("text/html;charset=UTF-8"));

    baseUrl.isEmpty() ? m_viewController.callMethod<void>("loadData",
                                                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                                          htmlString.object(),
                                                          mimeTypeString.object(),
                                                          0)

                      : m_viewController.callMethod<void>("loadDataWithBaseURL",
                                                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                                          QJNIObjectPrivate::fromString(baseUrl.toString()).object(),
                                                          htmlString.object(),
                                                          mimeTypeString.object(),
                                                          0,
                                                          0);
}

bool QAndroidWebViewPrivate::canGoBack() const
{
    return m_viewController.callMethod<jboolean>("canGoBack");
}

void QAndroidWebViewPrivate::goBack()
{
    m_viewController.callMethod<void>("goBack");
}

bool QAndroidWebViewPrivate::canGoForward() const
{
    return m_viewController.callMethod<jboolean>("canGoForward");
}

void QAndroidWebViewPrivate::goForward()
{
    m_viewController.callMethod<void>("goForward");
}

void QAndroidWebViewPrivate::reload()
{
    m_viewController.callMethod<void>("reload");
}

QString QAndroidWebViewPrivate::title() const
{
    return m_viewController.callObjectMethod<jstring>("getTitle").toString();
}

void QAndroidWebViewPrivate::setGeometry(const QRect &geometry)
{
    if (m_window == 0)
        return;

    m_window->setGeometry(geometry);
}

void QAndroidWebViewPrivate::setVisibility(QWindow::Visibility visibility)
{
    m_window->setVisibility(visibility);
}

void QAndroidWebViewPrivate::runJavaScriptPrivate(const QString &script,
                                                  int callbackId)
{
    if (QtAndroidPrivate::androidSdkVersion() < 19) {
        qWarning() << "runJavaScript() requires API level 19 or higher.";
        if (callbackId == -1)
            return;

        // Emit signal here to remove the callback.
        Q_EMIT javaScriptResult(callbackId, QVariant());
    }

    m_viewController.callMethod<void>("runJavaScript",
                                      "(Ljava/lang/String;J)V",
                                      static_cast<jstring>(QJNIObjectPrivate::fromString(script).object()),
                                      callbackId);
}

void QAndroidWebViewPrivate::setVisible(bool visible)
{
    m_window->setVisible(visible);
}

int QAndroidWebViewPrivate::loadProgress() const
{
    return m_viewController.callMethod<jint>("getProgress");
}

bool QAndroidWebViewPrivate::isLoading() const
{
    return m_viewController.callMethod<jboolean>("isLoading");
}

void QAndroidWebViewPrivate::setParentView(QObject *view)
{
    m_window->setParent(qobject_cast<QWindow *>(view));
}

QObject *QAndroidWebViewPrivate::parentView() const
{
    return m_window->parent();
}

void QAndroidWebViewPrivate::stop()
{
    m_viewController.callMethod<void>("stopLoading");
}

//void QAndroidWebViewPrivate::initialize()
//{
//    // TODO:
//}

void QAndroidWebViewPrivate::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (QtAndroidPrivate::androidSdkVersion() < 11)
        return;

    if (state == Qt::ApplicationActive)
        m_viewController.callMethod<void>("onResume");
    else
        m_viewController.callMethod<void>("onPause");
}

QT_END_NAMESPACE

static void c_onRunJavaScriptResult(JNIEnv *env,
                                    jobject thiz,
                                    jlong id,
                                    jlong callbackId,
                                    jstring result)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)

    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = static_cast<QAndroidWebViewPrivate *>(wv[id]);
    if (!wc)
        return;

    const QString &resultString = QJNIObjectPrivate(result).toString();

    // The result string is in JSON format, lets parse it to see what we got.
    QJsonValue jsonValue;
    const QByteArray &jsonData = "{ \"data\": " + resultString.toUtf8() + " }";
    QJsonParseError error;
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error == QJsonParseError::NoError && jsonDoc.isObject()) {
        const QJsonObject &object = jsonDoc.object();
        jsonValue = object.value(QStringLiteral("data"));
    }

    Q_EMIT wc->javaScriptResult(int(callbackId),
                                jsonValue.isNull() ? resultString
                                                   : jsonValue.toVariant());
}

static void c_onPageFinished(JNIEnv *env,
                             jobject thiz,
                             jlong id,
                             jstring url)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;

    QWebViewLoadRequestPrivate loadRequest(QUrl(QJNIObjectPrivate(url).toString()),
                                           QWebView::LoadSucceededStatus,
                                           QString());
    Q_EMIT wc->loadingChanged(loadRequest);
}

static void c_onPageStarted(JNIEnv *env,
                            jobject thiz,
                            jlong id,
                            jstring url,
                            jobject icon)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    Q_UNUSED(icon)
    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;
    QWebViewLoadRequestPrivate loadRequest(QUrl(QJNIObjectPrivate(url).toString()),
                                           QWebView::LoadStartedStatus,
                                           QString());
    Q_EMIT wc->loadingChanged(loadRequest);

//    if (!icon)
//        return;

//    QImage image;
//    if (favIcon(env, icon, &image))
//        Q_EMIT wc->iconChanged(image);
}

static void c_onProgressChanged(JNIEnv *env,
                                jobject thiz,
                                jlong id,
                                jint newProgress)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;

    Q_EMIT wc->loadProgressChanged(newProgress);
}

static void c_onReceivedIcon(JNIEnv *env,
                             jobject thiz,
                             jlong id,
                             jobject icon)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    Q_UNUSED(id)
    Q_UNUSED(icon)

    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;

    if (!icon)
        return;

//    QImage image;
//    if (favIcon(env, icon, &image))
//        Q_EMIT wc->iconChanged(image);
}

static void c_onReceivedTitle(JNIEnv *env,
                              jobject thiz,
                              jlong id,
                              jstring title)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;

    const QString &qTitle = QJNIObjectPrivate(title).toString();
    Q_EMIT wc->titleChanged(qTitle);
}

static void c_onReceivedError(JNIEnv *env,
                              jobject thiz,
                              jlong id,
                              jint errorCode,
                              jstring description,
                              jstring url)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    Q_UNUSED(errorCode)

    const WebViews &wv = (*g_webViews);
    QAndroidWebViewPrivate *wc = wv[id];
    if (!wc)
        return;
    QWebViewLoadRequestPrivate loadRequest(QUrl(QJNIObjectPrivate(url).toString()),
                                           QWebView::LoadFailedStatus,
                                           QJNIObjectPrivate(description).toString());
    Q_EMIT wc->loadingChanged(loadRequest);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    typedef union {
        JNIEnv *nativeEnvironment;
        void *venv;
    } UnionJNIEnvToVoid;

    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
        return JNI_ERR;

    JNIEnv *env = uenv.nativeEnvironment;

    jclass clazz = QJNIEnvironmentPrivate::findClass(qtAndroidWebViewControllerClass, env);
    if (!clazz)
        return JNI_ERR;

    JNINativeMethod methods[] = {
        {"c_onPageFinished", "(JLjava/lang/String;)V", reinterpret_cast<void *>(c_onPageFinished)},
        {"c_onPageStarted", "(JLjava/lang/String;Landroid/graphics/Bitmap;)V", reinterpret_cast<void *>(c_onPageStarted)},
        {"c_onProgressChanged", "(JI)V", reinterpret_cast<void *>(c_onProgressChanged)},
        {"c_onReceivedIcon", "(JLandroid/graphics/Bitmap;)V", reinterpret_cast<void *>(c_onReceivedIcon)},
        {"c_onReceivedTitle", "(JLjava/lang/String;)V", reinterpret_cast<void *>(c_onReceivedTitle)},
        {"c_onRunJavaScriptResult", "(JJLjava/lang/String;)V", reinterpret_cast<void *>(c_onRunJavaScriptResult)},
        {"c_onReceivedError", "(JILjava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void *>(c_onReceivedError)}
    };

    const int nMethods = sizeof(methods) / sizeof(methods[0]);

    if (env->RegisterNatives(clazz, methods, nMethods) != JNI_OK)
        return JNI_ERR;

    return JNI_VERSION_1_4;
}
