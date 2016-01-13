/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qandroidfunctions.h"
#include "qandroidactivityresultreceiver.h"
#include "qandroidactivityresultreceiver_p.h"

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

    Returns a handle to this applications main Activity

    \sa QAndroidJniObject
*/
QAndroidJniObject QtAndroid::androidActivity()
{
    return QtAndroidPrivate::activity();
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

QT_END_NAMESPACE
