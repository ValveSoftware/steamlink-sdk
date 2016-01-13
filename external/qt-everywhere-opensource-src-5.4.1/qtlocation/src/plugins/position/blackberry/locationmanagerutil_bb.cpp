/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "locationmanagerutil_bb.h"

#include <bb/PpsObject>

#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>
#include <QtDebug>

#include <errno.h>

namespace {

// Create a QVariantMap suitable for writing to a PpsObject specifying a cancel request to the
// Location Manager.
QVariantMap createCancelRequest()
{
    QVariantMap map;

    map.insert("msg", "cancel");
    map.insert("id", ::global::libQtLocationId);

    return map;
}

} // unnamed namespace

namespace global {

const QString libQtLocationId = "libQtLocation";
const QString locationManagerPpsFile = "/pps/services/geolocation/control";
const int minUpdateInterval = 1000;
const QVariantMap cancelRequest = createCancelRequest();

} // namespace global


// send a generic server-mode request, wrapped in a @control map, to ppsObject
bool sendRequest(bb::PpsObject &ppsObject, const QVariantMap &request)
{
    if (!ppsObject.isOpen()) {
        if (!ppsObject.open()) {
            qWarning() << "LocationManagerUtil.cpp:sendRequest(): error opening pps object, errno ="
                       << ppsObject.error() << "("
                       << strerror(ppsObject.error())
                       << "). Clients should verify that they have access_location_services "
                          "permission.";
            return false;
        }
    }

    // wrap the request in a @control map
    QVariantMap map;
    map.insert("@control", request);


    // encode it
    bool ok;
    QByteArray encodedRequest = bb::PpsObject::encode(map, &ok);
    if (!ok) {
        qWarning() << "LocationManagerUtil.cpp:sendRequest(): error encoding position request";
        ppsObject.close();
        return false;
    }

    // write it
    bool success = ppsObject.write(encodedRequest);
    if (!success) {
        qWarning() << "LocationManagerUtil.cpp:sendRequest(): error"
                   << ppsObject.error()
                   << "writing position request";
        ppsObject.close();
        return false;
    }

    return true;
}

// receive a generic server-mode reply from ppsObject, removing the @control map container
bool receiveReply(QVariantMap *reply, bb::PpsObject &ppsObject)
{
    if (!ppsObject.isOpen()) {
        if (!ppsObject.open()) {
            qWarning() << "LocationManagerUtil.cpp:receiveReply(): error opening pps object";
            return false;
        }
    }

    // read the reply
    bool ok;
    QByteArray encodedReply = ppsObject.read(&ok);
    if (!ok) {
        qWarning() << "LocationManagerUtil.cpp:receiveReply(): error"
                   << ppsObject.error()
                   << "reading position reply";
        ppsObject.close();
        return false;
    }

    // decode the reply
    *reply = bb::PpsObject::decode(encodedReply, &ok);
    if (!ok) {
        qWarning() << "LocationManagerUtil.cpp:receiveReply(): error decoding position reply";
        ppsObject.close();
        return false;
    }

    // peel out the control map from the reply
    *reply = reply->value("@control").toMap();

    // check for an error in the reply
    if (reply->contains("errCode")) {
        int errCode = reply->value("errCode").toInt();
        if (errCode) {
            qWarning() << "LocationManagerUtil.cpp:receiveReply(): (" << errCode << ")" <<
                    reply->value("err").toString().toLocal8Bit().constData() << ":" <<
                    reply->value("errstr").toString().toLocal8Bit().constData();
            return false;
        }
    }

    return true;
}

