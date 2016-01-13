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

#include "qandroidactivityresultreceiver.h"
#include "qandroidactivityresultreceiver_p.h"
#include "qandroidfunctions.h"
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qmutex.h>

QT_BEGIN_NAMESPACE

// Get a unique activity request code.
static int uniqueActivityRequestCode()
{
    static QMutex mutex;
    static int requestCode = 0x1000; // Reserve all request codes under 0x1000 for Qt.

    QMutexLocker locker(&mutex);
    if (requestCode == 0xf3ee) // Special case for MINISTRO_INSTALL_REQUEST_CODE
        requestCode++;

    if (requestCode == INT_MAX)
        qWarning("Unique activity request code has wrapped. Unexpected behavior may occur.");

    return requestCode++;
}

int QAndroidActivityResultReceiverPrivate::globalRequestCode(int localRequestCode) const
{
    if (!localToGlobalRequestCode.contains(localRequestCode)) {
        int globalRequestCode = uniqueActivityRequestCode();
        localToGlobalRequestCode[localRequestCode] = globalRequestCode;
        globalToLocalRequestCode[globalRequestCode] = localRequestCode;
    }
    return localToGlobalRequestCode.value(localRequestCode);
}

bool QAndroidActivityResultReceiverPrivate::handleActivityResult(jint requestCode, jint resultCode, jobject data)
{
    if (globalToLocalRequestCode.contains(requestCode)) {
        q->handleActivityResult(globalToLocalRequestCode.value(requestCode), resultCode, QAndroidJniObject(data));
        return true;
    }

    return false;
}

/*!
  \class QAndroidActivityResultReceiver
  \inmodule QtAndroidExtras
  \since 5.3
  \brief Interface used for callbacks from onActivityResult() in the main Android activity.

  Create a subclass of this class to be notified of the results when using the
  \c QtAndroid::startActivity() and \c QtAndroid::startIntentSender() APIs.
 */

/*!
   \internal
*/
QAndroidActivityResultReceiver::QAndroidActivityResultReceiver()
    : d(new QAndroidActivityResultReceiverPrivate)
{
    d->q = this;
    QtAndroidPrivate::registerActivityResultListener(d.data());
}

/*!
   \internal
*/
QAndroidActivityResultReceiver::~QAndroidActivityResultReceiver()
{
    QtAndroidPrivate::unregisterActivityResultListener(d.data());
}

/*!
   \fn void QAndroidActivityResultReceiver::handleActivityResult(int receiverRequestCode, int resultCode, const QAndroidJniObject &data) = 0;

   Reimplement this function to get activity results after starting an activity using either QtAndroid::startActivity() or
   QtAndroid::startIntentSender(). The \a receiverRequestCode is the request code unique to this receiver which was originally
   passed to the startActivity() or startIntentSender() functions. The \a resultCode is the result returned by the activity,
   and \a data is either null or a Java object of the class android.content.Intent. Both the last to arguments are identical to the
   arguments passed to onActivityResult().
*/

QT_END_NAMESPACE
