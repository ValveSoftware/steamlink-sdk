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

#ifndef WEB_CONTENTS_ADAPTER_H
#define WEB_CONTENTS_ADAPTER_H

#include "qtwebenginecoreglobal.h"
#include "web_contents_adapter_client.h"

#include <QScopedPointer>
#include <QSharedData>
#include <QString>
#include <QUrl>

namespace content {
class WebContents;
}
class WebContentsAdapterPrivate;
struct WebPreferences;

QT_FORWARD_DECLARE_CLASS(QAccessibleInterface);

class QWEBENGINE_EXPORT WebContentsAdapter : public QSharedData {
public:
    static QExplicitlySharedDataPointer<WebContentsAdapter> createFromSerializedNavigationHistory(QDataStream &input, WebContentsAdapterClient *adapterClient);
    // Takes ownership of the WebContents.
    WebContentsAdapter(content::WebContents *webContents = 0);
    ~WebContentsAdapter();
    void initialize(WebContentsAdapterClient *adapterClient);
    void reattachRWHV();

    bool canGoBack() const;
    bool canGoForward() const;
    void stop();
    void reload();
    void load(const QUrl&);
    void setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl);
    QUrl activeUrl() const;
    QUrl requestedUrl() const;
    QString pageTitle() const;
    QString selectedText() const;
    QUrl iconUrl() const;

    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void pasteAndMatchStyle();
    void selectAll();

    void navigateToIndex(int);
    void navigateToOffset(int);
    int navigationEntryCount();
    int currentNavigationEntryIndex();
    QUrl getNavigationEntryOriginalUrl(int index);
    QUrl getNavigationEntryUrl(int index);
    QString getNavigationEntryTitle(int index);
    QDateTime getNavigationEntryTimestamp(int index);
    QUrl getNavigationEntryIconUrl(int index);
    void clearNavigationHistory();
    void serializeNavigationHistory(QDataStream &output);
    void setZoomFactor(qreal);
    qreal currentZoomFactor() const;
    void enableInspector(bool);
    void filesSelectedInChooser(const QStringList &fileList, WebContentsAdapterClient::FileChooserMode);
    void runJavaScript(const QString &javaScript);
    quint64 runJavaScriptCallbackResult(const QString &javaScript);
    quint64 fetchDocumentMarkup();
    quint64 fetchDocumentInnerText();
    quint64 findText(const QString &subString, bool caseSensitively, bool findBackward);
    void stopFinding();
    void updateWebPreferences(const WebPreferences &webPreferences);

    void wasShown();
    void wasHidden();
    void grantMediaAccessPermission(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags flags);

    void dpiScaleChanged();
    QAccessibleInterface *browserAccessible();

private:
    Q_DISABLE_COPY(WebContentsAdapter);
    Q_DECLARE_PRIVATE(WebContentsAdapter);
    QScopedPointer<WebContentsAdapterPrivate> d_ptr;
    friend class WebContentsDelegateQt;
};
#endif // WEB_CONTENTS_ADAPTER_H
