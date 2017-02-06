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

#ifndef DOWNLOAD_MANAGER_DELEGATE_QT_H
#define DOWNLOAD_MANAGER_DELEGATE_QT_H

#include "content/public/browser/download_manager_delegate.h"
#include <base/memory/weak_ptr.h>

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class DownloadItem;
class WebContents;
}

namespace QtWebEngineCore {
class BrowserContextAdapter;
class DownloadManagerDelegateInstance;
class DownloadTargetHelper;

class DownloadManagerDelegateQt
        : public content::DownloadManagerDelegate
        , public content::DownloadItem::Observer
{
public:
    DownloadManagerDelegateQt(BrowserContextAdapter *contextAdapter);
    ~DownloadManagerDelegateQt();
    void GetNextId(const content::DownloadIdCallback& callback) Q_DECL_OVERRIDE;

    bool DetermineDownloadTarget(content::DownloadItem* item,
                                 const content::DownloadTargetCallback& callback) Q_DECL_OVERRIDE;

    void GetSaveDir(content::BrowserContext* browser_context,
                    base::FilePath* website_save_dir,
                    base::FilePath* download_save_dir,
                    bool* skip_dir_check) Q_DECL_OVERRIDE;
    void ChooseSavePath(content::WebContents *web_contents,
                        const base::FilePath &suggested_path,
                        const base::FilePath::StringType &default_extension,
                        bool can_save_as_complete,
                        const content::SavePackagePathPickedCallback &callback) Q_DECL_OVERRIDE;

    void cancelDownload(quint32 downloadId);

    void setDownloadType(int downloadType) { m_downloadType = downloadType; }

    // Inherited from content::DownloadItem::Observer
    void OnDownloadUpdated(content::DownloadItem *download) Q_DECL_OVERRIDE;
    void OnDownloadDestroyed(content::DownloadItem *download) Q_DECL_OVERRIDE;

private:
    void cancelDownload(const content::DownloadTargetCallback& callback);
    void savePackageDownloadCreated(content::DownloadItem *download);
    BrowserContextAdapter *m_contextAdapter;

    uint64_t m_currentId;
    base::WeakPtrFactory<DownloadManagerDelegateQt> m_weakPtrFactory;
    int m_downloadType;

    friend class DownloadManagerDelegateInstance;
    DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateQt);
};

} // namespace QtWebEngineCore

#endif //DOWNLOAD_MANAGER_DELEGATE_QT_H
