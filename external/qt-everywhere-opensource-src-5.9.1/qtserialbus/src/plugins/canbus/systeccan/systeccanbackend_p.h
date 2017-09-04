/****************************************************************************
**
** Copyright (C) 2017 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
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

#ifndef SYSTECCANBACKEND_P_H
#define SYSTECCANBACKEND_P_H

#include "systeccanbackend.h"
#include "systeccan_symbols_p.h"

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class QEvent;
class QSocketNotifier;
class QWinEventNotifier;
class QTimer;

class IncomingEventHandler : public QObject
{
    // no Q_OBJECT macro!
public:
    explicit IncomingEventHandler(SystecCanBackendPrivate *systecPrivate, QObject *parent) :
        QObject(parent),
        dptr(systecPrivate) { }

    void customEvent(QEvent *event);

private:
       SystecCanBackendPrivate *dptr;
};

class SystecCanBackendPrivate
{
    Q_DECLARE_PUBLIC(SystecCanBackend)

public:
    SystecCanBackendPrivate(SystecCanBackend *q);

    bool open();
    void close();
    void eventHandler(QEvent *event);
    bool setConfigurationParameter(int key, const QVariant &value);
    bool setupChannel(const QString &interfaceName);
    void setupDefaultConfigurations();
    QString systemErrorString(int errorCode);
    void enableWriteNotification(bool enable);
    void startWrite();
    void readAllReceivedMessages();
    bool verifyBitRate(int bitrate);

    SystecCanBackend * const q_ptr;

    tUcanHandle handle = 0;
    quint8 device = 255;
    quint8 channel = 255;

    QTimer *outgoingEventNotifier = nullptr;
    IncomingEventHandler *incomingEventHandler = nullptr;
};

QT_END_NAMESPACE

#endif // SYSTECCANBACKEND_P_H
