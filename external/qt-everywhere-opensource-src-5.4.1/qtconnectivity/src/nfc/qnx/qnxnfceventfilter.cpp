/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnxnfceventfilter_p.h"
#include <QDebug>
#include "nfc/nfc.h"
#include "qnxnfcmanager_p.h"

QT_BEGIN_NAMESPACE

QNXNFCEventFilter::QNXNFCEventFilter()
{
}

void QNXNFCEventFilter::installOnEventDispatcher(QAbstractEventDispatcher *dispatcher)
{
    dispatcher->installNativeEventFilter(this);
    // start flow of navigator events
    navigator_request_events(0);
}

void QNXNFCEventFilter::uninstallEventFilter()
{
    removeEventFilter(this);
}

bool QNXNFCEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    bps_event_t *event = static_cast<bps_event_t *>(message);

    int code = bps_event_get_code(event);

    if (code == NAVIGATOR_INVOKE_TARGET) {
        // extract bps request from event
        const navigator_invoke_invocation_t *invoke = navigator_invoke_event_get_invocation(event);
        const char *uri = navigator_invoke_invocation_get_uri(invoke);
        const char *type = navigator_invoke_invocation_get_type(invoke);
        int dataLength = navigator_invoke_invocation_get_data_length(invoke);
        const char *raw_data = (const char*)navigator_invoke_invocation_get_data(invoke);
        QByteArray data(raw_data, dataLength);

        //message.fromByteArray(data);

        //const char* metadata = navigator_invoke_invocation_get_metadata(invoke);

        nfc_ndef_message_t *ndefMessage;
        nfc_create_ndef_message_from_bytes(reinterpret_cast<const uchar_t *>(data.data()),
                                                  data.length(), &ndefMessage);

        QNdefMessage message = QNXNFCManager::instance()->decodeMessage(ndefMessage);

        unsigned int ndefRecordCount;
        nfc_get_ndef_record_count(ndefMessage, &ndefRecordCount);

        qQNXNFCDebug() << "Got Invoke event" << uri << "Type" << type;

        emit ndefEvent(message);
    }

    return false;
}

QT_END_NAMESPACE
