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

#ifndef QDECLARATIVEPLACEICON_P_H
#define QDECLARATIVEPLACEICON_P_H

#include "qdeclarativegeoserviceprovider_p.h"

#include <qplaceicon.h>
#include <QtQml/qqml.h>
#include <QtQml/QQmlPropertyMap>

#include <QObject>

QT_BEGIN_NAMESPACE

class QQmlPropertyMap;

class QDeclarativePlaceIcon : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPlaceIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(QObject *parameters READ parameters NOTIFY parametersChanged)
    Q_PROPERTY(QDeclarativeGeoServiceProvider *plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)

public:
    explicit QDeclarativePlaceIcon(QObject *parent = 0);
    QDeclarativePlaceIcon(const QPlaceIcon &src, QDeclarativeGeoServiceProvider *plugin, QObject *parent = 0);
    ~QDeclarativePlaceIcon();

    QPlaceIcon icon() const;
    void setIcon(const QPlaceIcon &src);

    Q_INVOKABLE QUrl url(const QSize &size = QSize()) const;

    QQmlPropertyMap *parameters() const;

    void setPlugin(QDeclarativeGeoServiceProvider *plugin);
    QDeclarativeGeoServiceProvider *plugin() const;

Q_SIGNALS:
    void pluginChanged();
    void parametersChanged(); //in practice is never emitted since parameters cannot be re-assigned
                              //the declaration is needed to avoid warnings about non-notifyable properties

private Q_SLOTS:
    void pluginReady();

private:
    QPlaceManager *manager() const;
    void initParameters(const QVariantMap &parameterMap);
    QDeclarativeGeoServiceProvider *m_plugin;
    QQmlPropertyMap *m_parameters;
};

QT_END_NAMESPACE

#endif
