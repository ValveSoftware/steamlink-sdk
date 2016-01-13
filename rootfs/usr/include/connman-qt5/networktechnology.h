/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef NETWORKTECHNOLOGY_H
#define NETWORKTECHNOLOGY_H

#include <QtDBus>

class NetConnmanTechnologyInterface;

class NetworkTechnology : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(bool powered READ powered WRITE setPowered NOTIFY poweredChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(quint32 idleTimeout READ idleTimeout WRITE setIdleTimeout NOTIFY idleTimeoutChanged)

    Q_PROPERTY(bool tethering READ tethering WRITE setTethering NOTIFY tetheringChanged)
    Q_PROPERTY(QString tetheringId READ tetheringId WRITE setTetheringId NOTIFY tetheringIdChanged)
    Q_PROPERTY(QString tetheringPassphrase READ tetheringPassphrase WRITE setTetheringPassphrase NOTIFY tetheringPassphraseChanged)

public:
    NetworkTechnology(const QString &path, const QVariantMap &properties, QObject* parent);
    NetworkTechnology(QObject* parent=0);

    virtual ~NetworkTechnology();

    const QString name() const;
    const QString type() const;
    bool powered() const;
    bool connected() const;
    const QString objPath() const;

    QString path() const;

    quint32 idleTimeout() const;
    void setIdleTimeout(quint32);

    bool tethering() const;
    void setTethering(bool);

    QString tetheringId() const;
    void setTetheringId(const QString &id);

    QString tetheringPassphrase() const;
    void setTetheringPassphrase(const QString &pass);


public Q_SLOTS:
    void setPowered(const bool &powered);
    void scan();
    void setPath(const QString &path);

Q_SIGNALS:
    void poweredChanged(const bool &powered);
    void connectedChanged(const bool &connected);
    void scanFinished();
    void idleTimeoutChanged(quint32 timeout);
    void tetheringChanged(bool tetheringEnabled);
    void tetheringIdChanged(const QString &tetheringId);
    void tetheringPassphraseChanged(const QString &passphrase);
    void pathChanged(const QString &path);
    void propertiesReady();

private:
    NetConnmanTechnologyInterface *m_technology;
    QVariantMap m_propertiesCache;

    static const QString Name;
    static const QString Type;
    static const QString Powered;
    static const QString Connected;
    static const QString IdleTimeout;
    static const QString Tethering;
    static const QString TetheringIdentifier;
    static const QString TetheringPassphrase;

    QString m_path;
    void init(const QString &path);


private Q_SLOTS:
    void propertyChanged(const QString &name, const QDBusVariant &value);
    void emitPropertyChange(const QString &name, const QVariant &value);

    void scanReply(QDBusPendingCallWatcher *call);
    void getPropertiesFinished(QDBusPendingCallWatcher *call);

private:
    Q_DISABLE_COPY(NetworkTechnology)
};

#endif //NETWORKTECHNOLOGY_H
