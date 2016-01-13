/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
