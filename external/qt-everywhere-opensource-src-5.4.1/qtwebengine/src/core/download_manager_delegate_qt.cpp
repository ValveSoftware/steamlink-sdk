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

#include "download_manager_delegate_qt.h"

#include "content/public/browser/download_item.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QStandardPaths>

#include "type_conversion.h"
#include "qtwebenginecoreglobal.h"

// Helper class to track currently ongoing downloads to prevent file name
// clashes / overwriting of files.
class DownloadTargetHelper : public content::DownloadItem::Observer {
public:
    DownloadTargetHelper()
        : m_defaultDownloadDirectory(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
    {

    }
    virtual ~DownloadTargetHelper() {}

    bool determineDownloadTarget(content::DownloadItem *item, const content::DownloadTargetCallback &callback);

    virtual void OnDownloadUpdated(content::DownloadItem *download) Q_DECL_OVERRIDE;
    virtual void OnDownloadDestroyed(content::DownloadItem *download) Q_DECL_OVERRIDE;
private:
    bool isPathAvailable(const QString& path);

    QDir m_defaultDownloadDirectory;
    QMap<content::DownloadItem*, QString> m_ongoingDownloads;
};

bool DownloadTargetHelper::isPathAvailable(const QString& path)
{
    return !m_ongoingDownloads.values().contains(path) && !QFile::exists(path);
}

bool DownloadTargetHelper::determineDownloadTarget(content::DownloadItem *item, const content::DownloadTargetCallback &callback)
{
    std::string suggestedFilename = item->GetSuggestedFilename();

    if (suggestedFilename.empty())
        suggestedFilename = item->GetTargetFilePath().AsUTF8Unsafe();

    if (suggestedFilename.empty())
        suggestedFilename = item->GetURL().ExtractFileName();

    if (suggestedFilename.empty())
        suggestedFilename = "qwe_download";

    if (!m_defaultDownloadDirectory.exists() && !m_defaultDownloadDirectory.mkpath(m_defaultDownloadDirectory.absolutePath()))
        return false;

    QString suggestedFilePath = m_defaultDownloadDirectory.absoluteFilePath(QString::fromStdString(suggestedFilename));
    if (!isPathAvailable(suggestedFilePath)) {
        int i = 1;
        for (; i < 99; i++) {
            QFileInfo tmpFile(suggestedFilePath);
            QString tmpFilePath = QString("%1%2%3(%4).%5").arg(tmpFile.absolutePath()).arg(QDir::separator()).arg(tmpFile.baseName()).arg(i).arg(tmpFile.completeSuffix());
            if (isPathAvailable(tmpFilePath)) {
                suggestedFilePath = tmpFilePath;
                break;
            }
        }
        if (i >= 99) {
            callback.Run(base::FilePath(), content::DownloadItem::TARGET_DISPOSITION_PROMPT, content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, base::FilePath());
            return false;
        }
    }

    m_ongoingDownloads.insert(item, suggestedFilePath);
    item->AddObserver(this);

    base::FilePath filePathForCallback(toFilePathString(suggestedFilePath));
    callback.Run(filePathForCallback, content::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, filePathForCallback.AddExtension(toFilePathString("download")));
    return true;
}

void DownloadTargetHelper::OnDownloadUpdated(content::DownloadItem *download)
{
    switch (download->GetState()) {
    case content::DownloadItem::COMPLETE:
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
        download->RemoveObserver(this);
        m_ongoingDownloads.remove(download);
        break;
    case content::DownloadItem::IN_PROGRESS:
    default:
        break;
    }
}

void DownloadTargetHelper::OnDownloadDestroyed(content::DownloadItem *download)
{
    download->RemoveObserver(this);
    m_ongoingDownloads.remove(download);
}

DownloadManagerDelegateQt::DownloadManagerDelegateQt()
    : m_targetHelper(new DownloadTargetHelper())
    , m_currentId(0)
{

}

DownloadManagerDelegateQt::~DownloadManagerDelegateQt()
{
    delete m_targetHelper;
}


void DownloadManagerDelegateQt::Shutdown()
{
    QT_NOT_YET_IMPLEMENTED
}

void DownloadManagerDelegateQt::GetNextId(const content::DownloadIdCallback& callback)
{
    callback.Run(++m_currentId);
}

bool DownloadManagerDelegateQt::ShouldOpenFileBasedOnExtension(const base::FilePath& path)
{
    QT_NOT_YET_IMPLEMENTED
    return false;
}

bool DownloadManagerDelegateQt::ShouldCompleteDownload(content::DownloadItem* item,
                                                       const base::Closure& complete_callback)
{
    QT_NOT_YET_IMPLEMENTED
    return true;
}

bool DownloadManagerDelegateQt::ShouldOpenDownload(content::DownloadItem* item,
                                                   const content::DownloadOpenDelayedCallback& callback)
{
    QT_NOT_YET_IMPLEMENTED
    return false;
}

bool DownloadManagerDelegateQt::DetermineDownloadTarget(content::DownloadItem* item,
                                                        const content::DownloadTargetCallback& callback)
{
    // Keep the forced file path if set, also as the temporary file, so the check for existence
    // will already return that the file exists. Forced file paths seem to be only used for
    // store downloads and other special downloads, so they might never end up here anyway.
    if (!item->GetForcedFilePath().empty()) {
        callback.Run(item->GetForcedFilePath(), content::DownloadItem::TARGET_DISPOSITION_PROMPT,
                     content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, item->GetForcedFilePath());
        return true;
    }

    // Let the target helper determine the download target path.
    return m_targetHelper->determineDownloadTarget(item, callback);
}

bool DownloadManagerDelegateQt::GenerateFileHash()
{
    QT_NOT_YET_IMPLEMENTED
    return false;
}

void DownloadManagerDelegateQt::ChooseSavePath(
        content::WebContents* web_contents,
        const base::FilePath& suggested_path,
        const base::FilePath::StringType& default_extension,
        bool can_save_as_complete,
        const content::SavePackagePathPickedCallback& callback)
{
    QT_NOT_YET_IMPLEMENTED
}

void DownloadManagerDelegateQt::OpenDownload(content::DownloadItem* download)
{
    QT_NOT_YET_IMPLEMENTED
}

void DownloadManagerDelegateQt::ShowDownloadInShell(content::DownloadItem* download)
{
    QT_NOT_YET_IMPLEMENTED
}

void DownloadManagerDelegateQt::CheckForFileExistence(
        content::DownloadItem* download,
        const content::CheckForFileExistenceCallback& callback)
{
    QT_NOT_YET_IMPLEMENTED
}

void DownloadManagerDelegateQt::GetSaveDir(content::BrowserContext* browser_context,
                        base::FilePath* website_save_dir,
                        base::FilePath* download_save_dir,
                        bool* skip_dir_check)
{
    static base::FilePath::StringType save_dir = toFilePathString(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    *website_save_dir = base::FilePath(save_dir);
    *download_save_dir = base::FilePath(save_dir);
    *skip_dir_check = true;
}


