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

#include "qdeclarativesearchsuggestionmodel_p.h"
#include "qdeclarativegeoserviceprovider_p.h"

#include <QtQml/QQmlInfo>
#include <QtLocation/QGeoServiceProvider>

#include <qplacemanager.h>
#include <qplacesearchrequest.h>

QT_USE_NAMESPACE

/*!
    \qmltype PlaceSearchSuggestionModel
    \instantiates QDeclarativeSearchSuggestionModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-models
    \since Qt Location 5.5

    \brief Provides access to search term suggestions.

    The PlaceSearchSuggestionModel can be used to provide search term suggestions as the user enters their
    search term.  The properties of this model should match that of the \l PlaceSearchModel, which
    will be used to perform the actual search query, to ensure that the search suggestion results are
    relevant.

    There are two ways of accessing the data provided by this model, either through the
    \l suggestions property or through views and delegates.  The latter is the preferred
    method.

    The \l offset and \l limit properties can be used to access paged suggestions.  When the
    \l offset and \l limit properties are set the suggestions between \l offset and
    (\l offset + \l limit - 1) will be returned.  Support for paging may vary
    from plugin to plugin.

    The model returns data for the following roles:

    \table
        \header
            \li Role
            \li Type
            \li Description
        \row
            \li suggestion
            \li string
            \li Suggested search term.
    \endtable

    The following example shows how to use the PlaceSearchSuggestionModel to get suggested search terms
    from a partial search term.  The \l searchArea is set to match what would be used to perform the
    actual place search with \l PlaceSearchModel.

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml SearchSuggestionModel

    \sa PlaceSearchModel, {QPlaceManager}
*/

/*!
    \qmlproperty Plugin PlaceSearchSuggestionModel::plugin

    This property holds the provider \l Plugin which will be used to perform the search.
*/

/*!
    \qmlproperty geoshape PlaceSearchSuggestionModel::searchArea

    This property holds the search area.  Search suggestion results returned by the model will be
    relevant to the given search area.

    If this property is set to a \l {geocircle} its
    \l {geocircle}{radius} property may be left unset, in which case the \l Plugin
    will choose an appropriate radius for the search.
*/

/*!
    \qmlproperty int PlaceSearchSuggestionModel::offset

    This property holds the index of the first item in the model.

    \sa limit
*/

/*!
    \qmlproperty int PlaceSearchSuggestionModel::limit

    This property holds the limit of the number of items that will be returned.

    \sa offset
*/

/*!
    \qmlproperty enum PlaceSearchSuggestionModel::status

    This property holds the status of the model.  It can be one of:

    \table
        \row
            \li PlaceSearchSuggestionModel.Null
            \li No search query has been executed.  The model is empty.
        \row
            \li PlaceSearchSuggestionModel.Ready
            \li The search query has completed, and the results are available.
        \row
            \li PlaceSearchSuggestionModel.Loading
            \li A search query is currently being executed.
        \row
            \li PlaceSearchSuggestionModel.Error
            \li An error occurred when executing the previous search query.
    \endtable
*/

/*!
    \qmlmethod void PlaceSearchSuggestionModel::update()

    Updates the model based on the provided query parameters.  The model will be populated with a
    list of search suggestions for the partial \l searchTerm and \l searchArea.  If the \l plugin
    supports it, other parameters such as \l limit and \l offset may be specified.  \c update()
    submits the set of parameters to the \l plugin to process.


    While the model is updating the \l status of the model is set to
    \c PlaceSearchSuggestionModel.Loading.  If the model is successfully updated, the \l status is
    set to \c PlaceSearchSuggestionModel.Ready, while if it unsuccessfully completes, the \l status
    is set to \c PlaceSearchSuggestionModel.Error and the model cleared.

    This example shows use of the model
    \code
    PlaceSeachSuggestionModel {
        id: model
        plugin: backendPlugin
        searchArea: QtPositioning.circle(QtPositioning.coordinate(10, 10))
        ...
    }

    MouseArea {
        ...
        onClicked: {
            model.searchTerm = "piz"
            model.searchArea.center.latitude = -27.5;
            model.searchArea.cetner.longitude = 153;
            model.update();
        }
    }
    \endcode

    A more detailed example can be found in the in
    \l {Places (QML)#Presenting-Search-Suggestions}{Places (QML)} example.

    \sa cancel(), status
*/

/*!
    \qmlmethod void PlaceSearchSuggestionModel::cancel()

    Cancels an ongoing search suggestion operation immediately and sets the model
    status to PlaceSearchSuggestionModel.Ready.  The model retains any search
    suggestions it had before the operation was started.

    If an operation is not ongoing, invoking cancel() has no effect.

    \sa update(), status
*/

/*!
    \qmlmethod void PlaceSearchSuggestionModel::reset()

    Resets the model.  All search suggestions are cleared, any outstanding requests are aborted and
    possible errors are cleared.  Model status will be set to PlaceSearchSuggestionModel.Null.
*/


/*!
    \qmlmethod string QtLocation::PlaceSearchSuggestionModel::errorString() const

    This read-only property holds the textual presentation of the latest search suggestion model error.
    If no error has occurred, or if the model was cleared, an empty string is returned.

    An empty string may also be returned if an error occurred which has no associated
    textual representation.
*/

QDeclarativeSearchSuggestionModel::QDeclarativeSearchSuggestionModel(QObject *parent)
:   QDeclarativeSearchModelBase(parent)
{
}

QDeclarativeSearchSuggestionModel::~QDeclarativeSearchSuggestionModel()
{
}

/*!
    \qmlproperty string PlaceSearchSuggestionModel::searchTerm

    This property holds the partial search term used in query.
*/
QString QDeclarativeSearchSuggestionModel::searchTerm() const
{
    return m_request.searchTerm();
}

void QDeclarativeSearchSuggestionModel::setSearchTerm(const QString &searchTerm)
{
    if (m_request.searchTerm() == searchTerm)
        return;

    m_request.setSearchTerm(searchTerm);
    emit searchTermChanged();
}

/*!
    \qmlproperty stringlist PlaceSearchSuggestionModel::suggestions

    This property holds the list of predicted search terms that the model currently has.
*/
QStringList QDeclarativeSearchSuggestionModel::suggestions() const
{
    return m_suggestions;
}

/*!
    \internal
*/
void QDeclarativeSearchSuggestionModel::clearData(bool suppressSignal)
{
    QDeclarativeSearchModelBase::clearData(suppressSignal);

    if (!m_suggestions.isEmpty()) {
        m_suggestions.clear();

        if (!suppressSignal)
            emit suggestionsChanged();
    }
}

/*!
    \internal
*/
int QDeclarativeSearchSuggestionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_suggestions.count();
}

/*!
    \internal
*/
QVariant QDeclarativeSearchSuggestionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rowCount(index.parent()) || index.row() < 0)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
    case SearchSuggestionRole:
        return m_suggestions.at(index.row());
    }

    return QVariant();
}

QHash<int, QByteArray> QDeclarativeSearchSuggestionModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QDeclarativeSearchModelBase::roleNames();
    roleNames.insert(SearchSuggestionRole, "suggestion");
    return roleNames;
}

/*!
    \internal
*/
void QDeclarativeSearchSuggestionModel::queryFinished()
{
    if (!m_reply)
        return;

    QPlaceReply *reply = m_reply;
    m_reply = 0;

    int initialCount = m_suggestions.count();
    beginResetModel();

    clearData(true);

    QPlaceSearchSuggestionReply *suggestionReply = qobject_cast<QPlaceSearchSuggestionReply *>(reply);
    m_suggestions = suggestionReply->suggestions();

    if (initialCount != m_suggestions.count())
        emit suggestionsChanged();

    endResetModel();

    if (suggestionReply->error() != QPlaceReply::NoError)
        setStatus(Error, suggestionReply->errorString());
    else
        setStatus(Ready);


    reply->deleteLater();
}

/*!
    \internal
*/
QPlaceReply *QDeclarativeSearchSuggestionModel::sendQuery(QPlaceManager *manager,
                                                        const QPlaceSearchRequest &request)
{
    return manager->searchSuggestions(request);
}
