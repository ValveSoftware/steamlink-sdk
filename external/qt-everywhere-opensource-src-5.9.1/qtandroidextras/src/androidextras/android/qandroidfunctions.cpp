/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qandroidfunctions.h"
#include "qandroidactivityresultreceiver.h"
#include "qandroidactivityresultreceiver_p.h"

#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>

QT_BEGIN_NAMESPACE

/*!
    \namespace QtAndroid
    \inmodule QtAndroidExtras
    \since 5.3
    \brief The QtAndroid namespace provides miscellaneous functions to aid Android development.
    \inheaderfile QtAndroid
*/

/*!
    \since 5.3
    \fn QAndroidJniObject QtAndroid::androidActivity()

    Returns a handle to this application's main Activity

    \sa QAndroidJniObject, androidService(), androidContext()
*/
QAndroidJniObject QtAndroid::androidActivity()
{
    return QtAndroidPrivate::activity();
}

/*!
    \since 5.7
    \fn QAndroidJniObject QtAndroid::androidService()

    Returns a handle to this application's main Service

    \sa QAndroidJniObject, androidActivity(), androidContext()
*/
QAndroidJniObject QtAndroid::androidService()
{
    return QtAndroidPrivate::service();
}

/*!
    \since 5.8
    \fn QAndroidJniObject QtAndroid::androidContext()

    Returns a handle to this application's main Context. Depending on the nature of
    the application the Context object is either the main Service or Activity
    object.

    \sa QAndroidJniObject, androidActivity(), androidService()
*/
QAndroidJniObject QtAndroid::androidContext()
{
    return QtAndroidPrivate::context();
}

/*!
    \since 5.3
    \fn int QtAndroid::androidSdkVersion()

    Returns the Android SDK version. This is also known as the API level.
*/
int QtAndroid::androidSdkVersion()
{
    return QtAndroidPrivate::androidSdkVersion();
}

/*!
  \since 5.3

  Starts the activity given by \a intent and provides the result asynchronously through the
  \a resultReceiver if this is non-null.

  If \a resultReceiver is null, then the \c startActivity() method in the \c androidActivity()
  will be called. Otherwise \c startActivityForResult() will be called.

  The \a receiverRequestCode is a request code unique to the \a resultReceiver, and will be
  returned along with the result, making it possible to use the same receiver for more than
  one intent.

 */
void QtAndroid::startActivity(const QAndroidJniObject &intent,
                              int receiverRequestCode,
                              QAndroidActivityResultReceiver *resultReceiver)
{
    QAndroidJniObject activity = androidActivity();
    if (resultReceiver != 0) {
        QAndroidActivityResultReceiverPrivate *resultReceiverD = QAndroidActivityResultReceiverPrivate::get(resultReceiver);
        activity.callMethod<void>("startActivityForResult",
                                  "(Landroid/content/Intent;I)V",
                                  intent.object<jobject>(),
                                  resultReceiverD->globalRequestCode(receiverRequestCode));
    } else {
        activity.callMethod<void>("startActivity",
                                  "(Landroid/content/Intent;)V",
                                  intent.object<jobject>());
    }
}

/*!
  \since 5.3

  Starts the activity given by \a intentSender and provides the result asynchronously through the
  \a resultReceiver if this is non-null.

  If \a resultReceiver is null, then the \c startIntentSender() method in the \c androidActivity()
  will be called. Otherwise \c startIntentSenderForResult() will be called.

  The \a receiverRequestCode is a request code unique to the \a resultReceiver, and will be
  returned along with the result, making it possible to use the same receiver for more than
  one intent.

*/
void QtAndroid::startIntentSender(const QAndroidJniObject &intentSender,
                                  int receiverRequestCode,
                                  QAndroidActivityResultReceiver *resultReceiver)
{
    QAndroidJniObject activity = androidActivity();
    if (resultReceiver != 0) {
        QAndroidActivityResultReceiverPrivate *resultReceiverD = QAndroidActivityResultReceiverPrivate::get(resultReceiver);
        activity.callMethod<void>("startIntentSenderForResult",
                                  "(Landroid/content/IntentSender;ILandroid/content/Intent;III)V",
                                  intentSender.object<jobject>(),
                                  resultReceiverD->globalRequestCode(receiverRequestCode),
                                  0,  // fillInIntent
                                  0,  // flagsMask
                                  0,  // flagsValues
                                  0); // extraFlags
    } else {
        activity.callMethod<void>("startIntentSender",
                                  "(Landroid/content/Intent;Landroid/content/Intent;III)V",
                                  intentSender.object<jobject>(),
                                  0,  // fillInIntent
                                  0,  // flagsMask
                                  0,  // flagsValues
                                  0); // extraFlags

    }

}

/*!
  \since 5.7
  \fn void QtAndroid::runOnAndroidThread(const Runnable &runnable)

  Posts the given \a runnable on the android thread.
  The \a runnable will be queued and executed on the Android UI thread, unless it called on the
  Android UI thread, in which case the runnable will be executed immediately.

  This function is useful to set asynchronously properties of objects that must be set on on Android UI thread.
*/
void QtAndroid::runOnAndroidThread(const QtAndroid::Runnable &runnable)
{
    QtAndroidPrivate::runOnAndroidThread(runnable, QJNIEnvironmentPrivate());
}

/*!
  \since 5.7
  \fn void QtAndroid::runOnAndroidThreadSync(const Runnable &runnable, int timeoutMs)

  Posts the \a runnable on the Android UI thread and waits until the runnable is executed,
  or until \a timeoutMs has passed

  This function is useful to create objects, or get properties on Android UI thread:

  \code
    QAndroidJniObject javaControl;
    QtAndroid::runOnAndroidThreadSync([&javaControl](){

        // create our Java control on Android UI thread.
        javaControl = QAndroidJniObject("android/webkit/WebView",
                                                    "(Landroid/content/Context;)V",
                                                    QtAndroid::androidActivity().object<jobject>());
        javaControl.callMethod<void>("setWebViewClient",
                                       "(Landroid/webkit/WebViewClient;)V",
                                       QAndroidJniObject("android/webkit/WebViewClient").object());
    });

    // Continue the execution normally
    qDebug() << javaControl.isValid();
  \endcode
*/
void QtAndroid::runOnAndroidThreadSync(const QtAndroid::Runnable &runnable, int timeoutMs)
{
    QtAndroidPrivate::runOnAndroidThreadSync(runnable, QJNIEnvironmentPrivate(), timeoutMs);
}


/*!
  \since 5.7
  \fn void QtAndroid::hideSplashScreen()

  Hides the splash screen.
*/
void QtAndroid::hideSplashScreen()
{
    QtAndroidPrivate::hideSplashScreen(QJNIEnvironmentPrivate());
}

QT_END_NAMESPACE
