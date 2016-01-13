/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/qhash.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>

#include <QtGui/qfont.h>

#include "addressbookmodel.h"

AddressBookModel::AddressBookModel(QObject *parent)
    : EnginioModel(parent)
    , _searchReply()
{}

AddressBookModel::~AddressBookModel()
{}

//![data]
QVariant AddressBookModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        // we assume here that column order is constant and it is the same as in AddressBookModel::Roles
        return EnginioModel::data(index, FirstNameRole + index.column()).value<QJsonValue>().toString();
    }
    if (role == Qt::FontRole) {
        // this role is used to mark items found in the full text search.
        QFont font;
        QString id = EnginioModel::data(index, Enginio::IdRole).value<QString>();
        font.setBold(_markedItems.contains(id));
        return font;
    }

    return EnginioModel::data(index, role);
}

bool AddressBookModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool result = role == Qt::EditRole
            ? EnginioModel::setData(this->index(index.row()), value, FirstNameRole + index.column())
            : EnginioModel::setData(this->index(index.row()), value, role);

    // if the data was edited, the marked items set may not be valid anymore
    // so we need to clear it.
    if (result)
        _markedItems.clear();
    return result;
}
//![data]

//![tableViewIntegration]
int AddressBookModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant AddressBookModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return QStringLiteral("First name");
        case 1: return QStringLiteral("Last name");
        case 2: return QStringLiteral("Email");
        case 3: return QStringLiteral("Phone number");
        case 4: return QStringLiteral("Address");
        }
        return QVariant();
    }
    return EnginioModel::headerData(section, orientation, role);
}
//![tableViewIntegration]

//![Roles]
QHash<int, QByteArray> AddressBookModel::roleNames() const
{
    QHash<int, QByteArray> roles = EnginioModel::roleNames();
    roles.insert(FirstNameRole, "firstName");
    roles.insert(LastNameRole, "lastName");
    roles.insert(EmailRole, "email");
    roles.insert(PhoneRole, "phone");
    roles.insert(AddressRole, "address");
    return roles;
}
//![Roles]

void AddressBookModel::newSearch(const QString &search)
{
    if (search.isEmpty())
        return;

    //![query]
    // construct JSON object:
    //    {
    //        "objectTypes": ["objects.addressbook"],
    //        "search": { "phrase": "*PHRASE*",
    //            "properties": ["firstName", "lastName", "email", "phone", "address"]
    //        }
    //    }

    QJsonObject query;
    {
        QJsonArray objectTypes;
        objectTypes.append(QString::fromUtf8("objects.addressbook"));

        QJsonArray properties;
        properties.append(QString::fromUtf8("firstName"));
        properties.append(QString::fromUtf8("lastName"));
        properties.append(QString::fromUtf8("email"));
        properties.append(QString::fromUtf8("phone"));
        properties.append(QString::fromUtf8("address"));

        QJsonObject searchQuery;
        searchQuery.insert("phrase", "*" + search + "*");
        searchQuery.insert("properties", properties);

        query.insert("objectTypes", objectTypes);
        query.insert("search", searchQuery);
    }
    //![query]

    if (_searchReply) {
        // a new search is created before the old one was finished
        // we will not be interested in the results of the old search.
        _searchReply->disconnect(this);
    }

    //![callSearch]
    _searchReply =  client()->fullTextSearch(query);
    QObject::connect(_searchReply, &EnginioReply::finished, this, &AddressBookModel::searchResultsArrived);
    QObject::connect(_searchReply, &EnginioReply::finished, _searchReply, &EnginioReply::deleteLater);
    //![callSearch]
}

//![results]
void AddressBookModel::searchResultsArrived()
{
    // clear old marks.
    _markedItems.clear();

    // update marked ids.
    QJsonArray results = _searchReply->data()["results"].toArray();
    foreach (const QJsonValue &value, results) {
        QJsonObject person = value.toObject();
        _markedItems.insert(person["id"].toString());
    }

    QVector<int> roles;
    roles.append(Qt::FontRole);
    // We do not keep id -> row mapping, therefore it is easier to emit
    // data change signal for all items, even if it is not optimal from
    // the performance point of view.
    emit dataChanged(index(0), index(rowCount() - 1, columnCount() - 1) , roles);

    _searchReply = 0;
    emit searchFinished();
}
//![results]
