/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#ifndef QDECLARATIVENEARFIELDSOCKET_P_H
#define QDECLARATIVENEARFIELDSOCKET_P_H

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

#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QtQml/QQmlParserStatus>
#include <QtNfc/QLlcpSocket>

QT_USE_NAMESPACE

class QDeclarativeNearFieldSocketPrivate;

class QDeclarativeNearFieldSocket : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY uriChanged)
    Q_PROPERTY(bool connected READ connected WRITE setConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool listening READ listening WRITE setListening NOTIFY listeningChanged)
    Q_PROPERTY(QString stringData READ stringData WRITE sendStringData NOTIFY dataAvailable)

    Q_INTERFACES(QQmlParserStatus)

    Q_DECLARE_PRIVATE(QDeclarativeNearFieldSocket)

public:
    explicit QDeclarativeNearFieldSocket(QObject *parent = 0);
    ~QDeclarativeNearFieldSocket();

    QString uri() const;
    bool connected() const;
    QString error() const;
    QString state() const;
    bool listening() const;

    QString stringData();

    // From QDeclarativeParserStatus
    void classBegin() {}
    void componentComplete();

signals:
    void uriChanged();
    void connectedChanged();
    void errorChanged();
    void stateChanged();
    void listeningChanged();
    void dataAvailable();

public slots:
    void setUri(const QString &service);
    void setConnected(bool connected);
    void setListening(bool listen);
    void sendStringData(const QString &data);

private slots:
    void socket_connected();
    void socket_disconnected();
    void socket_error(QLlcpSocket::SocketError);
    void socket_state(QLlcpSocket::SocketState);
    void socket_readyRead();

    void llcp_connection();

private:
    QDeclarativeNearFieldSocketPrivate *d_ptr;
};

QML_DECLARE_TYPE(QDeclarativeNearFieldSocket)

#endif // QDECLARATIVENEARFIELDSOCKET_P_H
