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

#include "qplacemanagerengine.h"
#include "qplacemanagerengine_p.h"
#include "unsupportedreplies_p.h"

#include <QtCore/QMetaType>

#include "qplaceicon.h"

QT_BEGIN_NAMESPACE

/*!
    \class QPlaceManagerEngine
    \inmodule QtLocation
    \ingroup QtLocation-impl
    \ingroup QtLocation-places
    \ingroup QtLocation-places-manager
    \since 5.6

    \brief The QPlaceManagerEngine class provides an interface for
    implementers of QGeoServiceProvider plugins who want to provide access to place
    functionality.

    Application developers need not concern themselves with the QPlaceManagerEngine.
    Backend implementers however will need to derive from QPlaceManagerEngine and provide
    implementations for the abstract virtual functions.

    For more information on writing a backend see the \l {Places Backend} documentation.

    \sa QPlaceManager
*/

/*!
    Constructs a new engine with the specified \a parent, using \a parameters to pass any
    implementation specific data to the engine.
*/
QPlaceManagerEngine::QPlaceManagerEngine(const QVariantMap &parameters,
                                         QObject *parent)
:   QObject(parent), d_ptr(new QPlaceManagerEnginePrivate)
{
    qRegisterMetaType<QPlaceReply::Error>();
    qRegisterMetaType<QPlaceReply *>();
    Q_UNUSED(parameters)
}

/*!
    Destroys this engine.
*/
QPlaceManagerEngine::~QPlaceManagerEngine()
{
    delete d_ptr;
}

/*!
    \internal
    Sets the name which this engine implementation uses to distinguish itself
    from the implementations provided by other plugins to \a managerName.

    This function does not need to be called by engine implementers,
    it is implicitly called by QGeoServiceProvider to set the manager
    name to be the same as the provider's.
*/
void QPlaceManagerEngine::setManagerName(const QString &managerName)
{
    d_ptr->managerName = managerName;
}

/*!
    Returns the name which this engine implementation uses to distinguish
    itself from the implementations provided by other plugins.

    The manager name is automatically set to be the same
    as the QGeoServiceProviderFactory::providerName().
*/
QString QPlaceManagerEngine::managerName() const
{
    return d_ptr->managerName;
}

/*!
    \internal
    Sets the version of this engine implementation to \a managerVersion.

    The combination of managerName() and managerVersion() should be unique
    amongst plugin implementations.
*/
void QPlaceManagerEngine::setManagerVersion(int managerVersion)
{
    d_ptr->managerVersion = managerVersion;
}

/*!
    Returns the version of this engine implementation.

    The manager version is automatically set to be the same
    as the QGeoServiceProviderFactory::providerVersion().
*/
int QPlaceManagerEngine::managerVersion() const
{
    return d_ptr->managerVersion;
}

/*!
    Retrieves details of place corresponding to the given \a placeId.
*/
QPlaceDetailsReply *QPlaceManagerEngine::getPlaceDetails(const QString &placeId)
{
    Q_UNUSED(placeId)

    return new QPlaceDetailsReplyUnsupported(this);
}

/*!
    Retrieves content for a place according to the parameters specified in \a request.
*/
QPlaceContentReply *QPlaceManagerEngine::getPlaceContent(const QPlaceContentRequest &request)
{
    Q_UNUSED(request)

    return new QPlaceContentReplyUnsupported(this);
}

/*!
    Searches for places according to the parameters specified in \a request.
*/
QPlaceSearchReply *QPlaceManagerEngine::search(const QPlaceSearchRequest &request)
{
    Q_UNUSED(request)

    return new QPlaceSearchReplyUnsupported(QPlaceReply::UnsupportedError,
                                            QStringLiteral("Place search is not supported."), this);
}

/*!
    Requests a set of search term suggestions according to the parameters specified in \a request.
*/
QPlaceSearchSuggestionReply *QPlaceManagerEngine::searchSuggestions(
        const QPlaceSearchRequest &request)
{
    Q_UNUSED(request)

    return new QPlaceSearchSuggestionReplyUnsupported(this);
}

/*!
    Saves a specified \a place to the manager engine's datastore.
*/
QPlaceIdReply *QPlaceManagerEngine::savePlace(const QPlace &place)
{
    Q_UNUSED(place)

    return new QPlaceIdReplyUnsupported(QStringLiteral("Save place is not supported"),
                                        QPlaceIdReply::SavePlace, this);
}

/*!
    Removes the place corresponding to \a placeId from the manager engine's datastore.
*/
QPlaceIdReply *QPlaceManagerEngine::removePlace(const QString &placeId)
{
    Q_UNUSED(placeId)

    return new QPlaceIdReplyUnsupported(QStringLiteral("Remove place is not supported"),
                                        QPlaceIdReply::RemovePlace, this);
}

/*!
    Saves a \a category that is a child of the category specified by \a parentId.  An empty
    \a parentId means \a category is saved as a top level category.
*/
QPlaceIdReply *QPlaceManagerEngine::saveCategory(const QPlaceCategory &category,
                                                 const QString &parentId)
{
    Q_UNUSED(category)
    Q_UNUSED(parentId)

    return new QPlaceIdReplyUnsupported(QStringLiteral("Save category is not supported"),
                                        QPlaceIdReply::SaveCategory, this);
}

/*!
    Removes the category corresponding to \a categoryId from the manager engine's datastore.
*/

QPlaceIdReply *QPlaceManagerEngine::removeCategory(const QString &categoryId)
{
    Q_UNUSED(categoryId)

    return new QPlaceIdReplyUnsupported(QStringLiteral("Remove category is not supported"),
                                        QPlaceIdReply::RemoveCategory, this);
}

/*!
    Initializes the categories of the manager engine.
*/
QPlaceReply *QPlaceManagerEngine::initializeCategories()
{
    return new QPlaceReplyUnsupported(QStringLiteral("Categories are not supported."), this);
}

/*!
    Returns the parent category identifier of the category corresponding to \a categoryId.
*/
QString QPlaceManagerEngine::parentCategoryId(const QString &categoryId) const
{
    Q_UNUSED(categoryId)

    return QString();
}

/*!
    Returns the child category identifiers of the category corresponding to \a categoryId.  If
    \a categoryId is empty then all top level category identifiers are returned.
*/
QStringList QPlaceManagerEngine::childCategoryIds(const QString &categoryId) const
{
    Q_UNUSED(categoryId)

    return QStringList();
}

/*!
    Returns the category corresponding to the given \a categoryId.
*/
QPlaceCategory QPlaceManagerEngine::category(const QString &categoryId) const
{
    Q_UNUSED(categoryId)

    return QPlaceCategory();
}

/*!
    Returns a list of categories that are children of the category corresponding to \a parentId.
    If \a parentId is empty, all the top level categories are returned.
*/
QList<QPlaceCategory> QPlaceManagerEngine::childCategories(const QString &parentId) const
{
    Q_UNUSED(parentId)

    return QList<QPlaceCategory>();
}

/*!
    Returns a list of preferred locales. The locales are used as a hint to the manager engine for
    what language place and category details should be returned in.

    If the first specified locale cannot be accommodated, the manager engine falls back to the next
    and so forth.

    Support for locales may vary from provider to provider.  For those that do support it, by
    default, the \l {QLocale::setDefault()}{global default locale} will be used.  If the manager
    engine has no locales assigned to it, it implicitly uses the global default locale.  For
    engines that do not support locales, the locale list is always empty.
*/
QList<QLocale> QPlaceManagerEngine::locales() const
{
    return QList<QLocale>();
}

/*!
    Set the list of preferred \a locales.
*/
void QPlaceManagerEngine::setLocales(const QList<QLocale> &locales)
{
    Q_UNUSED(locales)
}

/*!
    Returns the manager instance used to create this engine.
*/
QPlaceManager *QPlaceManagerEngine::manager() const
{
    return d_ptr->manager;
}

/*!
    Returns a pruned or modified version of the \a original place
    which is suitable to be saved by the manager engine.

    Only place details that are supported by this manager is
    present in the modified version.  Manager specific data such
    as the place id, is not copied over from the \a original.
*/
QPlace QPlaceManagerEngine::compatiblePlace(const QPlace &original) const
{
    Q_UNUSED(original);
    return QPlace();
}

/*!
    Returns a reply which contains a list of places which correspond/match those
    specified in \a request.
*/
QPlaceMatchReply * QPlaceManagerEngine::matchingPlaces(const QPlaceMatchRequest &request)
{
    Q_UNUSED(request)

    return new QPlaceMatchReplyUnsupported(this);
}

/*!
    QUrl QPlaceManagerEngine::constructIconUrl(const QPlaceIcon &icon, const QSize &size)

    Constructs an icon url from a given \a icon, \a size.  The URL of the icon
    image that most closely matches the given parameters is returned.
*/
QUrl QPlaceManagerEngine::constructIconUrl(const QPlaceIcon &icon, const QSize &size) const
{
    Q_UNUSED(icon);
    Q_UNUSED(size);

    return QUrl();
}

QPlaceManagerEnginePrivate::QPlaceManagerEnginePrivate()
    :   managerVersion(-1), manager(0)
{
}

QPlaceManagerEnginePrivate::~QPlaceManagerEnginePrivate()
{
}

/*!
    \fn void QPlaceManagerEngine::finished(QPlaceReply *reply)

    This signal is emitted when \a reply has finished processing.

    If reply->error() equals QPlaceReply::NoError then the processing
    finished successfully.

    This signal and QPlaceReply::finished() will be emitted at the same time.

    \note Do not delete the \a reply object in the slot connected to this signal.
    Use deleteLater() instead.
*/

/*!
    \fn void QPlaceManagerEngine::error(QPlaceReply * reply, QPlaceReply::Error error, const QString &errorString = QString());

    This signal is emitted when an error has been detected in the processing of
    \a reply.  The QPlaceManager::finished() signal will probably follow.

    The error will be described by the error code \a error.  If \a errorString is
    not empty it will contain a textual description of the error meant for developers
    and not end users.

    This signal and QPlaceReply::error() will be emitted at the same time.

    \note Do not delete the \a reply object in the slot connected to this signal.
    Use deleteLater() instead.
*/

/*!
    \fn void QPlaceManagerEngine::placeAdded(const QString &placeId)

    This signal is emitted if a place has been added to the manager engine's datastore.
    The particular added place is specified by \a placeId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
    \fn void QPlaceManagerEngine::placeUpdated(const QString &placeId)

    This signal is emitted if a place has been modified in the manager engine's datastore.
    The particular modified place is specified by \a placeId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
    \fn void QPlaceManagerEngine::placeRemoved(const QString &placeId)

    This signal is emitted if a place has been removed from the manager engine's datastore.
    The particular place that has been removed is specified by \a placeId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
    \fn void QPlaceManagerEngine::categoryAdded(const QPlaceCategory &category, const QString &parentId)

    This signal is emitted if a \a category has been added to the manager engine's datastore.
    The parent of the \a category is specified by \a parentId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
    \fn void QPlaceManagerEngine::categoryUpdated(const QPlaceCategory &category, const QString &parentId)

    This signal is emitted if a \a category has been modified in the manager engine's datastore.
    The parent of the modified category is specified by \a parentId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
    \fn void QPlaceManagerEngine::categoryRemoved(const QString &categoryId, const QString &parentId)

    This signal is emitted when the category corresponding to \a categoryId has
    been removed from the manager engine's datastore.  The parent of the removed category
    is specified by \a parentId.

    This signal is only emitted by manager engines that support the QPlaceManager::NotificationsFeature.
    \sa dataChanged()
*/

/*!
 * \fn QPlaceManagerEngine::dataChanged()

    This signal is emitted by the engine if there are large scale changes to its
    underlying datastore and the engine considers these changes radical enough
    to require clients to reload all data.

    If the signal is emitted, no other signals will be emitted for the associated changes.
*/

QT_END_NAMESPACE

#include "moc_qplacemanagerengine.cpp"
