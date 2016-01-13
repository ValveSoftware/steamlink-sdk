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

#ifndef QDECLARATIVESEARCHRESULTMODEL_P_H
#define QDECLARATIVESEARCHRESULTMODEL_P_H

#include "qdeclarativesearchmodelbase.h"
#include "qdeclarativecategory_p.h"
#include "qdeclarativeplace_p.h"
#include "qdeclarativeplaceicon_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeGeoServiceProvider;

class QDeclarativeSearchResultModel : public QDeclarativeSearchModelBase
{
    Q_OBJECT

    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(QQmlListProperty<QDeclarativeCategory> categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QString recommendationId READ recommendationId WRITE setRecommendationId NOTIFY recommendationIdChanged)
    Q_PROPERTY(RelevanceHint relevanceHint READ relevanceHint WRITE setRelevanceHint NOTIFY relevanceHintChanged)
    Q_PROPERTY(QDeclarativePlace::Visibility visibilityScope READ visibilityScope WRITE setVisibilityScope NOTIFY visibilityScopeChanged)

    Q_PROPERTY(int count READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(QDeclarativeGeoServiceProvider *favoritesPlugin READ favoritesPlugin WRITE setFavoritesPlugin NOTIFY favoritesPluginChanged)
    Q_PROPERTY(QVariantMap favoritesMatchParameters READ favoritesMatchParameters WRITE setFavoritesMatchParameters NOTIFY favoritesMatchParametersChanged)

    Q_ENUMS(SearchResultType RelevanceHint)

public:
    enum SearchResultType {
        UnknownSearchResult = QPlaceSearchResult::UnknownSearchResult,
        PlaceResult = QPlaceSearchResult::PlaceResult,
        ProposedSearchResult = QPlaceSearchResult::ProposedSearchResult
    };

    enum RelevanceHint {
        UnspecifiedHint = QPlaceSearchRequest::UnspecifiedHint,
        DistanceHint = QPlaceSearchRequest::DistanceHint,
        LexicalPlaceNameHint = QPlaceSearchRequest::LexicalPlaceNameHint
    };

    explicit QDeclarativeSearchResultModel(QObject *parent = 0);
    ~QDeclarativeSearchResultModel();

    QString searchTerm() const;
    void setSearchTerm(const QString &searchTerm);

    QQmlListProperty<QDeclarativeCategory> categories();
    static void categories_append(QQmlListProperty<QDeclarativeCategory> *list,
                                  QDeclarativeCategory *category);
    static int categories_count(QQmlListProperty<QDeclarativeCategory> *list);
    static QDeclarativeCategory *category_at(QQmlListProperty<QDeclarativeCategory> *list, int index);
    static void categories_clear(QQmlListProperty<QDeclarativeCategory> *list);

    QString recommendationId() const;
    void setRecommendationId(const QString &recommendationId);

    QDeclarativeSearchResultModel::RelevanceHint relevanceHint() const;
    void setRelevanceHint(QDeclarativeSearchResultModel::RelevanceHint hint);

    QDeclarativePlace::Visibility visibilityScope() const;
    void setVisibilityScope(QDeclarativePlace::Visibility visibilityScope);

    QDeclarativeGeoServiceProvider *favoritesPlugin() const;
    void setFavoritesPlugin(QDeclarativeGeoServiceProvider *plugin);

    QVariantMap favoritesMatchParameters() const;
    void setFavoritesMatchParameters(const QVariantMap &parameters);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    virtual void clearData(bool suppressSignal = false);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Q_INVOKABLE QVariant data(int index, const QString &roleName) const;
    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void updateWith(int proposedSearchIndex);

    void updateSearchRequest();

Q_SIGNALS:
    void searchTermChanged();
    void categoriesChanged();
    void recommendationIdChanged();
    void relevanceHintChanged();
    void visibilityScopeChanged();

    void rowCountChanged();
    void favoritesPluginChanged();
    void favoritesMatchParametersChanged();
    void dataChanged();

protected:
    QPlaceReply *sendQuery(QPlaceManager *manager, const QPlaceSearchRequest &request);
    virtual void initializePlugin(QDeclarativeGeoServiceProvider *plugin);

protected Q_SLOTS:
    virtual void queryFinished();

private Q_SLOTS:
    void updateLayout(const QList<QPlace> &favoritePlaces = QList<QPlace>());

    void placeUpdated(const QString &placeId);
    void placeRemoved(const QString &placeId);

private:
    enum Roles {
        SearchResultTypeRole = Qt::UserRole,
        TitleRole,
        IconRole,
        DistanceRole,
        PlaceRole,
        SponsoredRole
    };

    int getRow(const QString &placeId) const;
    QList<QDeclarativeCategory *> m_categories;
    QLocation::VisibilityScope m_visibilityScope;

    QList<QPlaceSearchResult> m_results;
    QList<QPlaceSearchResult> m_resultsBuffer;
    QList<QDeclarativePlace *> m_places;
    QList<QDeclarativePlaceIcon *> m_icons;

    QDeclarativeGeoServiceProvider *m_favoritesPlugin;
    QVariantMap m_matchParameters;
};

QT_END_NAMESPACE

#endif // QDECLARATIVESEARCHRESULTMODEL_P_H
