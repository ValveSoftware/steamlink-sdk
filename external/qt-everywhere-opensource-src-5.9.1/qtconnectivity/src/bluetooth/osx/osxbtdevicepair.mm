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

#include "osxbtdevicepair_p.h"
#include "osxbtutility_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth {

ObjCStrongReference<IOBluetoothDevice> device_with_address(const QBluetoothAddress &address)
{
    if (address.isNull())
        return ObjCStrongReference<IOBluetoothDevice>(nil, false);

    const BluetoothDeviceAddress &iobtAddress = iobluetooth_address(address);
    ObjCStrongReference<IOBluetoothDevice> res([[IOBluetoothDevice deviceWithAddress:&iobtAddress] retain], false);
    return res;
}

PairingDelegate::~PairingDelegate()
{
}

}


QT_END_NAMESPACE

QT_USE_NAMESPACE

@implementation QT_MANGLE_NAMESPACE(OSXBTPairing)

- (id)initWithTarget:(const QBluetoothAddress &)address
      delegate:(OSXBluetooth::PairingDelegate *)object
{
    if (self = [super init]) {
        Q_ASSERT_X(!address.isNull(), Q_FUNC_INFO, "invalid target address");
        Q_ASSERT_X(object, Q_FUNC_INFO, "invalid delegate (null)");

        m_targetAddress = address;
        m_object = object;
        m_active = false;
    }

    return self;
}

- (void)dealloc
{
    [m_pairing stop];
    [m_pairing release];
    [super dealloc];
}

- (IOReturn) start
{
    if (m_active)
        return kIOReturnBusy;

    Q_ASSERT_X(!m_targetAddress.isNull(), Q_FUNC_INFO, "invalid target address");

    QT_BT_MAC_AUTORELEASEPOOL;

    const BluetoothDeviceAddress &iobtAddress = OSXBluetooth::iobluetooth_address(m_targetAddress);
    // Device is autoreleased.
    IOBluetoothDevice *const device = [IOBluetoothDevice deviceWithAddress:&iobtAddress];
    if (!device) {
        qCCritical(QT_BT_OSX) << "failed to create a device to pair with";
        return kIOReturnError;
    }

    m_pairing = [[IOBluetoothDevicePair pairWithDevice:device] retain];
    if (!m_pairing) {
        qCCritical(QT_BT_OSX) << "failed to create pair";
        return kIOReturnError;
    }

    [m_pairing setDelegate:self];
    const IOReturn result = [m_pairing start];
    if (result != kIOReturnSuccess) {
        [m_pairing release];
        m_pairing = nil;
    } else
        m_active = true;

    return result;
}

- (bool)isActive
{
    return m_active;
}

- (void)stop
{
    // stop: stops pairing, removes the delegate
    // and disconnects if device was connected.
    if (m_pairing)
        [m_pairing stop];
}

- (const QBluetoothAddress &)targetAddress
{
    return m_targetAddress;
}

- (IOBluetoothDevicePair *)pairingRequest
{
    return [[m_pairing retain] autorelease];
}

- (IOBluetoothDevice *)targetDevice
{
    return [m_pairing device];//It's retained/autoreleased by pair.
}

// IOBluetoothDevicePairDelegate:

- (void)devicePairingStarted:(id)sender
{
    Q_UNUSED(sender)
}

- (void)devicePairingConnecting:(id)sender
{
    Q_UNUSED(sender)
}

- (void)deviceParingPINCodeRequest:(id)sender
{
    Q_UNUSED(sender)
}

- (void)devicePairingUserConfirmationRequest:(id)sender
        numericValue:(BluetoothNumericValue)numericValue
{
    if (sender != m_pairing) // Can never happen.
        return;

    Q_ASSERT_X(m_object, Q_FUNC_INFO, "invalid delegate (null)");

    m_object->requestUserConfirmation(self, numericValue);
}

- (void)devicePairingUserPasskeyNotification:(id)sender
        passkey:(BluetoothPasskey)passkey
{
    Q_UNUSED(sender)
    Q_UNUSED(passkey)
}

- (void)devicePairingFinished:(id)sender error:(IOReturn)error
{
    Q_ASSERT_X(m_object, Q_FUNC_INFO, "invalid delegate (null)");

    if (sender != m_pairing) // Can never happen though.
        return;

    m_active = false;
    if (error != kIOReturnSuccess)
        m_object->error(self, error);
    else
        m_object->pairingFinished(self);
}

- (void)deviceSimplePairingComplete:(id)sender
        status:(BluetoothHCIEventStatus)status
{
    Q_UNUSED(sender)
    Q_UNUSED(status)
}

@end
