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

#ifndef COLLECTIONCONFIGURATION_H
#define COLLECTIONCONFIGURATION_H

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE

class QHelpEngineCore;

class CollectionConfiguration
{
public:
    static const QString windowTitle(const QHelpEngineCore &helpEngine);
    static void setWindowTitle(QHelpEngineCore &helpEngine,
                               const QString &windowTitle);

    static const QString cacheDir(const QHelpEngineCore &helpEngine);
    static bool cacheDirIsRelativeToCollection(const QHelpEngineCore &helpEngine);
    static void setCacheDir(QHelpEngineCore &helpEngine,
                            const QString &cacheDir, bool relativeToCollection);

    static uint creationTime(const QHelpEngineCore &helpEngine);
    static void setCreationTime(QHelpEngineCore &helpEngine, uint time);

    static bool filterFunctionalityEnabled(const QHelpEngineCore &helpEngine);
    static void setFilterFunctionalityEnabled(QHelpEngineCore &helpEngine,
                                              bool enabled);

    static bool filterToolbarVisible(const QHelpEngineCore &helpEngine);
    static void setFilterToolbarVisible(QHelpEngineCore &helpEngine,
                                        bool visible);

    static bool addressBarEnabled(const QHelpEngineCore &helpEngine);
    static void setAddressBarEnabled(QHelpEngineCore &helpEngine, bool enabled);

    static bool addressBarVisible(const QHelpEngineCore &helpEngine);
    static void setAddressBarVisible(QHelpEngineCore &helpEngine, bool visible);


    static bool documentationManagerEnabled(const QHelpEngineCore &helpEngine);
    static void setDocumentationManagerEnabled(QHelpEngineCore &helpEngine,
                                               bool enabled);

    static const QByteArray applicationIcon(const QHelpEngineCore &helpEngine);
    static void setApplicationIcon(QHelpEngineCore &helpEngine,
                                   const QByteArray &icon);

    // TODO: Encapsulate encoding from/to QByteArray here
    static const QByteArray aboutMenuTexts(const QHelpEngineCore &helpEngine);
    static void setAboutMenuTexts(QHelpEngineCore &helpEngine,
                                  const QByteArray &texts);

    static const QByteArray aboutIcon(const QHelpEngineCore &helpEngine);
    static void setAboutIcon(QHelpEngineCore &helpEngine,
                             const QByteArray &icon);

    // TODO: Encapsulate encoding from/to QByteArray here
    static const QByteArray aboutTexts(const QHelpEngineCore &helpEngine);
    static void setAboutTexts(QHelpEngineCore &helpEngine,
                              const QByteArray &texts);

    static const QByteArray aboutImages(const QHelpEngineCore &helpEngine);
    static void setAboutImages(QHelpEngineCore &helpEngine,
                               const QByteArray &images);

    static const QString defaultHomePage(const QHelpEngineCore &helpEngine);
    static void setDefaultHomePage(QHelpEngineCore &helpEngine,
                                   const QString &page);

    // TODO: Don't allow last pages and zoom factors to be set in isolation
    //       Perhaps also fill up missing elements automatically or assert.
    static const QStringList lastShownPages(const QHelpEngineCore &helpEngine);
    static void setLastShownPages(QHelpEngineCore &helpEngine,
                                  const QStringList &lastShownPages);
    static const QStringList lastZoomFactors(const QHelpEngineCore &helpEngine);
    static void setLastZoomFactors(QHelpEngineCore &helPEngine,
                                   const QStringList &lastZoomFactors);

    static int lastTabPage(const QHelpEngineCore &helpEngine);
    static void setLastTabPage(QHelpEngineCore &helpEngine, int lastPage);

    static bool isNewer(const QHelpEngineCore &newer,
                        const QHelpEngineCore &older);
    static void copyConfiguration(const QHelpEngineCore &source,
                                  QHelpEngineCore &target);

    /*
     * Note that this only reflects register actions caused by the
     * "-register" command line switch, not GUI or remote control actions.
     */
    static const QDateTime lastRegisterTime(const QHelpEngineCore &helpEngine);
    static void updateLastRegisterTime(QHelpEngineCore &helpEngine);

    static bool fullTextSearchFallbackEnabled(const QHelpEngineCore &helpEngine);
    static void setFullTextSearchFallbackEnabled(QHelpEngineCore &helpEngine,
        bool on);

    static const QString DefaultZoomFactor;
    static const QString ListSeparator;
};

QT_END_NAMESPACE

#endif // COLLECTIONCONFIGURATION_H
