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

#include "favicon_manager.h"
#include "favicon_manager_p.h"

#include "type_conversion.h"
#include "web_contents_adapter_client.h"
#include "web_engine_settings.h"

#include "base/bind.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/geometry/size.h"

namespace QtWebEngineCore {

static inline bool isResourceUrl(const QUrl &url)
{
    return !url.scheme().compare(QLatin1String("qrc"));
}

static inline unsigned area(const QSize &size)
{
    return size.width() * size.height();
}


FaviconManagerPrivate::FaviconManagerPrivate(content::WebContents *webContents, WebContentsAdapterClient *viewClient)
    : m_webContents(webContents)
    , m_viewClient(viewClient)
    , m_weakFactory(this)
{
}

FaviconManagerPrivate::~FaviconManagerPrivate()
{
}

int FaviconManagerPrivate::downloadIcon(const QUrl &url)
{
    static int fakeId = 0;
    int id;

    bool cached = m_icons.contains(url);
    if (isResourceUrl(url) || cached) {
        id = --fakeId;
        m_pendingRequests.insert(id, url);
    } else {
        id = m_webContents->DownloadImage(
             toGurl(url),
             true, // is_favicon
             0,    // no max size
             false, // normal cache policy
             base::Bind(&FaviconManagerPrivate::iconDownloadFinished, m_weakFactory.GetWeakPtr()));
    }

    Q_ASSERT(!m_inProgressRequests.contains(id));
    m_inProgressRequests.insert(id, url);

    return id;
}

void FaviconManagerPrivate::iconDownloadFinished(int id,
                                                 int status,
                                                 const GURL &url,
                                                 const std::vector<SkBitmap> &bitmaps,
                                                 const std::vector<gfx::Size> &original_bitmap_sizes)
{
    Q_UNUSED(status);
    Q_UNUSED(url);
    Q_UNUSED(original_bitmap_sizes);

    storeIcon(id, toQIcon(bitmaps));
}

/* Pending requests are used to mark icons that are already downloaded (cached icons or icons
 * stored in qrc). These requests are also stored in the m_inProgressRequests but the corresponding
 * icons are stored in m_icons explicitly by this function. It is necessary to avoid
 * m_inProgressRequests being emptied right before the next icon is added by a downloadIcon() call.
 */
void FaviconManagerPrivate::downloadPendingRequests()
{
    for (auto it = m_pendingRequests.cbegin(), end = m_pendingRequests.cend(); it != end; ++it) {
        QIcon icon;

        QUrl requestUrl = it.value();
        if (isResourceUrl(requestUrl) && !m_icons.contains(requestUrl))
            icon = QIcon(requestUrl.toString().remove(0, 3));

        storeIcon(it.key(), icon);
    }

    m_pendingRequests.clear();
}

void FaviconManagerPrivate::storeIcon(int id, const QIcon &icon)
{
    Q_Q(FaviconManager);

    // Icon download has been interrupted
    if (m_inProgressRequests.isEmpty())
        return;

    Q_ASSERT(m_inProgressRequests.contains(id));

    QUrl requestUrl = m_inProgressRequests[id];
    FaviconInfo &faviconInfo = q->m_faviconInfoMap[requestUrl];

    unsigned iconCount = 0;
    if (!icon.isNull())
        iconCount = icon.availableSizes().count();

    if (iconCount > 0) {
        m_icons.insert(requestUrl, icon);

        faviconInfo.size = icon.availableSizes().at(0);
        if (iconCount > 1) {
            faviconInfo.multiSize = true;
            unsigned bestArea = area(faviconInfo.size);
            for (unsigned i = 1; i < iconCount; ++i) {
                QSize iconSize = icon.availableSizes().at(i);
                if (bestArea < area(iconSize)) {
                    faviconInfo.size = iconSize;
                    bestArea = area(iconSize);
                }
            }
        }
    } else if (id >= 0) {
        // Reset size if icon cannot be downloaded
        faviconInfo.size = QSize(0, 0);
    }

    m_inProgressRequests.remove(id);
    if (m_inProgressRequests.isEmpty()) {
        WebEngineSettings *settings = m_viewClient->webEngineSettings();
        bool touchIconsEnabled = settings->testAttribute(WebEngineSettings::TouchIconsEnabled);

        q->generateCandidateIcon(touchIconsEnabled);
        const QUrl &iconUrl = q->candidateIconUrl(touchIconsEnabled);
        propagateIcon(iconUrl);
    }
}

void FaviconManagerPrivate::propagateIcon(const QUrl &iconUrl) const
{
    content::NavigationEntry *entry = m_webContents->GetController().GetVisibleEntry();
    if (entry) {
        content::FaviconStatus &favicon = entry->GetFavicon();
        favicon.url = toGurl(iconUrl);
        favicon.valid = true;
    }

    m_viewClient->iconChanged(iconUrl);
}

FaviconManager::FaviconManager(FaviconManagerPrivate *d)
    : m_candidateCount(0)
{
    Q_ASSERT(d);
    d_ptr.reset(d);

    d->q_ptr = this;
}

FaviconManager::~FaviconManager()
{
}

QIcon FaviconManager::getIcon(const QUrl &url) const
{
    Q_D(const FaviconManager);

    if (url.isEmpty())
        return m_candidateIcon;

    if (!d->m_icons.contains(url))
        return QIcon();

    return d->m_icons[url];
}

FaviconInfo FaviconManager::getFaviconInfo(const QUrl &url) const
{
    Q_ASSERT(m_faviconInfoMap.contains(url));
    return m_faviconInfoMap[url];
}

QList<FaviconInfo> FaviconManager::getFaviconInfoList(bool candidatesOnly) const
{
    QList<FaviconInfo> faviconInfoList = m_faviconInfoMap.values();

    if (candidatesOnly) {
        QMutableListIterator<FaviconInfo> it(faviconInfoList);
        while (it.hasNext()) {
            if (!it.next().candidate)
                it.remove();
        }
    }

    return faviconInfoList;
}

void FaviconManager::update(const QList<FaviconInfo> &candidates)
{
    Q_D(FaviconManager);
    updateCandidates(candidates);

    WebEngineSettings *settings = d->m_viewClient->webEngineSettings();
    if (!settings->testAttribute(WebEngineSettings::AutoLoadIconsForPage)) {
        d->m_viewClient->iconChanged(QUrl());
        return;
    }

    bool touchIconsEnabled = settings->testAttribute(WebEngineSettings::TouchIconsEnabled);

    const QList<FaviconInfo> &faviconInfoList = getFaviconInfoList(true /* candidates only */);
    for (auto it = faviconInfoList.cbegin(), end = faviconInfoList.cend(); it != end; ++it) {
        if (!touchIconsEnabled && it->type != FaviconInfo::Favicon)
            continue;

        if (it->isValid())
            d->downloadIcon(it->url);
    }

    d->downloadPendingRequests();

    // Reset icon if nothing was downloaded
    if (d->m_inProgressRequests.isEmpty()) {
        content::NavigationEntry *entry = d->m_webContents->GetController().GetVisibleEntry();
        if (entry && !entry->GetFavicon().valid)
            d->m_viewClient->iconChanged(QUrl());
    }
}

void FaviconManager::updateCandidates(const QList<FaviconInfo> &candidates)
{
    m_candidateCount = candidates.count();
    for (FaviconInfo candidateFaviconInfo : candidates) {
        const QUrl &candidateUrl = candidateFaviconInfo.url;

        if (!m_faviconInfoMap.contains(candidateUrl))
            m_faviconInfoMap.insert(candidateUrl, candidateFaviconInfo);
        else {
            // The same icon can be used for more than one page with different types.
            m_faviconInfoMap[candidateUrl].type = candidateFaviconInfo.type;
        }

        m_faviconInfoMap[candidateUrl].candidate = true;
    }
}

void FaviconManager::resetCandidates()
{
    Q_D(FaviconManager);

    // Interrupt in progress icon downloads
    d->m_pendingRequests.clear();
    d->m_inProgressRequests.clear();

    m_candidateCount = 0;
    m_candidateIcon = QIcon();
    for (auto it = m_faviconInfoMap.begin(), end = m_faviconInfoMap.end(); it != end; ++it)
        it->candidate = false;
}

bool FaviconManager::hasCandidate() const
{
    return (m_candidateCount > 0);
}

QUrl FaviconManager::candidateIconUrl(bool touchIconsEnabled) const
{
    QUrl iconUrl;
    const QList<FaviconInfo> &faviconInfoList = getFaviconInfoList(true /* candidates only */);

    unsigned bestArea = 0;
    for (auto it = faviconInfoList.cbegin(), end = faviconInfoList.cend(); it != end; ++it) {
        if (!touchIconsEnabled && it->type != FaviconInfo::Favicon)
            continue;

        if (it->isValid() && bestArea < area(it->size)) {
            iconUrl = it->url;
            bestArea = area(it->size);
        }
    }

    return iconUrl;
}

void FaviconManager::generateCandidateIcon(bool touchIconsEnabled)
{
    Q_ASSERT(m_candidateCount);

    m_candidateIcon = QIcon();
    const QList<FaviconInfo> &faviconInfoList = getFaviconInfoList(true /* candidates only */);

    for (auto it = faviconInfoList.cbegin(), end = faviconInfoList.cend(); it != end; ++it) {
        if (!touchIconsEnabled && it->type != FaviconInfo::Favicon)
            continue;

        if (!it->isValid() || !it->isDownloaded())
            continue;

        const QIcon &icon = getIcon(it->url);

        if (!it->multiSize) {
            if (!m_candidateIcon.availableSizes().contains(it->size))
                m_candidateIcon.addPixmap(icon.pixmap(it->size));

            continue;
        }

        const auto sizes = icon.availableSizes();
        for (const QSize &size : sizes) {
            if (!m_candidateIcon.availableSizes().contains(size))
                m_candidateIcon.addPixmap(icon.pixmap(size));
        }
    }
}


FaviconInfo::FaviconInfo()
    : url(QUrl())
    , type(FaviconInfo::InvalidIcon)
    , size(QSize(0, 0))
    , candidate(false)
    , multiSize(false)
{
}

FaviconInfo::FaviconInfo(const FaviconInfo &other)
    : url(other.url)
    , type(other.type)
    , size(other.size)
    , candidate(other.candidate)
    , multiSize(other.multiSize)
{
}

FaviconInfo::FaviconInfo(const QUrl &url, FaviconInfo::FaviconType type)
    : url(url)
    , type(type)
    , size(QSize(0, 0))
    , candidate(false)
    , multiSize(false)
{
}

FaviconInfo::~FaviconInfo()
{
}

bool FaviconInfo::isValid() const
{
    if (type == FaviconInfo::InvalidIcon)
        return false;

    if (url.isEmpty() || !url.isValid())
        return false;

    return true;
}

bool FaviconInfo::isDownloaded() const
{
    return area(size) > 0;
}

} // namespace QtWebEngineCore
