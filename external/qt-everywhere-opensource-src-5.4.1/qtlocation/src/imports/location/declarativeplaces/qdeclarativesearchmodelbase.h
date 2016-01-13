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

#ifndef QDECLARATIVESEARCHMODELBASE_H
#define QDECLARATIVESEARCHMODELBASE_H

#include <QtCore/QAbstractListModel>
#include <QtQml/QQmlParserStatus>
#include <QtLocation/QPlaceSearchRequest>
#include <QtLocation/QPlaceSearchResult>
#include <QtLocation/QPlaceReply>

#include "qdeclarativegeoserviceprovider_p.h"

QT_BEGIN_NAMESPACE

class QPlaceManager;
class QPlaceSearchRequest;
class QPlaceSearchReply;
class QDeclarativePlace;

class QDeclarativeSearchModelBase : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QDeclarativeGeoServiceProvider *plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QVariant searchArea READ searchArea WRITE setSearchArea NOTIFY searchAreaChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(bool previousPagesAvailable READ previousPagesAvailable NOTIFY previousPagesAvailableChanged)
    Q_PROPERTY(bool nextPagesAvailable READ nextPagesAvailable NOTIFY nextPagesAvailableChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    Q_ENUMS(Status)

    Q_INTERFACES(QQmlParserStatus)

public:
    enum Status {
        Null,
        Ready,
        Loading,
        Error
    };

    explicit QDeclarativeSearchModelBase(QObject *parent = 0);
    ~QDeclarativeSearchModelBase();

    QDeclarativeGeoServiceProvider *plugin() const;
    void setPlugin(QDeclarativeGeoServiceProvider *plugin);

    QVariant searchArea() const;
    void setSearchArea(const QVariant &searchArea);

    int limit() const;
    void setLimit(int limit);

    bool previousPagesAvailable() const;
    bool nextPagesAvailable() const;

    Status status() const;
    void setStatus(Status status, const QString &errorString = QString());

    Q_INVOKABLE void update();

    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();

    Q_INVOKABLE QString errorString() const;

    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void nextPage();

    virtual void clearData(bool suppressSignal = false);

    // From QQmlParserStatus
    virtual void classBegin();
    virtual void componentComplete();

Q_SIGNALS:
    void pluginChanged();
    void searchAreaChanged();
    void limitChanged();
    void previousPagesAvailableChanged();
    void nextPagesAvailableChanged();
    void statusChanged();

protected:
    virtual void initializePlugin(QDeclarativeGeoServiceProvider *plugin);

protected Q_SLOTS:
    virtual void queryFinished() = 0;

private Q_SLOTS:
    void pluginNameChanged();

protected:
    virtual QPlaceReply *sendQuery(QPlaceManager *manager, const QPlaceSearchRequest &request) = 0;
    void setPreviousPageRequest(const QPlaceSearchRequest &previous);
    void setNextPageRequest(const QPlaceSearchRequest &next);

    QPlaceSearchRequest m_request;
    QDeclarativeGeoServiceProvider *m_plugin;
    QPlaceReply *m_reply;

private:
    bool m_complete;
    Status m_status;
    QString m_errorString;
    QPlaceSearchRequest m_previousPageRequest;
    QPlaceSearchRequest m_nextPageRequest;
};

QT_END_NAMESPACE

#endif // QDECLARATIVESEARCHMODELBASE_H
