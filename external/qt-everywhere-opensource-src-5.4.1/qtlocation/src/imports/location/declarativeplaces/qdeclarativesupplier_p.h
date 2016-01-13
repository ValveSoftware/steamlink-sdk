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

#ifndef QDECLARATIVESUPPLIER_P_H
#define QDECLARATIVESUPPLIER_P_H

#include <QObject>
#include <QtCore/QUrl>
#include <QtQml/qqml.h>
#include <QtQml/QQmlParserStatus>
#include <qplacesupplier.h>

#include "qdeclarativeplaceicon_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeSupplier : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QPlaceSupplier supplier READ supplier WRITE setSupplier)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString supplierId READ supplierId WRITE setSupplierId NOTIFY supplierIdChanged)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QDeclarativePlaceIcon *icon READ icon WRITE setIcon NOTIFY iconChanged)

    Q_INTERFACES(QQmlParserStatus)

public:
    explicit QDeclarativeSupplier(QObject *parent = 0);
    explicit QDeclarativeSupplier(const QPlaceSupplier &src, QDeclarativeGeoServiceProvider *plugin, QObject *parent = 0);
    ~QDeclarativeSupplier();

    // From QQmlParserStatus
    void classBegin() { }
    void componentComplete();

    QPlaceSupplier supplier();
    void setSupplier(const QPlaceSupplier &src, QDeclarativeGeoServiceProvider *plugin = 0);

    QString name() const;
    void setName(const QString &data);
    QString supplierId() const;
    void setSupplierId(const QString &data);
    QUrl url() const;
    void setUrl(const QUrl &data);

    QDeclarativePlaceIcon *icon() const;
    void setIcon(QDeclarativePlaceIcon *icon);

Q_SIGNALS:
    void nameChanged();
    void supplierIdChanged();
    void urlChanged();
    void iconChanged();

private:
    QPlaceSupplier m_src;
    QDeclarativePlaceIcon *m_icon;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeSupplier)

#endif // QDECLARATIVESUPPLIER_P_H
