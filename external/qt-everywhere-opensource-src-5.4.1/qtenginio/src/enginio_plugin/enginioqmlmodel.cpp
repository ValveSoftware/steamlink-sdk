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

#include "enginioqmlmodel_p.h"
#include <Enginio/private/enginiobasemodel_p.h>
#include <QtCore/qjsondocument.h>
#include "enginioqmlclient_p_p.h"
#include "enginioqmlreply_p.h"
#include "enginioqmlobjectadaptor_p.h"


QT_BEGIN_NAMESPACE

/*!
  \qmltype EnginioModel
  \since 5.3
  \instantiates EnginioQmlModel
  \inqmlmodule Enginio
  \ingroup engino-qml

  \brief Makes accessing collections of objects easy
  \snippet simple.qml import

  The query property of the model takes a JSON object.

  To get a model of each object of type "objects.city" use:
  \snippet models.qml model

  It is then possible to use a regular Qt Quick ListView
  to display the list of cities that the backend contains.
  \snippet models.qml view
  Note that the properties of the objects can be directly accessed.
  In this example, we have the type "objects.city" in the backend
  with two properties: "name" and "population".

  The model supports several function to modify the data, for example
  \l append(), \l remove(), \l setProperty()

  The QML version of EnginioModel supports the same functionality as the C++ version.
  \l {EnginioModelCpp}{EnginioModel C++}
*/

/*!
  \qmlproperty QJSValue EnginioModel::query
  \include model-query.qdocinc 0
  \l {Enginio::EnginioClient::query()}{EnginioClient::query()}
  \include model-query.qdocinc 1

  \sa {Enginio::EnginioClient::query()}{EnginioClient::query()}
*/

/*!
  \qmlproperty EnginioClient EnginioModel::client
  The instance of \l EnginioClient used for this model.
*/

/*!
  \qmlproperty Enginio::Operation EnginioModel::operation
  The operation used for the \l query.
*/

/*!
  \qmlmethod EnginioReply EnginioModel::append(QJSValue object)
  \include model-append.qdocinc

  To add a "city" object to the model by appending it:
  \snippet models.qml append
*/

/*!
  \qmlmethod EnginioReply EnginioModel::setProperty(int row, string propertyName, QVariant value)
  \brief Change a property of an object in the model

  The property \a propertyName of the object at \a row will be set to \a value.
  The model will locally reflect the change immediately and propagage the change to the
  server in the background.
*/

/*!
  \qmlmethod EnginioReply EnginioModel::remove(int row)
  \include model-remove.qdocinc

  \sa {Enginio::EnginioClient::remove()}{EnginioClient::remove()}
*/

namespace  {

struct Types
{
    typedef EnginioQmlReply Reply;
    typedef EnginioQmlModel Public;
    typedef EnginioQmlClient Client;
    typedef EnginioQmlClientPrivate ClientPrivate;
    typedef QJSValue Data;
};

} // namespace

class EnginioQmlModelPrivate : public EnginioModelPrivateT<EnginioQmlModelPrivate, Types>
{
public:
    typedef EnginioModelPrivateT<EnginioQmlModelPrivate, Types> Base;

    QJSValue convert(const QJsonObject &object) const
    {
        // TODO that is sooo bad, that I can not look at this.
        // TODO trace all calls and try to remove them
        EnginioQmlClientPrivate *enginio = static_cast<EnginioQmlClientPrivate*>(_enginio);
        QJsonDocument doc(object);
        QByteArray buffer = doc.toJson(QJsonDocument::Compact);
        return enginio->fromJson(buffer);
    }

    QJsonObject convert(const QJSValue &object) const
    {
        // TODO that is sooo bad, that I can not look at this.
        // TODO trace all calls and try to remove them
        EnginioQmlClientPrivate *enginio = static_cast<EnginioQmlClientPrivate*>(_enginio);
        QByteArray buffer = enginio->toJson(object);
        return QJsonDocument::fromJson(buffer).object();
    }

    EnginioQmlModelPrivate(EnginioBaseModel *pub)
        : Base(pub)
    {}

    virtual QJsonObject replyData(const EnginioReplyState *reply) const Q_DECL_OVERRIDE
    {
        QJSValue data = static_cast<const EnginioQmlReply*>(reply)->data();
        return convert(data);
    }

    virtual QJsonValue queryData(const QString &name) Q_DECL_OVERRIDE
    {
        QJSValue value = _query.property(name);
        if (value.isObject())
            return convert(value);
        if (value.isString())
            return QJsonValue(value.toString());
        if (value.isBool())
            return QJsonValue(value.toBool());
        if (value.isNumber())
            return QJsonValue(value.toNumber());
        if (value.isUndefined())
            return QJsonValue(QJsonValue::Undefined);
        if (value.isNull())
            return QJsonValue(QJsonValue::Null);
        Q_ASSERT(false);
        return QJsonValue();
    }

    virtual QJsonObject queryAsJson() const Q_DECL_OVERRIDE
    {
        return convert(_query);
    }
};

EnginioQmlModel::EnginioQmlModel(QObject *parent)
    : EnginioBaseModel(*new EnginioQmlModelPrivate(this), parent)
{
    Q_D(EnginioQmlModel);
    d->init();
    QObject::connect(this, &EnginioBaseModel::rowsRemoved, this, &EnginioQmlModel::rowCountChanged);
    QObject::connect(this, &EnginioBaseModel::rowsInserted, this, &EnginioQmlModel::rowCountChanged);
    QObject::connect(this, &EnginioBaseModel::modelReset, this, &EnginioQmlModel::rowCountChanged);
}

EnginioQmlModel::~EnginioQmlModel()
{
}

EnginioQmlReply *EnginioQmlModel::append(const QJSValue &value)
{
    Q_D(EnginioQmlModel);

    if (Q_UNLIKELY(!d->enginio())) {
        qWarning("EnginioQmlModel::append(): Enginio client is not set");
        return 0;
    }

    return d->append(d->convert(value));
}

EnginioQmlReply *EnginioQmlModel::remove(int row)
{
    Q_D(EnginioQmlModel);
    if (Q_UNLIKELY(!d->enginio())) {
        qWarning("EnginioQmlModel::remove(): Enginio client is not set");
        return 0;
    }

    if (unsigned(row) >= unsigned(d->rowCount())) {
        EnginioQmlClientPrivate *client = EnginioQmlClientPrivate::get(d->enginio());
        QNetworkReply *nreply = new EnginioFakeReply(client, EnginioQmlClientPrivate::constructErrorMessage(EnginioString::EnginioQmlModel_remove_row_is_out_of_range));
        EnginioQmlReply *ereply = new EnginioQmlReply(client, nreply);
        return ereply;
    }

    return d->remove(row);
}

EnginioQmlReply *EnginioQmlModel::setProperty(int row, const QString &role, const QVariant &value)
{
    Q_D(EnginioQmlModel);
    if (Q_UNLIKELY(!d->enginio())) {
        qWarning("EnginioQmlModel::setProperty(): Enginio client is not set");
        return 0;
    }

    if (unsigned(row) >= unsigned(d->rowCount())) {
        EnginioQmlClientPrivate *client = EnginioQmlClientPrivate::get(d->enginio());
        QNetworkReply *nreply = new EnginioFakeReply(client, EnginioQmlClientPrivate::constructErrorMessage(EnginioString::EnginioQmlModel_setProperty_row_is_out_of_range));
        EnginioQmlReply *ereply = new EnginioQmlReply(client, nreply);
        return ereply;
    }

    return d->setValue(row, role, value);
}

EnginioQmlReply *EnginioQmlModel::reload()
{
    Q_D(EnginioQmlModel);
    return d->reload();
}

EnginioQmlClient *EnginioQmlModel::client() const
{
    Q_D(const EnginioQmlModel);
    return d->enginio();
}

void EnginioQmlModel::setClient(const EnginioQmlClient *enginio)
{
    Q_D(EnginioQmlModel);
    if (enginio == d->enginio())
        return;

    d->setClient(enginio);
}

QJSValue EnginioQmlModel::query()
{
    Q_D(EnginioQmlModel);
    return d->query();
}

void EnginioQmlModel::setQuery(const QJSValue &query)
{
    Q_D(EnginioQmlModel);
    if (d->query().equals(query))
        return;
    return d->setQuery(query);
}

Enginio::Operation EnginioQmlModel::operation() const
{
    Q_D(const EnginioQmlModel);
    return d->operation();
}

void EnginioQmlModel::setOperation(Enginio::Operation operation)
{
    Q_D(EnginioQmlModel);
    if (operation == d->operation())
        return;
    d->setOperation(operation);
    emit operationChanged(operation);
}

QT_END_NAMESPACE
