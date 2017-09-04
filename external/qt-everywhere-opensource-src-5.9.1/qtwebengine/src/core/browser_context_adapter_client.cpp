/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "browser_context_adapter_client.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/save_page_type.h"

#include <QCoreApplication>
#include <QString>

namespace QtWebEngineCore {

ASSERT_ENUMS_MATCH(content::DownloadItem::IN_PROGRESS, BrowserContextAdapterClient::DownloadInProgress)
ASSERT_ENUMS_MATCH(content::DownloadItem::COMPLETE, BrowserContextAdapterClient::DownloadCompleted)
ASSERT_ENUMS_MATCH(content::DownloadItem::CANCELLED, BrowserContextAdapterClient::DownloadCancelled)
ASSERT_ENUMS_MATCH(content::DownloadItem::INTERRUPTED, BrowserContextAdapterClient::DownloadInterrupted)

ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_UNKNOWN, BrowserContextAdapterClient::UnknownSavePageFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_ONLY_HTML, BrowserContextAdapterClient::SingleHtmlSaveFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, BrowserContextAdapterClient::CompleteHtmlSaveFormat)
ASSERT_ENUMS_MATCH(content::SAVE_PAGE_TYPE_AS_MHTML, BrowserContextAdapterClient::MimeHtmlSaveFormat)

ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NONE, BrowserContextAdapterClient::NoReason)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, BrowserContextAdapterClient::FileFailed)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED, BrowserContextAdapterClient::FileAccessDenied)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, BrowserContextAdapterClient::FileNoSpace)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG, BrowserContextAdapterClient::FileNameTooLong)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE, BrowserContextAdapterClient::FileTooLarge)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED, BrowserContextAdapterClient::FileVirusInfected)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, BrowserContextAdapterClient::FileTransientError)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, BrowserContextAdapterClient::FileBlocked)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED, BrowserContextAdapterClient::FileSecurityCheckFailed)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT, BrowserContextAdapterClient::FileTooShort)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH, BrowserContextAdapterClient::FileHashMismatch)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, BrowserContextAdapterClient::NetworkFailed)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT, BrowserContextAdapterClient::NetworkTimeout)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED, BrowserContextAdapterClient::NetworkDisconnected)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN, BrowserContextAdapterClient::NetworkServerDown)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST, BrowserContextAdapterClient::NetworkInvalidRequest)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, BrowserContextAdapterClient::ServerFailed)
//ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE, BrowserContextAdapterClient::ServerNoRange)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, BrowserContextAdapterClient::ServerBadContent)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED, BrowserContextAdapterClient::ServerUnauthorized)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM, BrowserContextAdapterClient::ServerCertProblem)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN, BrowserContextAdapterClient::ServerForbidden)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE, BrowserContextAdapterClient::ServerUnreachable)
ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, BrowserContextAdapterClient::UserCanceled)
//ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, BrowserContextAdapterClient::UserShutdown)
//ASSERT_ENUMS_MATCH(content::DOWNLOAD_INTERRUPT_REASON_CRASH, BrowserContextAdapterClient::Crash)

QString BrowserContextAdapterClient::downloadInterruptReasonToString(DownloadInterruptReason reason)
{
    switch (reason) {
    default:
        // Yield an error in debug mode, but fall through to some defined behavior
        Q_UNREACHABLE();
    case NoReason:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Unknown reason or not interrupted");
    case FileFailed:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "General file operation failure");
    case FileAccessDenied:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The file cannot be written locally, due to access restrictions");
    case FileNoSpace:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Insufficient space on the target drive");
    case FileNameTooLong:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The directory or file name is too long");
    case FileTooLarge:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The file size exceeds the file system limitation");
    case FileVirusInfected:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The file is infected with a virus");
    case FileTransientError:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Temporary problem (for example file in use, or too many open files)");
    case FileBlocked:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The file was blocked due to local policy");
    case FileSecurityCheckFailed:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Checking the safety of the download failed due to unexpected reasons");
    case FileTooShort:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "File seek past the end of a file (resuming previously interrupted download)");
    case FileHashMismatch:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The partial file did not match the expected hash");
    case NetworkFailed:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "General network failure");
    case NetworkTimeout:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The network operation has timed out");
    case NetworkDisconnected:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The network connection has been terminated");
    case NetworkServerDown:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The server has gone down");
    case NetworkInvalidRequest:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The network request was invalid (for example, the URL or scheme is invalid)");
    case ServerFailed:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "General server failure");
    //case ServerNoRange:
    //    return QCoreApplication::translate("DownloadInterruptReason",
    //                                       "Server does not support range requests");
    case ServerBadContent:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The server does not have the requested data");
    case ServerUnauthorized:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "The server did not authorize access to the resource");
    case ServerCertProblem:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "A problem with the server certificate occurred");
    case ServerForbidden:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Access forbidden by the server");
    case ServerUnreachable:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Unexpected server response");
    case UserCanceled:
        return QCoreApplication::translate("DownloadInterruptReason",
                                           "Download canceled by the user");
    //case UserShutdown:
    //    return QCoreApplication::translate("DownloadInterruptReason",
    //                                       "The user shut down the browser");
    //case Crash:
    //    return QCoreApplication::translate("DownloadInterruptReason",
    //                                       "The browser crashed");
    }
}

} // namespace QtWebEngineCore
