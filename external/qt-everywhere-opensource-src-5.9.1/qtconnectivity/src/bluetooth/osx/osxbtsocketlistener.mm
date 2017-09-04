/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "osxbtsocketlistener_p.h"
#include "osxbtutility_p.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth {

SocketListener::~SocketListener()
{
}

}

QT_END_NAMESPACE

QT_USE_NAMESPACE

@implementation QT_MANGLE_NAMESPACE(OSXBTSocketListener)

- (id)initWithListener:(OSXBluetooth::SocketListener *)aDelegate
{
    Q_ASSERT_X(aDelegate, Q_FUNC_INFO, "invalid delegate (null)");
    if (self = [super init]) {
        connectionNotification = nil;
        delegate = aDelegate;
        port = 0;
    }

    return self;
}

- (void)dealloc
{
    [connectionNotification unregister];
    [connectionNotification release];

    [super dealloc];
}

- (bool)listenRFCOMMConnectionsWithChannelID:(BluetoothRFCOMMChannelID)channelID
{
    Q_ASSERT_X(!connectionNotification, Q_FUNC_INFO, "already listening");

    connectionNotification = [IOBluetoothRFCOMMChannel  registerForChannelOpenNotifications:self
                                                        selector:@selector(rfcommOpenNotification:channel:)
                                                        withChannelID:channelID
                                                        direction:kIOBluetoothUserNotificationChannelDirectionIncoming];
    connectionNotification = [connectionNotification retain];
    if (connectionNotification)
        port = channelID;

    return connectionNotification;
}

- (bool)listenL2CAPConnectionsWithPSM:(BluetoothL2CAPPSM)psm
{
    Q_ASSERT_X(!connectionNotification, Q_FUNC_INFO, "already listening");

    connectionNotification = [IOBluetoothL2CAPChannel registerForChannelOpenNotifications:self
                                                      selector:@selector(l2capOpenNotification:channel:)
                                                      withPSM:psm
                                                      direction:kIOBluetoothUserNotificationChannelDirectionIncoming];
    connectionNotification = [connectionNotification retain];
    if (connectionNotification)
        port = psm;

    return connectionNotification;
}

- (void)rfcommOpenNotification:(IOBluetoothUserNotification *)notification
        channel:(IOBluetoothRFCOMMChannel *)newChannel
{
    Q_UNUSED(notification)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");
    delegate->openNotify(newChannel);
}

- (void)l2capOpenNotification:(IOBluetoothUserNotification *)notification
        channel:(IOBluetoothL2CAPChannel *)newChannel
{
    Q_UNUSED(notification)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");
    delegate->openNotify(newChannel);
}

- (quint16)port
{
    return port;
}

@end
