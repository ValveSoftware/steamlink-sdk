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

#ifndef WEB_CONTENTS_ADAPTER_H
#define WEB_CONTENTS_ADAPTER_H

#include "qtwebenginecoreglobal.h"
#include "web_contents_adapter_client.h"

#include <QScopedPointer>
#include <QSharedPointer>
#include <QString>
#include <QUrl>

namespace content {
class WebContents;
struct WebPreferences;
}

QT_BEGIN_NAMESPACE
class QAccessibleInterface;
class QDragEnterEvent;
class QDragMoveEvent;
class QPageLayout;
class QString;
class QWebChannel;
QT_END_NAMESPACE

namespace QtWebEngineCore {

class BrowserContextQt;
class MessagePassingInterface;
class WebContentsAdapterPrivate;
class FaviconManager;

class QWEBENGINE_EXPORT WebContentsAdapter : public QEnableSharedFromThis<WebContentsAdapter> {
public:
    static QSharedPointer<WebContentsAdapter> createFromSerializedNavigationHistory(QDataStream &input, WebContentsAdapterClient *adapterClient);
    // Takes ownership of the WebContents.
    WebContentsAdapter(content::WebContents *webContents = 0);
    ~WebContentsAdapter();
    void initialize(WebContentsAdapterClient *adapterClient);
    void reattachRWHV();

    bool canGoBack() const;
    bool canGoForward() const;
    void stop();
    void reload();
    void reloadAndBypassCache();
    void load(const QUrl&);
    void setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl);
    void save(const QString &filePath = QString(), int savePageFormat = -1);
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
    void unselect();

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
    void runJavaScript(const QString &javaScript, quint32 worldId);
    quint64 runJavaScriptCallbackResult(const QString &javaScript, quint32 worldId);
    quint64 fetchDocumentMarkup();
    quint64 fetchDocumentInnerText();
    quint64 findText(const QString &subString, bool caseSensitively, bool findBackward);
    void stopFinding();
    void updateWebPreferences(const content::WebPreferences &webPreferences);
    void download(const QUrl &url, const QString &suggestedFileName);
    bool isAudioMuted() const;
    void setAudioMuted(bool mute);
    bool recentlyAudible();

    // Must match blink::WebMediaPlayerAction::Type.
    enum MediaPlayerAction {
        MediaPlayerNoAction,
        MediaPlayerPlay,
        MediaPlayerMute,
        MediaPlayerLoop,
        MediaPlayerControls,
        MediaPlayerTypeLast = MediaPlayerControls
    };
    void copyImageAt(const QPoint &location);
    void executeMediaPlayerActionAt(const QPoint &location, MediaPlayerAction action, bool enable);

    void inspectElementAt(const QPoint &location);
    bool hasInspector() const;
    void exitFullScreen();
    void requestClose();
    void changedFullScreen();

    void wasShown();
    void wasHidden();
    void grantMediaAccessPermission(const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags flags);
    void runGeolocationRequestCallback(const QUrl &securityOrigin, bool allowed);
    void grantMouseLockPermission(bool granted);

    void dpiScaleChanged();
    void backgroundColorChanged();
    QAccessibleInterface *browserAccessible();
    BrowserContextQt* browserContext();
    BrowserContextAdapter* browserContextAdapter();
    QWebChannel *webChannel() const;
    void setWebChannel(QWebChannel *, uint worldId);
    FaviconManager *faviconManager();

    QPointF lastScrollOffset() const;
    QSizeF lastContentsSize() const;

    void startDragging(QObject *dragSource, const content::DropData &dropData,
                       Qt::DropActions allowedActions, const QPixmap &pixmap, const QPoint &offset);
    void enterDrag(QDragEnterEvent *e, const QPoint &screenPos);
    Qt::DropAction updateDragPosition(QDragMoveEvent *e, const QPoint &screenPos);
    void updateDragAction(Qt::DropAction action);
    void finishDragUpdate();
    void endDragging(const QPoint &clientPos, const QPoint &screenPos);
    void leaveDrag();
    void initUpdateDragCursorMessagePollingTimer();
    void printToPDF(const QPageLayout&, const QString&);
    quint64 printToPDFCallbackResult(const QPageLayout &, const bool colorMode = true);

    // meant to be used within WebEngineCore only
    content::WebContents *webContents() const;
    void replaceMisspelling(const QString &word);

    void viewSource();
    bool canViewSource();
    void focusIfNecessary();


private:
    Q_DISABLE_COPY(WebContentsAdapter)
    Q_DECLARE_PRIVATE(WebContentsAdapter)
    QScopedPointer<WebContentsAdapterPrivate> d_ptr;

};

} // namespace QtWebEngineCore

#endif // WEB_CONTENTS_ADAPTER_H
