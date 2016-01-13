/*
 * Copyright © 2010, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef CLOCKMODEL_H
#define CLOCKMODEL_H

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTime>

class QDBusPendingCallWatcher;
class QDBusVariant;

class NetConnmanClockInterface;

namespace Tests {
    class UtClock;
}

class ClockModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString timezone READ timezone WRITE setTimezone NOTIFY timezoneChanged)
    Q_PROPERTY(QString timezoneUpdates READ timezoneUpdates WRITE setTimezoneUpdates NOTIFY timezoneUpdatesChanged)
    Q_PROPERTY(QString timeUpdates READ timeUpdates WRITE setTimeUpdates NOTIFY timeUpdatesChanged)
    Q_PROPERTY(QStringList timeservers READ timeservers WRITE setTimeservers NOTIFY timeserversChanged)

    friend class Tests::UtClock;

public:
    ClockModel();

public Q_SLOTS:
    QString timezone() const;
    void setTimezone(const QString &val);
    QString timezoneUpdates() const;
    void setTimezoneUpdates(const QString &val);
    QString timeUpdates() const;
    void setTimeUpdates(const QString &val);
    QStringList timeservers() const;
    void setTimeservers(const QStringList &val);

    void setDate(QDate date);
    void setTime(QTime time);

    // helper function for Timepicker
    QTime time(QString h, QString m) { return QTime(h.toInt(), m.toInt()); }

Q_SIGNALS:
    void timezoneChanged();
    void timezoneUpdatesChanged();
    void timeUpdatesChanged();
    void timeserversChanged();

private Q_SLOTS:
    void connectToConnman();
    void getPropertiesFinished(QDBusPendingCallWatcher*);
    void setPropertyFinished(QDBusPendingCallWatcher*);
    void propertyChanged(const QString&, const QDBusVariant&);

private:
    NetConnmanClockInterface *mClockProxy;
    QString mTimezone;
    QString mTimezoneUpdates;
    QString mTimeUpdates;
    QStringList mTimeservers;

    Q_DISABLE_COPY(ClockModel)
};

#endif
