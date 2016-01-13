/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef QDECLARATIVEGEOCODEMODEL_H
#define QDECLARATIVEGEOCODEMODEL_H

#include "qdeclarativegeoserviceprovider_p.h"

#include <qgeocodereply.h>
#include <QtPositioning/private/qdeclarativegeoaddress_p.h>
#include <QtPositioning/private/qdeclarativegeolocation_p.h>

#include <QtQml/qqml.h>
#include <QtQml/QQmlParserStatus>
#include <QAbstractListModel>
#include <QPointer>

QT_BEGIN_NAMESPACE

class QGeoServiceProvider;
class QGeoCodingManager;
class QDeclarativeGeoLocation;

class QDeclarativeGeocodeModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_ENUMS(Status)
    Q_ENUMS(GeocodeError)

    Q_PROPERTY(QDeclarativeGeoServiceProvider *plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(int offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(QVariant query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariant bounds READ bounds WRITE setBounds NOTIFY boundsChanged)
    Q_PROPERTY(GeocodeError error READ error NOTIFY errorChanged)
    Q_INTERFACES(QQmlParserStatus)

public:
    enum Status {
        Null,
        Ready,
        Loading,
        Error
    };

    enum GeocodeError {
        NoError = QGeoCodeReply::NoError,
        EngineNotSetError = QGeoCodeReply::EngineNotSetError,
        CommunicationError = QGeoCodeReply::CommunicationError,
        ParseError = QGeoCodeReply::ParseError,
        UnsupportedOptionError = QGeoCodeReply::UnsupportedOptionError,
        CombinationError = QGeoCodeReply::CombinationError,
        UnknownError = QGeoCodeReply::UnknownError
    };

    enum Roles {
        LocationRole = Qt::UserRole + 1
    };

    explicit QDeclarativeGeocodeModel(QObject *parent = 0);
    virtual ~QDeclarativeGeocodeModel();

    // From QQmlParserStatus
    virtual void classBegin() {}
    virtual void componentComplete();

    // From QAbstractListModel
    virtual int rowCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QHash<int,QByteArray> roleNames() const;

    void setPlugin(QDeclarativeGeoServiceProvider *plugin);
    QDeclarativeGeoServiceProvider *plugin() const;

    void setBounds(const QVariant &boundingArea);
    QVariant bounds() const;

    Status status() const;
    QString errorString() const;
    GeocodeError error() const;

    bool autoUpdate() const;
    void setAutoUpdate(bool update);

    int count() const;
    Q_INVOKABLE QDeclarativeGeoLocation *get(int index);

    int limit() const;
    void setLimit(int limit);
    int offset() const;
    void setOffset(int offset);

    QVariant query() const;
    void setQuery(const QVariant &query);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void countChanged();
    void pluginChanged();
    void statusChanged();
    void errorStringChanged();
    void errorChanged();
    void locationsChanged();
    void autoUpdateChanged();
    void boundsChanged();
    void queryChanged();
    void limitChanged();
    void offsetChanged();

public Q_SLOTS:
    void update();

protected Q_SLOTS:
    void queryContentChanged();
    void geocodeFinished(QGeoCodeReply *reply);
    void geocodeError(QGeoCodeReply *reply,
                     QGeoCodeReply::Error error,
                     const QString &errorString);
    void pluginReady();

protected:
    QGeoCodingManager *searchManager();
    void setStatus(Status status);
    void setErrorString(const QString &error);
    void setError(GeocodeError error);
    bool autoUpdate_;
    bool complete_;

private:
    void setLocations(const QList<QGeoLocation> &locations);
    void abortRequest();
    QGeoCodeReply *reply_;

    QDeclarativeGeoServiceProvider *plugin_;
    QGeoShape boundingArea_;

    QList<QDeclarativeGeoLocation *> declarativeLocations_;

    Status status_;
    QString errorString_;
    GeocodeError error_;
    QVariant queryVariant_;
    QGeoCoordinate coordinate_;
    QDeclarativeGeoAddress *address_;
    QString searchString_;

    int limit_;
    int offset_;
};

QT_END_NAMESPACE

#endif
