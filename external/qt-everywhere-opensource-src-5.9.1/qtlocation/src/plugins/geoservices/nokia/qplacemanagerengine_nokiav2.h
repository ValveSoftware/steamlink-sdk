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

#ifndef QPLACEMANAGERENGINE_NOKIAV2_H
#define QPLACEMANAGERENGINE_NOKIAV2_H

#include <QtCore/QPointer>
#include <QtNetwork/QNetworkReply>
#include <QtLocation/QPlaceManagerEngine>
#include <QtLocation/QGeoServiceProvider>

QT_BEGIN_NAMESPACE

class QPlaceContentReply;
class QNetworkReply;
class QNetworkAccessManager;
class QPlaceCategoriesReplyHere;
class QGeoNetworkAccessManager;
class QGeoUriProvider;

struct PlaceCategoryNode
{
    QString parentId;
    QStringList childIds;
    QPlaceCategory category;
};

typedef QMap<QString, PlaceCategoryNode> QPlaceCategoryTree;

class QPlaceManagerEngineNokiaV2 : public QPlaceManagerEngine
{
    Q_OBJECT

public:
    QPlaceManagerEngineNokiaV2(QGeoNetworkAccessManager *networkManager,
                               const QVariantMap &parameters,
                               QGeoServiceProvider::Error *error,
                               QString *errorString);
    ~QPlaceManagerEngineNokiaV2();

    QPlaceDetailsReply *getPlaceDetails(const QString &placeId) Q_DECL_OVERRIDE;

    QPlaceContentReply *getPlaceContent(const QPlaceContentRequest &request) Q_DECL_OVERRIDE;

    QPlaceSearchReply *search(const QPlaceSearchRequest &query) Q_DECL_OVERRIDE;

    QPlaceSearchSuggestionReply *searchSuggestions(const QPlaceSearchRequest &query) Q_DECL_OVERRIDE;

    QPlaceIdReply *savePlace(const QPlace &place) Q_DECL_OVERRIDE;
    QPlaceIdReply *removePlace(const QString &placeId) Q_DECL_OVERRIDE;

    QPlaceIdReply *saveCategory(const QPlaceCategory &category, const QString &parentId) Q_DECL_OVERRIDE;
    QPlaceIdReply *removeCategory(const QString &categoryId) Q_DECL_OVERRIDE;

    QPlaceReply *initializeCategories() Q_DECL_OVERRIDE;
    QString parentCategoryId(const QString &categoryId) const Q_DECL_OVERRIDE;
    QStringList childCategoryIds(const QString &categoryId) const Q_DECL_OVERRIDE;
    QPlaceCategory category(const QString &categoryId) const Q_DECL_OVERRIDE;
    QList<QPlaceCategory> childCategories(const QString &parentId) const Q_DECL_OVERRIDE;

    QList<QLocale> locales() const Q_DECL_OVERRIDE;
    void setLocales(const QList<QLocale> &locales) Q_DECL_OVERRIDE;

    QPlaceIcon icon(const QString &remotePath,
                    const QList<QPlaceCategory> &categories = QList<QPlaceCategory>()) const;

    QUrl constructIconUrl(const QPlaceIcon &icon, const QSize &size) const Q_DECL_OVERRIDE;

private:
    QNetworkReply *sendRequest(const QUrl &url);
    QByteArray createLanguageString() const;

private Q_SLOTS:
    void replyFinished();
    void replyError(QPlaceReply::Error error_, const QString &errorString);
    void categoryReplyFinished();
    void categoryReplyError();

private:
    QGeoNetworkAccessManager *m_manager;
    QGeoUriProvider *m_uriProvider;

    QList<QLocale> m_locales;

    QPlaceCategoryTree m_categoryTree;
    QPlaceCategoryTree m_tempTree;
    QHash<QString, QString> m_restIdToIconHash;

    QPointer<QPlaceCategoriesReplyHere> m_categoryReply;
    QHash<QString, QNetworkReply *> m_categoryRequests;

    QString m_appId;
    QString m_appCode;

    QString m_localDataPath;
    QString m_theme;
};

QT_END_NAMESPACE

#endif // QPLACEMANAGERENGINE_NOKIAV2_H
