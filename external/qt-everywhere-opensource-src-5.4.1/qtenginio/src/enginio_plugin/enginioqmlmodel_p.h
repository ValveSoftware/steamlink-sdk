/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#ifndef ENGINIOQMLMODEL_H
#define ENGINIOQMLMODEL_H

#include <Enginio/enginiobasemodel.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class EnginioQmlReply;
class EnginioQmlClient;
class EnginioQmlModelPrivate;
class EnginioQmlModel : public EnginioBaseModel
{
    Q_OBJECT
    Q_DISABLE_COPY(EnginioQmlModel)
public:
    EnginioQmlModel(QObject *parent = 0);
    ~EnginioQmlModel();

    Q_ENUMS(Enginio::Operation) // TODO remove me QTBUG-33577
    Q_ENUMS(Enginio::ErrorType); // TODO remove me QTBUG-33577
    Q_ENUMS(Enginio::Role); // TODO remove me QTBUG-33577

    Q_PROPERTY(EnginioQmlClient *client READ client WRITE setClient NOTIFY clientChanged)
    Q_PROPERTY(QJSValue query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(Enginio::Operation operation READ operation WRITE setOperation NOTIFY operationChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)

    EnginioQmlClient *client() const Q_REQUIRED_RESULT;
    void setClient(const EnginioQmlClient *client);

    QJSValue query() Q_REQUIRED_RESULT;
    void setQuery(const QJSValue &query);

    Enginio::Operation operation() const Q_REQUIRED_RESULT;
    void setOperation(Enginio::Operation operation);

    Q_INVOKABLE EnginioQmlReply *append(const QJSValue &value);
    Q_INVOKABLE EnginioQmlReply *remove(int row);
    Q_INVOKABLE EnginioQmlReply *setProperty(int row, const QString &role, const QVariant &value);
    Q_INVOKABLE EnginioQmlReply *reload();

Q_SIGNALS:
    void queryChanged(const QJSValue &query);
    void clientChanged(EnginioQmlClient *client);
    void operationChanged(Enginio::Operation operation);
    void rowCountChanged();

private:
    Q_DECLARE_PRIVATE(EnginioQmlModel)
};

QT_END_NAMESPACE

#endif // ENGINIOQMLOBJECTMODEL_H
