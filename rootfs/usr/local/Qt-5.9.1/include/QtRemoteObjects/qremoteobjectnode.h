/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
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

#ifndef QREMOTEOBJECTNODE_H
#define QREMOTEOBJECTNODE_H

#include <QtCore/QSharedPointer>
#include <QtCore/QMetaClassInfo>
#include <QtRemoteObjects/qtremoteobjectglobal.h>
#include <QtRemoteObjects/qremoteobjectregistry.h>
#include <QtRemoteObjects/qremoteobjectdynamicreplica.h>

QT_BEGIN_NAMESPACE

class QRemoteObjectReplica;
class SourceApiMap;
class QAbstractItemModel;
class QAbstractItemModelReplica;
class QItemSelectionModel;
class QRemoteObjectNodePrivate;
class QRemoteObjectHostBasePrivate;
class QRemoteObjectHostPrivate;
class QRemoteObjectRegistryHostPrivate;
class ClientIoDevice;

class Q_REMOTEOBJECTS_EXPORT QRemoteObjectPersistedStore
{
public:
    virtual ~QRemoteObjectPersistedStore() {}
    virtual void saveProperties(const QString &repName, const QByteArray &repSig, const QVariantList &values) = 0;
    virtual QVariantList restoreProperties(const QString &repName, const QByteArray &repSig) = 0;
};

class Q_REMOTEOBJECTS_EXPORT QRemoteObjectNode : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl registryUrl READ registryUrl WRITE setRegistryUrl)

public:
    enum ErrorCode{
        NoError,
        RegistryNotAcquired,
        RegistryAlreadyHosted,
        NodeIsNoServer,
        ServerAlreadyCreated,
        UnintendedRegistryHosting,
        OperationNotValidOnClientNode,
        SourceNotRegistered,
        MissingObjectName,
        HostUrlInvalid
    };
    Q_ENUM(ErrorCode)
    enum StorageOwnership {
        DoNotPassOwnership,
        PassOwnershipToNode
    };

    QRemoteObjectNode(QObject *parent = nullptr);
    QRemoteObjectNode(const QUrl &registryAddress, QObject *parent = nullptr);
    virtual ~QRemoteObjectNode();

    Q_INVOKABLE bool connectToNode(const QUrl &address);
    virtual void setName(const QString &name);
    template < class ObjectType >
    ObjectType *acquire(const QString &name = QString())
    {
        return new ObjectType(this, name);
    }

    template<typename T>
    QStringList instances() const
    {
        const QMetaObject *mobj = &T::staticMetaObject;
        const int index = mobj->indexOfClassInfo(QCLASSINFO_REMOTEOBJECT_TYPE);
        if (index == -1)
            return QStringList();

        const QString typeName = QString::fromLatin1(mobj->classInfo(index).value());
        return instances(typeName);
    }
    QStringList instances(const QString &typeName) const;

    QRemoteObjectDynamicReplica *acquireDynamic(const QString &name);
    QAbstractItemModelReplica *acquireModel(const QString &name);
    QUrl registryUrl() const;
    virtual bool setRegistryUrl(const QUrl &registryAddress);
    bool waitForRegistry(int timeout = 30000);
    const QRemoteObjectRegistry *registry() const;
    void setPersistedStore(QRemoteObjectPersistedStore *store, StorageOwnership ownership=DoNotPassOwnership);

    ErrorCode lastError() const;

    void timerEvent(QTimerEvent*);

Q_SIGNALS:
    void remoteObjectAdded(const QRemoteObjectSourceLocation &);
    void remoteObjectRemoved(const QRemoteObjectSourceLocation &);

    void error(QRemoteObjectNode::ErrorCode errorCode);

protected:
    QRemoteObjectNode(QRemoteObjectNodePrivate &, QObject *parent);

private:
    void initializeReplica(QRemoteObjectReplica *instance, const QString &name = QString());
    void persistProperties(const QString &repName, const QByteArray &repSig, const QVariantList &props);
    QVariantList retrieveProperties(const QString &repName, const QByteArray &repSig);
    Q_DECLARE_PRIVATE(QRemoteObjectNode)
    Q_PRIVATE_SLOT(d_func(), void onClientRead(QObject *obj))
    Q_PRIVATE_SLOT(d_func(), void onRemoteObjectSourceAdded(const QRemoteObjectSourceLocation &entry))
    Q_PRIVATE_SLOT(d_func(), void onRemoteObjectSourceRemoved(const QRemoteObjectSourceLocation &entry))
    Q_PRIVATE_SLOT(d_func(), void onRegistryInitialized())
    Q_PRIVATE_SLOT(d_func(), void onShouldReconnect(ClientIoDevice *ioDevice))
    friend class QRemoteObjectReplica;
};

class Q_REMOTEOBJECTS_EXPORT QRemoteObjectHostBase : public QRemoteObjectNode
{
    Q_OBJECT
public:
    void setName(const QString &name) override;

    template <template <typename> class ApiDefinition, typename ObjectType>
    bool enableRemoting(ObjectType *object)
    {
        ApiDefinition<ObjectType> *api = new ApiDefinition<ObjectType>(object);
        return enableRemoting(object, api);
    }
    bool enableRemoting(QObject *object, const QString &name = QString());
    bool enableRemoting(QAbstractItemModel *model, const QString &name, const QVector<int> roles, QItemSelectionModel *selectionModel = nullptr);
    bool disableRemoting(QObject *remoteObject);

protected:
    virtual QUrl hostUrl() const;
    virtual bool setHostUrl(const QUrl &hostAddress);
    QRemoteObjectHostBase(QRemoteObjectHostBasePrivate &, QObject *);

private:
    bool enableRemoting(QObject *object, const SourceApiMap *, QObject *adapter = nullptr);
    Q_DECLARE_PRIVATE(QRemoteObjectHostBase)
};

class Q_REMOTEOBJECTS_EXPORT QRemoteObjectHost : public QRemoteObjectHostBase
{
    Q_OBJECT
public:
    QRemoteObjectHost(QObject *parent = nullptr);
    QRemoteObjectHost(const QUrl &address, const QUrl &registryAddress = QUrl(), QObject *parent = nullptr);
    QRemoteObjectHost(const QUrl &address, QObject *parent);
    virtual ~QRemoteObjectHost();
    QUrl hostUrl() const override;
    bool setHostUrl(const QUrl &hostAddress) override;

protected:
    QRemoteObjectHost(QRemoteObjectHostPrivate &, QObject *);

private:
    Q_DECLARE_PRIVATE(QRemoteObjectHost)
};

class Q_REMOTEOBJECTS_EXPORT QRemoteObjectRegistryHost : public QRemoteObjectHostBase
{
    Q_OBJECT
public:
    QRemoteObjectRegistryHost(const QUrl &registryAddress = QUrl(), QObject *parent = nullptr);
    virtual ~QRemoteObjectRegistryHost();
    bool setRegistryUrl(const QUrl &registryUrl) override;

protected:
    QRemoteObjectRegistryHost(QRemoteObjectRegistryHostPrivate &, QObject *);

private:
    Q_DECLARE_PRIVATE(QRemoteObjectRegistryHost)
};

QT_END_NAMESPACE

#endif
