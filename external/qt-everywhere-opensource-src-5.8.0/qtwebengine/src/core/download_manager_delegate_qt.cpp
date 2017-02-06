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

#include "download_manager_delegate_qt.h"

#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_content_disposition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMimeDatabase>
#include <QStandardPaths>

#include "browser_context_adapter.h"
#include "browser_context_adapter_client.h"
#include "browser_context_qt.h"
#include "type_conversion.h"
#include "web_contents_delegate_qt.h"
#include "qtwebenginecoreglobal.h"

namespace QtWebEngineCore {

ASSERT_ENUMS_MATCH(content::DownloadItem::IN_PROGRESS, BrowserContextAdapterClient::DownloadInProgress)
ASSERT_ENUMS_MATCH(content::DownloadItem::COMPLETE, BrowserContextAdapterClient::DownloadCompleted)
ASSERT_ENUMS_MATCH(content::DownloadItem::CANCELLED, BrowserContextAdapterClient::DownloadCancelled)
ASSERT_ENUMS_MATCH(content::DownloadItem::INTERRUPTED, BrowserContextAdapterClient::DownloadInterrupted)

ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_UNKNOWN, BrowserContextAdapterClient::UnknownSavePageFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_ONLY_HTML, BrowserContextAdapterClient::SingleHtmlSaveFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, BrowserContextAdapterClient::CompleteHtmlSaveFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_MHTML, BrowserContextAdapterClient::MimeHtmlSaveFormat)

DownloadManagerDelegateQt::DownloadManagerDelegateQt(BrowserContextAdapter *contextAdapter)
    : m_contextAdapter(contextAdapter)
    , m_currentId(0)
    , m_weakPtrFactory(this)
    , m_downloadType(BrowserContextAdapterClient::Attachment)
{
    Q_ASSERT(m_contextAdapter);
}

DownloadManagerDelegateQt::~DownloadManagerDelegateQt()
{
}

void DownloadManagerDelegateQt::GetNextId(const content::DownloadIdCallback& callback)
{
    callback.Run(++m_currentId);
}

void DownloadManagerDelegateQt::cancelDownload(const content::DownloadTargetCallback& callback)
{
    callback.Run(base::FilePath(), content::DownloadItem::TARGET_DISPOSITION_PROMPT, content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, base::FilePath());
}

void DownloadManagerDelegateQt::cancelDownload(quint32 downloadId)
{
    content::DownloadManager* dlm = content::BrowserContext::GetDownloadManager(m_contextAdapter->browserContext());
    content::DownloadItem *download = dlm->GetDownload(downloadId);
    if (download)
        download->Cancel(/* user_cancel */ true);
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

    QString suggestedFilename = toQt(item->GetSuggestedFilename());
    QString mimeTypeString = toQt(item->GetMimeType());

    bool isAttachment = net::HttpContentDisposition(item->GetContentDisposition(), std::string()).is_attachment();

    if (!isAttachment || !BrowserContextAdapterClient::UserRequested)
        m_downloadType = BrowserContextAdapterClient::DownloadAttribute;

    if (suggestedFilename.isEmpty())
        suggestedFilename = toQt(net::HttpContentDisposition(item->GetContentDisposition(), std::string()).filename());

    if (suggestedFilename.isEmpty())
        suggestedFilename = toQt(item->GetTargetFilePath().AsUTF8Unsafe());

    if (suggestedFilename.isEmpty())
        suggestedFilename = toQt(item->GetURL().ExtractFileName());

    if (suggestedFilename.isEmpty()) {
        suggestedFilename = QStringLiteral("qwe_download");
        QMimeType mimeType = QMimeDatabase().mimeTypeForName(mimeTypeString);
        if (mimeType.isValid() && !mimeType.preferredSuffix().isEmpty())
            suggestedFilename += QStringLiteral(".") + mimeType.preferredSuffix();
    }

    QDir defaultDownloadDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    QFileInfo suggestedFile(defaultDownloadDirectory.absoluteFilePath(suggestedFilename));
    QString suggestedFilePath = suggestedFile.absoluteFilePath();
    QString tmpFileBase = QString("%1%2%3").arg(suggestedFile.absolutePath()).arg(QDir::separator()).arg(suggestedFile.baseName());

    for (int i = 1; QFileInfo::exists(suggestedFilePath); ++i) {
        suggestedFilePath = QString("%1(%2).%3").arg(tmpFileBase).arg(i).arg(suggestedFile.completeSuffix());
        if (i >= 99) {
            suggestedFilePath = suggestedFile.absoluteFilePath();
            break;
        }
    }

    item->AddObserver(this);
    QList<BrowserContextAdapterClient*> clients = m_contextAdapter->clients();
    if (!clients.isEmpty()) {
        BrowserContextAdapterClient::DownloadItemInfo info = {
            item->GetId(),
            toQt(item->GetURL()),
            item->GetState(),
            item->GetTotalBytes(),
            item->GetReceivedBytes(),
            mimeTypeString,
            suggestedFilePath,
            BrowserContextAdapterClient::UnknownSavePageFormat,
            false /* accepted */,
            m_downloadType
        };

        Q_FOREACH (BrowserContextAdapterClient *client, clients) {
            client->downloadRequested(info);
            if (info.accepted)
                break;
        }

        suggestedFile.setFile(info.path);

        if (info.accepted && !suggestedFile.absoluteDir().mkpath(suggestedFile.absolutePath())) {
            qWarning("Creating download path failed, download cancelled: %s", suggestedFile.absolutePath().toUtf8().data());
            info.accepted = false;
        }

        if (!info.accepted) {
            cancelDownload(callback);
            return true;
        }

        base::FilePath filePathForCallback(toFilePathString(suggestedFile.absoluteFilePath()));
        callback.Run(filePathForCallback, content::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                     content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, filePathForCallback.AddExtension(toFilePathString("download")));
    } else
        cancelDownload(callback);

    return true;
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

void DownloadManagerDelegateQt::ChooseSavePath(content::WebContents *web_contents,
                        const base::FilePath &suggested_path,
                        const base::FilePath::StringType &default_extension,
                        bool can_save_as_complete,
                        const content::SavePackagePathPickedCallback &callback)
{
    Q_UNUSED(default_extension);
    Q_UNUSED(can_save_as_complete);

    QList<BrowserContextAdapterClient*> clients = m_contextAdapter->clients();
    if (clients.isEmpty())
        return;

    WebContentsDelegateQt *contentsDelegate = static_cast<WebContentsDelegateQt *>(
            web_contents->GetDelegate());
    const SavePageInfo &spi = contentsDelegate->savePageInfo();

    bool acceptedByDefault = false;
    QString suggestedFilePath = spi.requestedFilePath;
    if (suggestedFilePath.isEmpty()) {
        suggestedFilePath = QFileInfo(toQt(suggested_path.AsUTF8Unsafe())).completeBaseName()
                + QStringLiteral(".mhtml");
    } else {
        acceptedByDefault = true;
    }
    if (QFileInfo(suggestedFilePath).isRelative()) {
        const QDir downloadDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        suggestedFilePath = downloadDir.absoluteFilePath(suggestedFilePath);
    }

    BrowserContextAdapterClient::SavePageFormat suggestedSaveFormat
            = static_cast<BrowserContextAdapterClient::SavePageFormat>(spi.requestedFormat);
    if (suggestedSaveFormat == BrowserContextAdapterClient::UnknownSavePageFormat)
        suggestedSaveFormat = BrowserContextAdapterClient::MimeHtmlSaveFormat;

    // Clear the delegate's SavePageInfo. It's only valid for the page currently being saved.
    contentsDelegate->setSavePageInfo(SavePageInfo());

    BrowserContextAdapterClient::DownloadItemInfo info = {
        m_currentId + 1,
        toQt(web_contents->GetURL()),
        content::DownloadItem::IN_PROGRESS,
        0, /* totalBytes */
        0, /* receivedBytes */
        QStringLiteral("application/x-mimearchive"),
        suggestedFilePath,
        suggestedSaveFormat,
        acceptedByDefault,
        BrowserContextAdapterClient::SavePage
    };

    Q_FOREACH (BrowserContextAdapterClient *client, clients) {
        client->downloadRequested(info);
        if (info.accepted)
            break;
    }

    if (!info.accepted)
        return;

    callback.Run(toFilePath(info.path), static_cast<content::SavePageType>(info.savePageFormat),
                 base::Bind(&DownloadManagerDelegateQt::savePackageDownloadCreated,
                            m_weakPtrFactory.GetWeakPtr()));
}

void DownloadManagerDelegateQt::savePackageDownloadCreated(content::DownloadItem *item)
{
    OnDownloadUpdated(item);
    item->AddObserver(this);
}

void DownloadManagerDelegateQt::OnDownloadUpdated(content::DownloadItem *download)
{
    QList<BrowserContextAdapterClient*> clients = m_contextAdapter->clients();
    if (!clients.isEmpty()) {
        BrowserContextAdapterClient::DownloadItemInfo info = {
            download->GetId(),
            toQt(download->GetURL()),
            download->GetState(),
            download->GetTotalBytes(),
            download->GetReceivedBytes(),
            toQt(download->GetMimeType()),
            QString(),
            BrowserContextAdapterClient::UnknownSavePageFormat,
            true /* accepted */,
            m_downloadType
        };

        Q_FOREACH (BrowserContextAdapterClient *client, clients) {
            client->downloadUpdated(info);
        }
    }
}

void DownloadManagerDelegateQt::OnDownloadDestroyed(content::DownloadItem *download)
{
    download->RemoveObserver(this);
    download->Cancel(/* user_cancel */ false);
}

} // namespace QtWebEngineCore
