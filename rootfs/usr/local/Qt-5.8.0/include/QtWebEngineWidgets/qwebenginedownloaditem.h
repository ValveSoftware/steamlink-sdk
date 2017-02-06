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

#ifndef QWEBENGINEDOWNLOADITEM_H
#define QWEBENGINEDOWNLOADITEM_H

#include <QtWebEngineWidgets/qtwebenginewidgetsglobal.h>

#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QWebEngineDownloadItemPrivate;
class QWebEngineProfilePrivate;

class QWEBENGINEWIDGETS_EXPORT QWebEngineDownloadItem : public QObject
{
    Q_OBJECT
public:
    ~QWebEngineDownloadItem();

    enum DownloadState {
        DownloadRequested,
        DownloadInProgress,
        DownloadCompleted,
        DownloadCancelled,
        DownloadInterrupted
    };
    Q_ENUM(DownloadState)

    enum SavePageFormat {
        UnknownSaveFormat = -1,
        SingleHtmlSaveFormat,
        CompleteHtmlSaveFormat,
        MimeHtmlSaveFormat
    };
    Q_ENUM(SavePageFormat)

    enum DownloadType {
        Attachment = 0,
        DownloadAttribute,
        UserRequested,
        SavePage
    };
    Q_ENUM(DownloadType)

    quint32 id() const;
    DownloadState state() const;
    qint64 totalBytes() const;
    qint64 receivedBytes() const;
    QUrl url() const;
    QString mimeType() const;
    QString path() const;
    void setPath(QString path);
    bool isFinished() const;
    SavePageFormat savePageFormat() const;
    void setSavePageFormat(SavePageFormat format);
    DownloadType type() const;

public Q_SLOTS:
    void accept();
    void cancel();

Q_SIGNALS:
    void finished();
    void stateChanged(QWebEngineDownloadItem::DownloadState state);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    Q_DISABLE_COPY(QWebEngineDownloadItem)
    Q_DECLARE_PRIVATE(QWebEngineDownloadItem)

    friend class QWebEngineProfilePrivate;

    QWebEngineDownloadItem(QWebEngineDownloadItemPrivate*, QObject *parent = Q_NULLPTR);
    QScopedPointer<QWebEngineDownloadItemPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QWEBENGINEDOWNLOADITEM_H
