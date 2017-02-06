/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "qquickwebenginefaviconprovider_p_p.h"

#include "favicon_manager.h"
#include "qquickwebengineview_p.h"
#include "qquickwebengineview_p_p.h"
#include "web_contents_adapter.h"

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

QT_BEGIN_NAMESPACE

using QtWebEngineCore::FaviconInfo;
using QtWebEngineCore::FaviconManager;

static inline unsigned area(const QSize &size)
{
    return size.width() * size.height();
}

QString QQuickWebEngineFaviconProvider::identifier()
{
    return QStringLiteral("favicon");
}

QUrl QQuickWebEngineFaviconProvider::faviconProviderUrl(const QUrl &url)
{
    if (url.isEmpty())
        return url;

    QUrl providerUrl;
    providerUrl.setScheme(QStringLiteral("image"));
    providerUrl.setHost(identifier());
    providerUrl.setPath(QStringLiteral("/%1").arg(url.toString()));

    return providerUrl;
}

QQuickWebEngineFaviconProvider::QQuickWebEngineFaviconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_latestView(0)
{
}

QQuickWebEngineFaviconProvider::~QQuickWebEngineFaviconProvider()
{
    qDeleteAll(m_iconUrlMap);
}

QUrl QQuickWebEngineFaviconProvider::attach(QQuickWebEngineView *view, const QUrl &iconUrl)
{
    if (iconUrl.isEmpty())
        return QUrl();

    m_latestView = view;

    if (!m_iconUrlMap.contains(view))
        m_iconUrlMap.insert(view, new QList<QUrl>());

    QList<QUrl> *iconUrls = m_iconUrlMap[view];
    if (!iconUrls->contains(iconUrl))
        iconUrls->append(iconUrl);

    return faviconProviderUrl(iconUrl);
}

void QQuickWebEngineFaviconProvider::detach(QQuickWebEngineView *view)
{
    QList<QUrl> *iconUrls = m_iconUrlMap.take(view);
    delete iconUrls;
}

QPixmap QQuickWebEngineFaviconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size);
    Q_UNUSED(requestedSize);

    QUrl iconUrl(id);
    QQuickWebEngineView *view = viewForIconUrl(iconUrl);

    if (!view || iconUrl.isEmpty())
        return QPixmap();

    FaviconManager *faviconManager = view->d_ptr->adapter->faviconManager();

    Q_ASSERT(faviconManager);
    const FaviconInfo &faviconInfo = faviconManager->getFaviconInfo(iconUrl);
    const QIcon &icon = faviconManager->getIcon(faviconInfo.candidate ? QUrl() : iconUrl);

    Q_ASSERT(!icon.isNull());
    const QSize &bestSize = faviconInfo.size;

    // If source size is not specified, use the best quality
    if (!requestedSize.isValid()) {
        if (size)
            *size = bestSize;

        return icon.pixmap(bestSize).copy();
    }

    const QSize &fitSize = findFitSize(icon.availableSizes(), requestedSize, bestSize);
    const QPixmap &iconPixmap = icon.pixmap(fitSize);

    if (size)
        *size = iconPixmap.size();

    return iconPixmap.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation).copy();
}

QQuickWebEngineView *QQuickWebEngineFaviconProvider::viewForIconUrl(const QUrl &iconUrl) const
{
    // The most common use case is that the requested iconUrl belongs to the
    // latest WebEngineView which was raised an iconChanged signal.
    if (m_latestView) {
        QList<QUrl> *iconUrls = m_iconUrlMap[m_latestView];
        if (iconUrls && iconUrls->contains(iconUrl))
            return m_latestView;
    }

    for (auto it = m_iconUrlMap.cbegin(), end = m_iconUrlMap.cend(); it != end; ++it) {
        if (it.value()->contains(iconUrl))
            return it.key();
    }

    return 0;
}

QSize QQuickWebEngineFaviconProvider::findFitSize(const QList<QSize> &availableSizes,
                                                  const QSize &requestedSize,
                                                  const QSize &bestSize) const
{
    Q_ASSERT(availableSizes.count());
    if (availableSizes.count() == 1 || area(requestedSize) >= area(bestSize))
        return bestSize;

    QSize fitSize = bestSize;
    for (const QSize &size : availableSizes) {
        if (area(size) == area(requestedSize))
            return size;

        if (area(requestedSize) < area(size) && area(size) < area(fitSize))
            fitSize = size;
    }

    return fitSize;
}

QT_END_NAMESPACE
