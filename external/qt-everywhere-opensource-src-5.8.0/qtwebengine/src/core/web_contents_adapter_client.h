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

#ifndef WEB_CONTENTS_ADAPTER_CLIENT_H
#define WEB_CONTENTS_ADAPTER_CLIENT_H

#include "qtwebenginecoreglobal.h"

#include <QFlags>
#include <QRect>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

QT_FORWARD_DECLARE_CLASS(QKeyEvent)
QT_FORWARD_DECLARE_CLASS(QVariant)
QT_FORWARD_DECLARE_CLASS(CertificateErrorController)

namespace content {
struct DropData;
}

namespace QtWebEngineCore {

class AuthenticationDialogController;
class BrowserContextAdapter;
class ColorChooserController;
class FilePickerController;
class JavaScriptDialogController;
class RenderWidgetHostViewQt;
class RenderWidgetHostViewQtDelegate;
class RenderWidgetHostViewQtDelegateClient;
class WebContentsAdapter;
class WebContentsDelegateQt;
class WebEngineSettings;


class WebEngineContextMenuSharedData : public QSharedData {

public:
    WebEngineContextMenuSharedData()
        :  hasImageContent(false)
        , isEditable(false)
        , isSpellCheckerEnabled(false)
        , mediaType(0)
        , mediaFlags(0)
    {
    }
    bool hasImageContent;
    bool isEditable;
    bool isSpellCheckerEnabled;
    uint mediaType;
    uint mediaFlags;
    QPoint pos;
    QUrl linkUrl;
    QUrl mediaUrl;
    QString linkText;
    QString selectedText;
    QString suggestedFileName;
    QString misspelledWord;
    QStringList spellCheckerSuggestions;
    // Some likely candidates for future additions as we add support for the related actions:
    //    bool isImageBlocked;
    //    <enum tbd> mediaType;
    //    ...
};

class WebEngineContextMenuData {
public:
    // Must match blink::WebContextMenuData::MediaType:
    enum MediaType {
        // No special node is in context.
        MediaTypeNone = 0x0,
        // An image node is selected.
        MediaTypeImage,
        // A video node is selected.
        MediaTypeVideo,
        // An audio node is selected.
        MediaTypeAudio,
        // A canvas node is selected.
        MediaTypeCanvas,
        // A file node is selected.
        MediaTypeFile,
        // A plugin node is selected.
        MediaTypePlugin,
        MediaTypeLast = MediaTypePlugin
    };
    // Must match blink::WebContextMenuData::MediaFlags:
    enum MediaFlags {
        MediaNone = 0x0,
        MediaInError = 0x1,
        MediaPaused = 0x2,
        MediaMuted = 0x4,
        MediaLoop = 0x8,
        MediaCanSave = 0x10,
        MediaHasAudio = 0x20,
        MediaCanToggleControls = 0x40,
        MediaControls = 0x80,
        MediaCanPrint = 0x100,
        MediaCanRotate = 0x200,
    };

    WebEngineContextMenuData():d(new WebEngineContextMenuSharedData) {
    }

    void setPosition(const QPoint &pos) {
        d->pos = pos;
    }

    QPoint position() const {
        return d->pos;
    }

    void setLinkUrl(const QUrl &url) {
        d->linkUrl = url;
    }

    QUrl linkUrl() const {
        return d->linkUrl;
    }

    void setLinkText(const QString &text) {
        d->linkText = text;
    }

    QString linkText() const {
        return d->linkText;
    }

    void setSelectedText(const QString &text) {
        d->selectedText = text;
    }

    QString selectedText() const {
        return d->selectedText;
    }

    void setMediaUrl(const QUrl &url) {
        d->mediaUrl = url;
    }

    QUrl mediaUrl() const {
        return d->mediaUrl;
    }

    void setMediaType(MediaType type) {
        d->mediaType = type;
    }

    MediaType mediaType() const {
        return MediaType(d->mediaType);
    }

    void setHasImageContent(bool imageContent) {
        d->hasImageContent = imageContent;
    }

    bool hasImageContent() const {
        return d->hasImageContent;
    }

    void setMediaFlags(MediaFlags flags) {
        d->mediaFlags = flags;
    }

    MediaFlags mediaFlags() const {
        return MediaFlags(d->mediaFlags);
    }

    void setSuggestedFileName(const QString &filename) {
        d->suggestedFileName = filename;
    }

    QString suggestedFileName() const {
        return d->suggestedFileName;
    }

    void setIsEditable(bool editable) {
        d->isEditable = editable;
    }

    bool isEditable() const {
        return d->isEditable;
    }

    void setIsSpellCheckerEnabled(bool spellCheckerEnabled) {
        d->isSpellCheckerEnabled = spellCheckerEnabled;
    }

    bool isSpellCheckerEnabled() const {
        return d->isSpellCheckerEnabled;
    }

    void setMisspelledWord(const QString &word) {
        d->misspelledWord = word;
    }

    QString misspelledWord() const {
        return d->misspelledWord;
    }

    void setSpellCheckerSuggestions(const QStringList &suggestions) {
        d->spellCheckerSuggestions = suggestions;
    }

    QStringList spellCheckerSuggestions() const {
        return d->spellCheckerSuggestions;
    }

private:
    QSharedDataPointer<WebEngineContextMenuSharedData> d;
};


class QWEBENGINE_EXPORT WebContentsAdapterClient {
public:
    // This must match window_open_disposition_list.h.
    enum WindowOpenDisposition {
        UnknownDisposition = 0,
        SuppressOpenDisposition = 1,
        CurrentTabDisposition = 2,
        SingletonTabDisposition = 3,
        NewForegroundTabDisposition = 4,
        NewBackgroundTabDisposition = 5,
        NewPopupDisposition = 6,
        NewWindowDisposition = 7,
        SaveToDiskDisposition = 8,
        OffTheRecordDisposition = 9,
        IgnoreActionDisposition = 10,
    };

    // Must match the values in javascript_message_type.h.
    enum JavascriptDialogType {
        AlertDialog,
        ConfirmDialog,
        PromptDialog,
        UnloadDialog,
        // Leave room for potential new specs
        InternalAuthorizationDialog = 0x10,
    };

    enum NavigationRequestAction {
        AcceptRequest,
        // Make room in the valid range of the enum for extra actions exposed in Experimental.
        IgnoreRequest = 0xFF
    };

    enum NavigationType {
        LinkNavigation,
        TypedNavigation,
        FormSubmittedNavigation,
        BackForwardNavigation,
        ReloadNavigation,
        OtherNavigation
    };

    enum JavaScriptConsoleMessageLevel {
        Info = 0,
        Warning,
        Error
    };

    enum RenderProcessTerminationStatus {
        NormalTerminationStatus = 0,
        AbnormalTerminationStatus,
        CrashedTerminationStatus,
        KilledTerminationStatus
    };

    enum MediaRequestFlag {
        MediaNone = 0,
        MediaAudioCapture = 0x01,
        MediaVideoCapture = 0x02,
    };
    Q_DECLARE_FLAGS(MediaRequestFlags, MediaRequestFlag)

    virtual ~WebContentsAdapterClient() { }

    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegate(RenderWidgetHostViewQtDelegateClient *client) = 0;
    virtual RenderWidgetHostViewQtDelegate* CreateRenderWidgetHostViewQtDelegateForPopup(RenderWidgetHostViewQtDelegateClient *client) = 0;
    virtual void titleChanged(const QString&) = 0;
    virtual void urlChanged(const QUrl&) = 0;
    virtual void iconChanged(const QUrl&) = 0;
    virtual void loadProgressChanged(int progress) = 0;
    virtual void didUpdateTargetURL(const QUrl&) = 0;
    virtual void selectionChanged() = 0;
    virtual void recentlyAudibleChanged(bool recentlyAudible) = 0;
    virtual QRectF viewportRect() const = 0;
    virtual qreal dpiScale() const = 0;
    virtual QColor backgroundColor() const = 0;
    virtual void loadStarted(const QUrl &provisionalUrl, bool isErrorPage = false) = 0;
    virtual void loadCommitted() = 0;
    virtual void loadVisuallyCommitted() = 0;
    virtual void loadFinished(bool success, const QUrl &url, bool isErrorPage = false, int errorCode = 0, const QString &errorDescription = QString()) = 0;
    virtual void focusContainer() = 0;
    virtual void unhandledKeyEvent(QKeyEvent *event) = 0;
    virtual void adoptNewWindow(QSharedPointer<WebContentsAdapter> newWebContents, WindowOpenDisposition disposition, bool userGesture, const QRect & initialGeometry) = 0;
    virtual bool isBeingAdopted() = 0;
    virtual void close() = 0;
    virtual void windowCloseRejected() = 0;
    virtual bool contextMenuRequested(const WebEngineContextMenuData &) = 0;
    virtual void navigationRequested(int navigationType, const QUrl &url, int &navigationRequestAction, bool isMainFrame) = 0;
    virtual void requestFullScreenMode(const QUrl &origin, bool fullscreen) = 0;
    virtual bool isFullScreenMode() const = 0;
    virtual void javascriptDialog(QSharedPointer<JavaScriptDialogController>) = 0;
    virtual void runFileChooser(QSharedPointer<FilePickerController>) = 0;
    virtual void showColorDialog(QSharedPointer<ColorChooserController>) = 0;
    virtual void didRunJavaScript(quint64 requestId, const QVariant& result) = 0;
    virtual void didFetchDocumentMarkup(quint64 requestId, const QString& result) = 0;
    virtual void didFetchDocumentInnerText(quint64 requestId, const QString& result) = 0;
    virtual void didFindText(quint64 requestId, int matchCount) = 0;
    virtual void didPrintPage(quint64 requestId, const QByteArray &result) = 0;
    virtual void passOnFocus(bool reverse) = 0;
    // returns the last QObject (QWidget/QQuickItem) based object in the accessibility
    // hierarchy before going into the BrowserAccessibility tree
#ifndef QT_NO_ACCESSIBILITY
    virtual QObject *accessibilityParentObject() = 0;
#endif // QT_NO_ACCESSIBILITY
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID) = 0;
    virtual void authenticationRequired(QSharedPointer<AuthenticationDialogController>) = 0;
    virtual void runGeolocationPermissionRequest(const QUrl &securityOrigin) = 0;
    virtual void runMediaAccessPermissionRequest(const QUrl &securityOrigin, MediaRequestFlags requestFlags) = 0;
    virtual void runMouseLockPermissionRequest(const QUrl &securityOrigin) = 0;
    virtual WebEngineSettings *webEngineSettings() const = 0;
    virtual void showValidationMessage(const QRect &anchor, const QString &mainText, const QString &subText) = 0;
    virtual void hideValidationMessage() = 0;
    virtual void moveValidationMessage(const QRect &anchor) = 0;
    RenderProcessTerminationStatus renderProcessExitStatus(int);
    virtual void renderProcessTerminated(RenderProcessTerminationStatus terminationStatus, int exitCode) = 0;
    virtual void requestGeometryChange(const QRect &geometry) = 0;
    virtual void allowCertificateError(const QSharedPointer<CertificateErrorController> &errorController) = 0;
    virtual void updateScrollPosition(const QPointF &position) = 0;
    virtual void updateContentsSize(const QSizeF &size) = 0;
    virtual void startDragging(const content::DropData &dropData, Qt::DropActions allowedActions,
                               const QPixmap &pixmap, const QPoint &offset) = 0;
    virtual bool isEnabled() const = 0;
    virtual const QObject *holdingQObject() const = 0;
    virtual void setToolTip(const QString& toolTipText) = 0;

    virtual QSharedPointer<BrowserContextAdapter> browserContextAdapter() = 0;
    virtual WebContentsAdapter* webContentsAdapter() = 0;

};

} // namespace QtWebEngineCore

#endif // WEB_CONTENTS_ADAPTER_CLIENT_H
