/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include "contactmodel.h"

ContactModel::ContactModel(QObject *parent ) : QAbstractListModel(parent)
{
    m_contacts.append({ "Angel Hogan", "Chapel St. 368 ", "Clearwater" , "0311 1823993" });
    m_contacts.append({ "Felicia Patton", "Annadale Lane 2", "Knoxville" , "0368 1244494" });
    m_contacts.append({ "Grant Crawford", "Windsor Drive 34", "Riverdale" , "0351 7826892" });
    m_contacts.append({ "Gretchen Little", "Sunset Drive 348", "Virginia Beach" , "0343 1234991" });
    m_contacts.append({ "Geoffrey Richards", "University Lane 54", "Trussville" , "0423 2144944" });
    m_contacts.append({ "Henrietta Chavez", "Via Volto San Luca 3", "Piobesi Torinese" , "0399 2826994" });
    m_contacts.append({ "Harvey Chandler", "North Squaw Creek 11", "Madisonville" , "0343 1244492" });
    m_contacts.append({ "Miguel Gomez", "Wild Rose Street 13", "Trussville" , "0343 9826996" });
    m_contacts.append({ "Norma Rodriguez", " Glen Eagles Street  53", "Buffalo" , "0241 5826596" });
    m_contacts.append({ "Shelia Ramirez", "East Miller Ave 68", "Pickerington" , "0346 4844556" });
    m_contacts.append({ "Stephanie Moss", "Piazza Trieste e Trento 77", "Roata Chiusani" , "0363 0510490" });
}

int ContactModel::rowCount(const QModelIndex &) const
{
    return m_contacts.count();
}

QVariant ContactModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < rowCount())
        switch (role) {
        case FullNameRole: return m_contacts.at(index.row()).fullName;
        case AddressRole: return m_contacts.at(index.row()).address;
        case CityRole: return m_contacts.at(index.row()).city;
        case NumberRole: return m_contacts.at(index.row()).number;
        default: return QVariant();
    }
    return QVariant();
}

QHash<int, QByteArray> ContactModel::roleNames() const
{
    static const QHash<int, QByteArray> roles {
        { FullNameRole, "fullName" },
        { AddressRole, "address" },
        { CityRole, "city" },
        { NumberRole, "number" }
    };
    return roles;
}

QVariantMap ContactModel::get(int row) const
{
    const Contact contact = m_contacts.value(row);
    return { {"fullName", contact.fullName}, {"address", contact.address}, {"city", contact.city}, {"number", contact.number} };
}

void ContactModel::append(const QString &fullName, const QString &address, const QString &city, const QString &number)
{
    int row = 0;
    while (row < m_contacts.count() && fullName > m_contacts.at(row).fullName)
        ++row;
    beginInsertRows(QModelIndex(), row, row);
    m_contacts.insert(row, {fullName, address, city, number});
    endInsertRows();
}

void ContactModel::set(int row, const QString &fullName, const QString &address, const QString &city, const QString &number)
{
    if (row < 0 || row >= m_contacts.count())
        return;

    m_contacts.replace(row, { fullName, address, city, number });
    dataChanged(index(row, 0), index(row, 0), { FullNameRole, AddressRole, CityRole, NumberRole });
}

void ContactModel::remove(int row)
{
    if (row < 0 || row >= m_contacts.count())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_contacts.removeAt(row);
    endRemoveRows();
}
