/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@jollamobile.com
**
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef NETWORKCOUNTER_H
#define NETWORKCOUNTER_H

#include <QObject>
#include <counter.h>

class NetworkCounter : public QObject
{
    Q_OBJECT
public:
    explicit NetworkCounter(QObject *parent = 0);

Q_SIGNALS:

public Q_SLOTS:
    void counterChanged(const QString servicePath, const QVariantMap &counters,  bool roaming);

    void bytesReceivedChanged(quint64 bytesRx);

    void bytesTransmittedChanged(quint64 bytesTx);

    void secondsOnlineChanged(quint64 seconds);

    void roamingChanged(bool roaming);
private:
    Counter *counter;

};

#endif // NETWORKCOUNTER_H
