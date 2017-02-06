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

#ifndef BROWSER_CONTEXT_ADAPTER_CLIENT_H
#define BROWSER_CONTEXT_ADAPTER_CLIENT_H

#include "qtwebenginecoreglobal.h"
#include <QString>
#include <QUrl>

namespace QtWebEngineCore {

class QWEBENGINE_EXPORT BrowserContextAdapterClient
{
public:
    // Keep in sync with content::DownloadItem::DownloadState
    enum DownloadState {
        // Download is actively progressing.
        DownloadInProgress = 0,
        // Download is completely finished.
        DownloadCompleted,
        // Download has been cancelled.
        DownloadCancelled,
        // This state indicates that the download has been interrupted.
        DownloadInterrupted
    };

    // Keep in sync with content::SavePageType
    enum SavePageFormat {
        UnknownSavePageFormat = -1,
        SingleHtmlSaveFormat,
        CompleteHtmlSaveFormat,
        MimeHtmlSaveFormat
    };

    enum DownloadType {
        Attachment = 0,
        DownloadAttribute,
        UserRequested,
        SavePage
    };

    struct DownloadItemInfo {
        const quint32 id;
        const QUrl url;
        const int state;
        const qint64 totalBytes;
        const qint64 receivedBytes;
        const QString mimeType;

        QString path;
        int savePageFormat;
        bool accepted;
        int downloadType;
    };

    virtual ~BrowserContextAdapterClient() { }

    virtual void downloadRequested(DownloadItemInfo &info) = 0;
    virtual void downloadUpdated(const DownloadItemInfo &info) = 0;
};

} // namespace

#endif // BROWSER_CONTEXT_ADAPTER_CLIENT_H
