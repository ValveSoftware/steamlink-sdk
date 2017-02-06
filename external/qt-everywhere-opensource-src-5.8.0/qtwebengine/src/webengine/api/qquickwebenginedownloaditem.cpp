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

#include "qquickwebenginedownloaditem_p.h"
#include "qquickwebenginedownloaditem_p_p.h"
#include "qquickwebengineprofile_p.h"

using QtWebEngineCore::BrowserContextAdapterClient;

QT_BEGIN_NAMESPACE

static inline QQuickWebEngineDownloadItem::DownloadState toDownloadState(int state) {
    switch (state) {
    case BrowserContextAdapterClient::DownloadInProgress:
        return QQuickWebEngineDownloadItem::DownloadInProgress;
    case BrowserContextAdapterClient::DownloadCompleted:
        return QQuickWebEngineDownloadItem::DownloadCompleted;
    case BrowserContextAdapterClient::DownloadCancelled:
        return QQuickWebEngineDownloadItem::DownloadCancelled;
    case BrowserContextAdapterClient::DownloadInterrupted:
        return QQuickWebEngineDownloadItem::DownloadInterrupted;
    default:
        Q_UNREACHABLE();
        return QQuickWebEngineDownloadItem::DownloadCancelled;
    }
}

QQuickWebEngineDownloadItemPrivate::QQuickWebEngineDownloadItemPrivate(QQuickWebEngineProfile *p)
    : profile(p)
    , downloadId(-1)
    , downloadState(QQuickWebEngineDownloadItem::DownloadCancelled)
    , savePageFormat(QQuickWebEngineDownloadItem::UnknownSaveFormat)
    , type(QQuickWebEngineDownloadItem::Attachment)
    , totalBytes(-1)
    , receivedBytes(0)
{
}

QQuickWebEngineDownloadItemPrivate::~QQuickWebEngineDownloadItemPrivate()
{
    if (profile)
        profile->d_ptr->downloadDestroyed(downloadId);
}

/*!
    \qmltype WebEngineDownloadItem
    \instantiates QQuickWebEngineDownloadItem
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1
    \brief Provides information about a download.

    Stores the state of a download to be used to manage requested downloads.

    By default, the download is rejected unless the user explicitly accepts it with
    WebEngineDownloadItem::accept().
*/

void QQuickWebEngineDownloadItemPrivate::update(const BrowserContextAdapterClient::DownloadItemInfo &info)
{
    Q_Q(QQuickWebEngineDownloadItem);

    updateState(toDownloadState(info.state));

    if (info.receivedBytes != receivedBytes) {
        receivedBytes = info.receivedBytes;
        Q_EMIT q->receivedBytesChanged();
    }

    if (info.totalBytes != totalBytes) {
        totalBytes = info.totalBytes;
        Q_EMIT q->totalBytesChanged();
    }
}

void QQuickWebEngineDownloadItemPrivate::updateState(QQuickWebEngineDownloadItem::DownloadState newState)
{
    Q_Q(QQuickWebEngineDownloadItem);

    if (downloadState != newState) {
        downloadState = newState;
        Q_EMIT q->stateChanged();
    }
}

/*!
    \qmlmethod void WebEngineDownloadItem::accept()

    Accepts the download request, which will start the download.

   \sa WebEngineDownloadItem::cancel()
*/

void QQuickWebEngineDownloadItem::accept()
{
    Q_D(QQuickWebEngineDownloadItem);

    if (d->downloadState != QQuickWebEngineDownloadItem::DownloadRequested)
        return;

    d->updateState(QQuickWebEngineDownloadItem::DownloadInProgress);
}

/*!
    \qmlmethod void WebEngineDownloadItem::cancel()

    Cancels the download.
*/

void QQuickWebEngineDownloadItem::cancel()
{
    Q_D(QQuickWebEngineDownloadItem);

    QQuickWebEngineDownloadItem::DownloadState state = d->downloadState;

    if (state == QQuickWebEngineDownloadItem::DownloadCompleted
            || state == QQuickWebEngineDownloadItem::DownloadCancelled)
        return;

    d->updateState(QQuickWebEngineDownloadItem::DownloadCancelled);

    // We directly cancel the download if the user cancels before
    // it even started, so no need to notify the profile here.
    if (state == QQuickWebEngineDownloadItem::DownloadInProgress) {
        if (d->profile)
            d->profile->d_ptr->cancelDownload(d->downloadId);
    }
}

/*!
    \qmlproperty int WebEngineDownloadItem::id

    Holds the download item's ID.
*/

quint32 QQuickWebEngineDownloadItem::id() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->downloadId;
}

/*!
    \qmlproperty enumeration WebEngineDownloadItem::state

    Describes the state of the download:

    \value  WebEngineDownloadItem.DownloadRequested
            Download has been requested, but it has not been accepted yet.
    \value  WebEngineDownloadItem.DownloadInProgress
            Download is in progress.
    \value  WebEngineDownloadItem.DownloadCompleted
            Download completed successfully.
    \value  WebEngineDownloadItem.DownloadCancelled
            Download was cancelled by the user.
    \value  WebEngineDownloadItem.DownloadInterrupted
            Download has been interrupted (by the server or because of lost connectivity).
*/

QQuickWebEngineDownloadItem::DownloadState QQuickWebEngineDownloadItem::state() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->downloadState;
}

/*!
    \qmlproperty int WebEngineDownloadItem::totalBytes

    Holds the total amount of data to download in bytes.

    \c -1 means the total size is unknown.
*/

qint64 QQuickWebEngineDownloadItem::totalBytes() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->totalBytes;
}

/*!
    \qmlproperty int WebEngineDownloadItem::receivedBytes

    Holds the amount of data in bytes that has been downloaded so far.
*/

qint64 QQuickWebEngineDownloadItem::receivedBytes() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->receivedBytes;
}

/*!
    \qmlproperty string WebEngineDownloadItem::mimeType
    \since QtWebEngine 1.2

    Holds the MIME type of the download.
*/

QString QQuickWebEngineDownloadItem::mimeType() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->mimeType;
}

/*!
    \qmlproperty string WebEngineDownloadItem::path

    Holds the full target path where data is being downloaded to.

    The path includes the file name. The default suggested path is the standard
    download location and file name is deduced not to overwrite already existing files.

    The download path can only be set in the \c WebEngineProfile.onDownloadRequested
    handler before the download is accepted.

    \sa WebEngineProfile::downloadRequested(), accept()
*/

QString QQuickWebEngineDownloadItem::path() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->downloadPath;
}

void QQuickWebEngineDownloadItem::setPath(QString path)
{
    Q_D(QQuickWebEngineDownloadItem);
    if (d->downloadState != QQuickWebEngineDownloadItem::DownloadRequested) {
        qWarning("Setting the download path is not allowed after the download has been accepted.");
        return;
    }
    if (d->downloadPath != path) {
        d->downloadPath = path;
        Q_EMIT pathChanged();
    }
}

/*!
    \qmlproperty enumeration WebEngineDownloadItem::savePageFormat
    \since QtWebEngine 1.3

    Describes the format that is used to save a web page.

    \value  WebEngineDownloadItem.UnknownSaveFormat
            This is not a request for downloading a complete web page.
    \value  WebEngineDownloadItem.SingleHtmlSaveFormat
            The page is saved as a single HTML page. Resources such as images
            are not saved.
    \value  WebEngineDownloadItem.CompleteHtmlSaveFormat
            The page is saved as a complete HTML page, for example a directory
            containing the single HTML page and the resources.
    \value  WebEngineDownloadItem.MimeHtmlSaveFormat
            The page is saved as a complete web page in the MIME HTML format.
*/

QQuickWebEngineDownloadItem::SavePageFormat QQuickWebEngineDownloadItem::savePageFormat() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->savePageFormat;
}

void QQuickWebEngineDownloadItem::setSavePageFormat(QQuickWebEngineDownloadItem::SavePageFormat format)
{
    Q_D(QQuickWebEngineDownloadItem);
    if (d->savePageFormat != format) {
        d->savePageFormat = format;
        Q_EMIT savePageFormatChanged();
    }
}

/*!
    \qmlproperty enumeration WebEngineDownloadItem::type
    \readonly
    \since QtWebEngine 1.4

    Describes the requested download's type.

    \value WebEngineDownloadItem.Attachment The web server's response includes a
           \c Content-Disposition header with the \c attachment directive. If \c Content-Disposition
           is present in the reply, the web server is indicating that the client should prompt the
           user to save the content regardless of the content type.
           See \l {RFC 2616 section 19.5.1} for details.
    \value WebEngineDownloadItem.DownloadAttribute The user clicked a link with the \c download
           attribute. See \l {HTML download attribute} for details.
    \value WebEngineDownloadItem.UserRequested The user initiated the download, for example by
           selecting a web action.
    \value WebEngineDownloadItem.SavePage Saving of the current page was requested (for example by
           the \l{WebEngineView::WebAction}{WebEngineView.SavePage} web action).
 */

QQuickWebEngineDownloadItem::DownloadType QQuickWebEngineDownloadItem::type() const
{
    Q_D(const QQuickWebEngineDownloadItem);
    return d->type;
}

QQuickWebEngineDownloadItem::QQuickWebEngineDownloadItem(QQuickWebEngineDownloadItemPrivate *p, QObject *parent)
    : QObject(parent)
    , d_ptr(p)
{
    p->q_ptr = this;
}

QQuickWebEngineDownloadItem::~QQuickWebEngineDownloadItem()
{
}

QT_END_NAMESPACE
