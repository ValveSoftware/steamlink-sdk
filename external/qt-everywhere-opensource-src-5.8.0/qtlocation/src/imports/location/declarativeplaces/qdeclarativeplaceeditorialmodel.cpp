/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativeplaceeditorialmodel.h"

#include <QtCore/QUrl>
#include <QtLocation/QPlaceEditorial>

QT_BEGIN_NAMESPACE

/*!
    \qmltype EditorialModel
    \instantiates QDeclarativePlaceEditorialModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-models
    \since Qt Location 5.5

    \brief The EditorialModel type provides a model of place editorials.

    The EditorialModel is a read-only model used to fetch editorials related to a \l Place.
    Binding a \l Place via \l EditorialModel::place initiates an initial fetch of editorials.
    The model performs fetches incrementally and is intended to be used in conjunction
    with a View such as a \l ListView.  When the View reaches the last of the editorials
    currently in the model, a fetch is performed to retrieve more if they are available.
    The View is automatically updated as the editorials are received.  The number of
    editorials which are fetched at a time is specified by the \l batchSize property.
    The total number of editorials available can be accessed via the \l totalCount property.

    The model returns data for the following roles:

    \table
        \header
            \li Role
            \li Type
            \li Description
        \row
            \li text
            \li string
            \li The editorial's textual description of the place.  It can be either rich (HTML based) text or plain text
               depending upon the provider.
        \row
            \li title
            \li string
            \li The title of the editorial.
        \row
            \li language
            \li string
            \li The language that the editorial is written in.
        \row
            \li supplier
            \li \l Supplier
            \li The supplier of the editorial.
        \row
            \li user
            \li \l {QtLocation::User}{User}
            \li The user who contributed the editorial.
        \row
            \li attribution
            \li string
            \li Attribution text which must be displayed when displaying the editorial.
    \endtable

    \section1 Example

    The following example shows how to display editorials for a place:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml EditorialModel

*/

/*!
    \qmlproperty Place EditorialModel::place

    This property holds the Place that the editorials are for.
*/

/*!
    \qmlproperty int EditorialModel::batchSize

    This property holds the batch size to use when fetching more editorials items.
*/

/*!
    \qmlproperty int EditorialModel::totalCount

    This property holds the total number of editorial items for the place.
*/

QDeclarativePlaceEditorialModel::QDeclarativePlaceEditorialModel(QObject *parent)
:   QDeclarativePlaceContentModel(QPlaceContent::EditorialType, parent)
{
}

QDeclarativePlaceEditorialModel::~QDeclarativePlaceEditorialModel()
{
}

/*!
    \internal
*/
QVariant QDeclarativePlaceEditorialModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rowCount(index.parent()) || index.row() < 0)
        return QVariant();

    const QPlaceEditorial &description = m_content.value(index.row());

    switch (role) {
    case TextRole:
        return description.text();
    case TitleRole:
        return description.title();
    case LanguageRole:
        return description.language();
    }

    return QDeclarativePlaceContentModel::data(index, role);
}

QHash<int, QByteArray> QDeclarativePlaceEditorialModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QDeclarativePlaceContentModel::roleNames();
    roleNames.insert(TextRole, "text");
    roleNames.insert(TitleRole, "title");
    roleNames.insert(LanguageRole, "language");
    return roleNames;
}

QT_END_NAMESPACE
