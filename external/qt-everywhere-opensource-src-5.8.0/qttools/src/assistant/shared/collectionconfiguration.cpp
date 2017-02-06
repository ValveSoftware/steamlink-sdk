/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "collectionconfiguration.h"

#include <QtHelp/QHelpEngineCore>

QT_BEGIN_NAMESPACE

namespace {
    const QString AboutIconKey(QLatin1String("AboutIcon"));
    const QString AboutImagesKey(QLatin1String("AboutImages"));
    const QString AboutMenuTextsKey(QLatin1String("AboutMenuTexts"));
    const QString AboutTextsKey(QLatin1String("AboutTexts"));
    const QString ApplicationIconKey(QLatin1String("ApplicationIcon"));
    const QString CacheDirKey(QLatin1String("CacheDirectory"));
    const QString CacheDirRelativeToCollectionKey(QLatin1String("CacheDirRelativeToCollection"));
    const QString CreationTimeKey(QLatin1String("CreationTime"));
    const QString DefaultHomePageKey(QLatin1String("defaultHomepage"));
    const QString EnableAddressBarKey(QLatin1String("EnableAddressBar"));
    const QString EnableDocManagerKey(QLatin1String("EnableDocumentationManager"));
    const QString EnableFilterKey(QLatin1String("EnableFilterFunctionality"));
    const QString HideAddressBarKey(QLatin1String("HideAddressBar"));
    const QString FilterToolbarHiddenKey(QLatin1String("HideFilterFunctionality"));
    const QString LastPageKey(QLatin1String("LastTabPage"));
    const QString LastRegisterTime(QLatin1String("LastRegisterTime"));
    const QString LastShownPagesKey(QLatin1String("LastShownPages"));
    const QString LastZoomFactorsKey(QLatin1String(
#if defined(BROWSER_QTWEBKIT)
            "LastPagesZoomWebView"
#else
            "LastPagesZoomTextBrowser"
#endif
            ));
    const QString WindowTitleKey(QLatin1String("WindowTitle"));
    const QString FullTextSearchFallbackKey(QLatin1String("FullTextSearchFallback"));
} // anonymous namespace

const QString CollectionConfiguration::DefaultZoomFactor(QLatin1String("0.0"));
const QString CollectionConfiguration::ListSeparator(QLatin1String("|"));

uint CollectionConfiguration::creationTime(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(CreationTimeKey, 0).toUInt();
}

void CollectionConfiguration::setCreationTime(QHelpEngineCore &helpEngine, uint time)
{
    helpEngine.setCustomValue(CreationTimeKey, time);
}

const QString CollectionConfiguration::windowTitle(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(WindowTitleKey).toString();
}

void CollectionConfiguration::setWindowTitle(QHelpEngineCore &helpEngine,
                                             const QString &windowTitle)
{
    helpEngine.setCustomValue(WindowTitleKey, windowTitle);
}

bool CollectionConfiguration::filterFunctionalityEnabled(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(EnableFilterKey, true).toBool();
}

void CollectionConfiguration::setFilterFunctionalityEnabled(QHelpEngineCore &helpEngine,
                                                            bool enabled)
{
    helpEngine.setCustomValue(EnableFilterKey, enabled);
}

bool CollectionConfiguration::filterToolbarVisible(const QHelpEngineCore &helpEngine)
{
    return !helpEngine.customValue(FilterToolbarHiddenKey, true).toBool();
}

void CollectionConfiguration::setFilterToolbarVisible(QHelpEngineCore &helpEngine,
                                                      bool visible)
{
    helpEngine.setCustomValue(FilterToolbarHiddenKey, !visible);
}

bool CollectionConfiguration::addressBarEnabled(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(EnableAddressBarKey, true).toBool();
}

void CollectionConfiguration::setAddressBarEnabled(QHelpEngineCore &helpEngine,
                                                   bool enabled)
{
    helpEngine.setCustomValue(EnableAddressBarKey, enabled);
}

bool CollectionConfiguration::addressBarVisible(const QHelpEngineCore &helpEngine)
{
    return !helpEngine.customValue(HideAddressBarKey, true).toBool();
}

void CollectionConfiguration::setAddressBarVisible(QHelpEngineCore &helpEngine,
                                                   bool visible)
{
    helpEngine.setCustomValue(HideAddressBarKey, !visible);
}

const QString CollectionConfiguration::cacheDir(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(CacheDirKey).toString();
}

bool CollectionConfiguration::cacheDirIsRelativeToCollection(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(CacheDirRelativeToCollectionKey).toBool();
}

void CollectionConfiguration::setCacheDir(QHelpEngineCore &helpEngine,
                            const QString &cacheDir, bool relativeToCollection)
{
    helpEngine.setCustomValue(CacheDirKey, cacheDir);
    helpEngine.setCustomValue(CacheDirRelativeToCollectionKey,
                              relativeToCollection);
}

bool CollectionConfiguration::documentationManagerEnabled(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(EnableDocManagerKey, true).toBool();
}

void CollectionConfiguration::setDocumentationManagerEnabled(QHelpEngineCore &helpEngine,
                                                             bool enabled)
{
    helpEngine.setCustomValue(EnableDocManagerKey, enabled);
}

const QByteArray CollectionConfiguration::applicationIcon(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(ApplicationIconKey).toByteArray();
}

void CollectionConfiguration::setApplicationIcon(QHelpEngineCore &helpEngine,
                                                 const QByteArray &icon)
{
    helpEngine.setCustomValue(ApplicationIconKey, icon);
}

const QByteArray CollectionConfiguration::aboutMenuTexts(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(AboutMenuTextsKey).toByteArray();
}

void CollectionConfiguration::setAboutMenuTexts(QHelpEngineCore &helpEngine,
                                                const QByteArray &texts)
{
    helpEngine.setCustomValue(AboutMenuTextsKey, texts);
}

const QByteArray CollectionConfiguration::aboutIcon(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(AboutIconKey).toByteArray();
}

void CollectionConfiguration::setAboutIcon(QHelpEngineCore &helpEngine,
                                           const QByteArray &icon)
{
    helpEngine.setCustomValue(AboutIconKey, icon);
}

const QByteArray CollectionConfiguration::aboutTexts(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(AboutTextsKey).toByteArray();
}

void CollectionConfiguration::setAboutTexts(QHelpEngineCore &helpEngine,
                                            const QByteArray &texts)
{
    helpEngine.setCustomValue(AboutTextsKey, texts);
}

const QByteArray CollectionConfiguration::aboutImages(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(AboutImagesKey).toByteArray();
}

void CollectionConfiguration::setAboutImages(QHelpEngineCore &helpEngine,
                                             const QByteArray &images)
{
    helpEngine.setCustomValue(AboutImagesKey, images);
}

const QString CollectionConfiguration::defaultHomePage(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(DefaultHomePageKey, QLatin1String("help")).
        toString();
}

void CollectionConfiguration::setDefaultHomePage(QHelpEngineCore &helpEngine,
                                                 const QString &page)
{
    helpEngine.setCustomValue(DefaultHomePageKey, page);
}

const QStringList CollectionConfiguration::lastShownPages(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(LastShownPagesKey).toString().
        split(ListSeparator, QString::SkipEmptyParts);
}

void CollectionConfiguration::setLastShownPages(QHelpEngineCore &helpEngine,
                                             const QStringList &lastShownPages)
{
    helpEngine.setCustomValue(LastShownPagesKey,
                              lastShownPages.join(ListSeparator));
}

const QStringList CollectionConfiguration::lastZoomFactors(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(LastZoomFactorsKey).toString().
        split(ListSeparator, QString::SkipEmptyParts);
}

void CollectionConfiguration::setLastZoomFactors(QHelpEngineCore &helpEngine,
                                             const QStringList &lastZoomFactors)
{
    helpEngine.setCustomValue(LastZoomFactorsKey,
                              lastZoomFactors.join(ListSeparator));
}

int CollectionConfiguration::lastTabPage(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(LastPageKey, 1).toInt();
}

void CollectionConfiguration::setLastTabPage(QHelpEngineCore &helpEngine,
                                             int lastPage)
{
    helpEngine.setCustomValue(LastPageKey, lastPage);
}

const QDateTime CollectionConfiguration::lastRegisterTime(const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(LastRegisterTime, QDateTime()).toDateTime();
}

void CollectionConfiguration::updateLastRegisterTime(QHelpEngineCore &helpEngine)
{
    helpEngine.setCustomValue(LastRegisterTime, QDateTime::currentDateTime());
}

bool CollectionConfiguration::isNewer(const QHelpEngineCore &newer,
                                const QHelpEngineCore &older)
{
    return creationTime(newer) > creationTime(older);
}

void CollectionConfiguration::copyConfiguration(const QHelpEngineCore &source,
                                                QHelpEngineCore &target)
{
    setCreationTime(target, creationTime(source));
    setWindowTitle(target, windowTitle(source));
    target.setCurrentFilter(source.currentFilter());
    setCacheDir(target, cacheDir(source), cacheDirIsRelativeToCollection(source));
    setFilterFunctionalityEnabled(target, filterFunctionalityEnabled(source));
    setFilterToolbarVisible(target, filterToolbarVisible(source));
    setAddressBarEnabled(target, addressBarEnabled(source));
    setAddressBarVisible(target, addressBarVisible(source));
    setDocumentationManagerEnabled(target, documentationManagerEnabled(source));
    setApplicationIcon(target, applicationIcon(source));
    setAboutMenuTexts(target, aboutMenuTexts(source));
    setAboutIcon(target, aboutIcon(source));
    setAboutTexts(target, aboutTexts(source));
    setAboutImages(target, aboutImages(source));
    setDefaultHomePage(target, defaultHomePage(source));
    setFullTextSearchFallbackEnabled(target, fullTextSearchFallbackEnabled(source));
}

bool CollectionConfiguration:: fullTextSearchFallbackEnabled(
    const QHelpEngineCore &helpEngine)
{
    return helpEngine.customValue(FullTextSearchFallbackKey, false).toBool();
}

void CollectionConfiguration::setFullTextSearchFallbackEnabled(
    QHelpEngineCore &helpEngine, bool on)
{
    helpEngine.setCustomValue(FullTextSearchFallbackKey, on);
}

QT_END_NAMESPACE
