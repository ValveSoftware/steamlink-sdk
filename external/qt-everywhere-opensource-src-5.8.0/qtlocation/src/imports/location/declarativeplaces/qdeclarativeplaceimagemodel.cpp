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

#include "qdeclarativeplaceimagemodel_p.h"
#include "qdeclarativesupplier_p.h"

#include <QtCore/QUrl>
#include <QtLocation/QPlaceImage>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ImageModel
    \instantiates QDeclarativePlaceImageModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-models
    \since Qt Location 5.5

    \brief The ImageModel type provides a model of place images.

    The ImageModel is a read-only model used to fetch images related to a \l Place.
    Binding a \l Place via \l ImageModel::place initiates an initial fetch of images.
    The model performs fetches incrementally and is intended to be used in conjunction
    with a View such as a \l ListView.  When the View reaches the last of the images
    currently in the model, a fetch is performed to retrieve more if they are available.
    The View is automatically updated as the images are received.  The number of images
    which are fetched at a time is specified by the \l batchSize property.  The total number
    of images available can be accessed via the \l totalCount property.

    The model returns data for the following roles:

    \table
        \header
            \li Role
            \li Type
            \li Description
        \row
            \li url
            \li url
            \li The URL of the image.
        \row
            \li imageId
            \li string
            \li The identifier of the image.
        \row
            \li mimeType
            \li string
            \li The MIME type of the image.
        \row
            \li supplier
            \li \l Supplier
            \li The supplier of the image.
        \row
            \li user
            \li \l {QtLocation::User}{User}
            \li The user who contributed the image.
        \row
            \li attribution
            \li string
            \li Attribution text which must be displayed when displaying the image.
    \endtable


    \section1 Example

    The following example shows how to display images for a place:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml ImageModel
*/

/*!
    \qmlproperty Place ImageModel::place

    This property holds the Place that the images are for.
*/

/*!
    \qmlproperty int ImageModel::batchSize

    This property holds the batch size to use when fetching more image items.
*/

/*!
    \qmlproperty int ImageModel::totalCount

    This property holds the total number of image items for the place.
*/

QDeclarativePlaceImageModel::QDeclarativePlaceImageModel(QObject *parent)
:   QDeclarativePlaceContentModel(QPlaceContent::ImageType, parent)
{
}

QDeclarativePlaceImageModel::~QDeclarativePlaceImageModel()
{
    qDeleteAll(m_suppliers);
}

/*!
    \internal
*/
QVariant QDeclarativePlaceImageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rowCount(index.parent()) || index.row() < 0)
        return QVariant();

    const QPlaceImage &image = m_content.value(index.row());

    switch (role) {
    case UrlRole:
        return image.url();
    case ImageIdRole:
        return image.imageId();
    case MimeTypeRole:
        return image.mimeType();
    }

    return QDeclarativePlaceContentModel::data(index, role);
}

QHash<int, QByteArray> QDeclarativePlaceImageModel::roleNames() const
{
    QHash<int, QByteArray> roles = QDeclarativePlaceContentModel::roleNames();
    roles.insert(UrlRole, "url");
    roles.insert(ImageIdRole, "imageId");
    roles.insert(MimeTypeRole, "mimeType");
    return roles;
}

QT_END_NAMESPACE
