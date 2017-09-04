/****************************************************************************
**
** Copyright (C) 2017-2015 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef QCONNECTIONFACTORIES_H
#define QCONNECTIONFACTORIES_H

#include <QAbstractSocket>
#include <QDataStream>
#include <QtRemoteObjects/qtremoteobjectglobal.h>

QT_BEGIN_NAMESPACE

//The Qt servers create QIODevice derived classes from handleConnection.
//The problem is that they behave differently, so this class adds some
//consistency.
class Q_REMOTEOBJECTS_EXPORT ServerIoDevice : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ServerIoDevice)

public:
    explicit ServerIoDevice(QObject *parent = nullptr);
    virtual ~ServerIoDevice();

    bool read(QtRemoteObjects::QRemoteObjectPacketTypeEnum &, QString &);

    virtual void write(const QByteArray &data);
    virtual void write(const QByteArray &data, qint64);
    void close();
    virtual qint64 bytesAvailable();
    virtual QIODevice *connection() const = 0;
    void initializeDataStream();
    QDataStream& stream() { return m_dataStream; }

Q_SIGNALS:
    void disconnected();
    void readyRead();

protected:
    virtual void doClose() = 0;

private:
    bool m_isClosing;
    quint32 m_curReadSize;
    QDataStream m_dataStream;
};

class Q_REMOTEOBJECTS_EXPORT QConnectionAbstractServer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QConnectionAbstractServer)

public:
    explicit QConnectionAbstractServer(QObject *parent = nullptr);
    virtual ~QConnectionAbstractServer();

    virtual bool hasPendingConnections() const = 0;
    ServerIoDevice* nextPendingConnection();
    virtual QUrl address() const = 0;
    virtual bool listen(const QUrl &address) = 0;
    virtual QAbstractSocket::SocketError serverError() const = 0;
    virtual void close() = 0;

protected:
    virtual ServerIoDevice* configureNewConnection() = 0;

Q_SIGNALS:
    void newConnection();
};

class Q_REMOTEOBJECTS_EXPORT ClientIoDevice : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ClientIoDevice)

public:
    explicit ClientIoDevice(QObject *parent = nullptr);
    virtual ~ClientIoDevice();

    bool read(QtRemoteObjects::QRemoteObjectPacketTypeEnum &, QString &);

    virtual void write(const QByteArray &data);
    virtual void write(const QByteArray &data, qint64);
    void close();
    virtual void connectToServer() = 0;
    virtual qint64 bytesAvailable();

    QUrl url() const;
    void addSource(const QString &);
    void removeSource(const QString &);
    QSet<QString> remoteObjects() const;

    virtual bool isOpen() = 0;
    virtual QIODevice *connection() = 0;
    inline QDataStream& stream() { return m_dataStream; }

Q_SIGNALS:
    void disconnected();
    void readyRead();
    void shouldReconnect(ClientIoDevice*);
protected:
    virtual void doClose() = 0;
    inline bool isClosing() { return m_isClosing; }
    QDataStream m_dataStream;

private:
    bool m_isClosing;
    QUrl m_url;

private:
    friend class QtROClientFactory;

    quint32 m_curReadSize;
    QSet<QString> m_remoteObjects;
};

class QtROServerFactory
{
public:
    Q_REMOTEOBJECTS_EXPORT static QtROServerFactory *instance();

    QConnectionAbstractServer *create(const QUrl &url, QObject *parent = nullptr)
    {
        auto creatorFunc = m_creatorFuncs.value(url.scheme());
        return creatorFunc ? (*creatorFunc)(parent) : nullptr;
    }

    template<typename T>
    void registerType(const QString &id)
    {
        m_creatorFuncs[id] = [](QObject *parent) -> QConnectionAbstractServer * {
            return new T(parent);
        };
    }

private:
    friend class QtROFactoryLoader;
    QtROServerFactory();

    using CreatorFunc = QConnectionAbstractServer * (*)(QObject *);
    QHash<QString, CreatorFunc> m_creatorFuncs;
};

class QtROClientFactory
{
public:
    Q_REMOTEOBJECTS_EXPORT static QtROClientFactory *instance();

    /// creates an object from a string
    ClientIoDevice *create(const QUrl &url, QObject *parent = nullptr)
    {
        auto creatorFunc = m_creatorFuncs.value(url.scheme());
        if (!creatorFunc)
            return nullptr;

        ClientIoDevice *res = (*creatorFunc)(parent);
        if (res)
            res->m_url = url;
        return res;
    }

    template<typename T>
    void registerType(const QString &id)
    {
        m_creatorFuncs[id] = [](QObject *parent) -> ClientIoDevice * {
            return new T(parent);
        };
    }

private:
    friend class QtROFactoryLoader;
    QtROClientFactory();

    using CreatorFunc = ClientIoDevice * (*)(QObject *);
    QHash<QString, CreatorFunc> m_creatorFuncs;
};

template <typename T>
inline void qRegisterRemoteObjectsClient(const QString &id)
{
    QtROClientFactory::instance()->registerType<T>(id);
}

template <typename T>
inline void qRegisterRemoteObjectsServer(const QString &id)
{
    QtROServerFactory::instance()->registerType<T>(id);
}

QT_END_NAMESPACE

#endif // QCONNECTIONFACTORIES_H
