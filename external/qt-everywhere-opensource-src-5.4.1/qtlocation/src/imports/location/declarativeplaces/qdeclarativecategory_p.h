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

#ifndef QDECLARATIVECATEGORY_P_H
#define QDECLARATIVECATEGORY_P_H

#include <QtQml/qqml.h>
#include <QtQml/QQmlParserStatus>
#include <QObject>

#include <qplacecategory.h>

#include "qdeclarativegeoserviceprovider_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativePlaceIcon;
class QPlaceReply;
class QPlaceManager;

class QDeclarativeCategory : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_ENUMS(Status Visibility)


    Q_PROPERTY(QPlaceCategory category READ category WRITE setCategory)
    Q_PROPERTY(QDeclarativeGeoServiceProvider *plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QString categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(Visibility visibility READ visibility WRITE setVisibility NOTIFY visibilityChanged)
    Q_PROPERTY(QDeclarativePlaceIcon *icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    Q_INTERFACES(QQmlParserStatus)

public:
    explicit QDeclarativeCategory(QObject *parent = 0);
    QDeclarativeCategory(const QPlaceCategory &category, QDeclarativeGeoServiceProvider *plugin, QObject *parent = 0);
    ~QDeclarativeCategory();

    enum Visibility {
        UnspecifiedVisibility = QLocation::UnspecifiedVisibility,
        DeviceVisibility = QLocation::DeviceVisibility,
        PrivateVisibility = QLocation::PrivateVisibility,
        PublicVisibility = QLocation::PublicVisibility
    };
    enum Status {Ready, Saving, Removing, Error};

    //From QQmlParserStatus
    virtual void classBegin() {}
    virtual void componentComplete();

    void setPlugin(QDeclarativeGeoServiceProvider *plugin);
    QDeclarativeGeoServiceProvider *plugin() const;

    QPlaceCategory category();
    void setCategory(const QPlaceCategory &category);

    QString categoryId() const;
    void setCategoryId(const QString &catID);
    QString name() const;
    void setName(const QString &name);

    Visibility visibility() const;
    void setVisibility(Visibility visibility);

    QDeclarativePlaceIcon *icon() const;
    void setIcon(QDeclarativePlaceIcon *icon);

    Q_INVOKABLE QString errorString() const;

    Status status() const;
    void setStatus(Status status, const QString &errorString = QString());

    Q_INVOKABLE void save(const QString &parentId = QString());
    Q_INVOKABLE void remove();

Q_SIGNALS:
    void pluginChanged();
    void categoryIdChanged();
    void nameChanged();
    void visibilityChanged();
    void iconChanged();
    void statusChanged();

private Q_SLOTS:
    void replyFinished();
    void pluginReady();

private:
    QPlaceManager *manager();

    QPlaceCategory m_category;
    QDeclarativePlaceIcon *m_icon;
    QDeclarativeGeoServiceProvider *m_plugin;
    QPlaceReply *m_reply;
    bool m_complete;
    Status m_status;
    QString m_errorString;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeCategory)

#endif // QDECLARATIVECATEGORY_P_H
