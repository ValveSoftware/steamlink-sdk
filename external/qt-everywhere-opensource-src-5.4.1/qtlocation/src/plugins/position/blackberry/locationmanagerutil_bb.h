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

#ifndef LOCATIONMANAGERUTIL_BB_H
#define LOCATIONMANAGERUTIL_BB_H

#include <QtCore/QVariantMap>

namespace bb
{
class PpsObject;
}

namespace global {

// the libQtLocation id for the server-mode accessed pps file id field
extern const QString libQtLocationId;

// the path to the location manager pps file that is the gateway for positioning requests/replies
extern const QString locationManagerPpsFile;

// the minimum interval (in msec) that positional and satellite updates can be provided for
extern const int minUpdateInterval;

// a QVariantMap suitable for writing to a PpsObject specifying a cancel request to the Location
// Manager. This cancels the current request
extern const QVariantMap cancelRequest;

} // namespace global

// send a generic server-mode request, wrapped in a @control map, to ppsObject
bool sendRequest(bb::PpsObject &ppsObject, const QVariantMap &request);

// receive a generic server-mode reply from ppsObject, removing the @control map container
bool receiveReply(QVariantMap *reply, bb::PpsObject &ppsObject);

#endif
