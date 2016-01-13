/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DOWNLOAD_MANAGER_DELEGATE_QT_H
#define DOWNLOAD_MANAGER_DELEGATE_QT_H

#include "content/public/browser/download_manager_delegate.h"

#include <qglobal.h>

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class DownloadItem;
class WebContents;
}

class DownloadTargetHelper;

class DownloadManagerDelegateQt : public content::DownloadManagerDelegate
{
public:
    DownloadManagerDelegateQt();
    virtual ~DownloadManagerDelegateQt();

    void Shutdown() Q_DECL_OVERRIDE;
    void GetNextId(const content::DownloadIdCallback& callback) Q_DECL_OVERRIDE;
    bool ShouldOpenFileBasedOnExtension(const base::FilePath& path) Q_DECL_OVERRIDE;
    bool ShouldCompleteDownload(content::DownloadItem* item,
                                const base::Closure& complete_callback) Q_DECL_OVERRIDE;

    bool ShouldOpenDownload(content::DownloadItem* item,
                            const content::DownloadOpenDelayedCallback& callback) Q_DECL_OVERRIDE;

    bool DetermineDownloadTarget(content::DownloadItem* item,
                                 const content::DownloadTargetCallback& callback) Q_DECL_OVERRIDE;

    bool GenerateFileHash() Q_DECL_OVERRIDE;
    void ChooseSavePath(content::WebContents* web_contents,
                        const base::FilePath& suggested_path,
                        const base::FilePath::StringType& default_extension,
                        bool can_save_as_complete,
                        const content::SavePackagePathPickedCallback& callback) Q_DECL_OVERRIDE;

    void OpenDownload(content::DownloadItem* download) Q_DECL_OVERRIDE;
    void ShowDownloadInShell(content::DownloadItem* download) Q_DECL_OVERRIDE;
    void CheckForFileExistence(content::DownloadItem* download,
                               const content::CheckForFileExistenceCallback& callback) Q_DECL_OVERRIDE;

    void GetSaveDir(content::BrowserContext* browser_context,
                    base::FilePath* website_save_dir,
                    base::FilePath* download_save_dir,
                    bool* skip_dir_check) Q_DECL_OVERRIDE;

private:
    DownloadTargetHelper* m_targetHelper;
    uint64 m_currentId;
    DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateQt);
};

#endif //DOWNLOAD_MANAGER_DELEGATE_QT_H
