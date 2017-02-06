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

#ifndef QQUICKWEBENGINEVIEW_P_H
#define QQUICKWEBENGINEVIEW_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qtwebengineglobal_p.h>
#include "qquickwebenginescript_p.h"
#include <QQuickItem>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QQmlWebChannel;
class QQuickWebEngineAuthenticationDialogRequest;
class QQuickWebEngineCertificateError;
class QQuickWebEngineColorDialogRequest;
class QQuickWebEngineContextMenuRequest;
class QQuickWebEngineFaviconProvider;
class QQuickWebEngineFileDialogRequest;
class QQuickWebEngineHistory;
class QQuickWebEngineJavaScriptDialogRequest;
class QQuickWebEngineLoadRequest;
class QQuickWebEngineNavigationRequest;
class QQuickWebEngineNewViewRequest;
class QQuickWebEngineProfile;
class QQuickWebEngineSettings;
class QQuickWebEngineFormValidationMessageRequest;
class QQuickWebEngineViewPrivate;

#ifdef ENABLE_QML_TESTSUPPORT_API
class QQuickWebEngineTestSupport;
#endif

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineFullScreenRequest {
    Q_GADGET
    Q_PROPERTY(QUrl origin READ origin)
    Q_PROPERTY(bool toggleOn READ toggleOn)
public:
    QQuickWebEngineFullScreenRequest();
    QQuickWebEngineFullScreenRequest(QQuickWebEngineViewPrivate *viewPrivate, const QUrl &origin, bool toggleOn);

    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    QUrl origin() const { return m_origin; }
    bool toggleOn() const { return m_toggleOn; }

private:
    QQuickWebEngineViewPrivate *m_viewPrivate;
    const QUrl m_origin;
    const bool m_toggleOn;
};

#define LATEST_WEBENGINEVIEW_REVISION 4

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QUrl icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY urlChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY urlChanged)
    Q_PROPERTY(bool isFullScreen READ isFullScreen NOTIFY isFullScreenChanged REVISION 1)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged REVISION 1)
    Q_PROPERTY(QQuickWebEngineProfile *profile READ profile WRITE setProfile NOTIFY profileChanged FINAL REVISION 1)
    Q_PROPERTY(QQuickWebEngineSettings *settings READ settings REVISION 1)
    Q_PROPERTY(QQuickWebEngineHistory *navigationHistory READ navigationHistory CONSTANT FINAL REVISION 1)
    Q_PROPERTY(QQmlWebChannel *webChannel READ webChannel WRITE setWebChannel NOTIFY webChannelChanged REVISION 1)
    Q_PROPERTY(QQmlListProperty<QQuickWebEngineScript> userScripts READ userScripts FINAL REVISION 1)
    Q_PROPERTY(bool activeFocusOnPress READ activeFocusOnPress WRITE setActiveFocusOnPress NOTIFY activeFocusOnPressChanged REVISION 2)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged REVISION 2)
    Q_PROPERTY(QSizeF contentsSize READ contentsSize NOTIFY contentsSizeChanged FINAL REVISION 3)
    Q_PROPERTY(QPointF scrollPosition READ scrollPosition NOTIFY scrollPositionChanged FINAL REVISION 3)
    Q_PROPERTY(bool audioMuted READ isAudioMuted WRITE setAudioMuted NOTIFY audioMutedChanged FINAL REVISION 3)
    Q_PROPERTY(bool recentlyAudible READ recentlyAudible NOTIFY recentlyAudibleChanged FINAL REVISION 3)
    Q_PROPERTY(uint webChannelWorld READ webChannelWorld WRITE setWebChannelWorld NOTIFY webChannelWorldChanged REVISION 3)

#ifdef ENABLE_QML_TESTSUPPORT_API
    Q_PROPERTY(QQuickWebEngineTestSupport *testSupport READ testSupport WRITE setTestSupport FINAL)
#endif

    Q_FLAGS(FindFlags);

public:
    QQuickWebEngineView(QQuickItem *parent = 0);
    ~QQuickWebEngineView();

    QUrl url() const;
    void setUrl(const QUrl&);
    QUrl icon() const;
    bool isLoading() const;
    int loadProgress() const;
    QString title() const;
    bool canGoBack() const;
    bool canGoForward() const;
    bool isFullScreen() const;
    qreal zoomFactor() const;
    void setZoomFactor(qreal arg);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    QSizeF contentsSize() const;
    QPointF scrollPosition() const;

    // must match WebContentsAdapterClient::NavigationRequestAction
    enum NavigationRequestAction {
        AcceptRequest,
        // Make room in the valid range of the enum so
        // we can expose extra actions.
        IgnoreRequest = 0xFF
    };
    Q_ENUM(NavigationRequestAction)

    // must match WebContentsAdapterClient::NavigationType
    enum NavigationType {
        LinkClickedNavigation,
        TypedNavigation,
        FormSubmittedNavigation,
        BackForwardNavigation,
        ReloadNavigation,
        OtherNavigation
    };
    Q_ENUM(NavigationType)

    enum LoadStatus {
        LoadStartedStatus,
        LoadStoppedStatus,
        LoadSucceededStatus,
        LoadFailedStatus
    };
    Q_ENUM(LoadStatus)

    enum ErrorDomain {
         NoErrorDomain,
         InternalErrorDomain,
         ConnectionErrorDomain,
         CertificateErrorDomain,
         HttpErrorDomain,
         FtpErrorDomain,
         DnsErrorDomain
    };
    Q_ENUM(ErrorDomain)

    enum NewViewDestination {
        NewViewInWindow,
        NewViewInTab,
        NewViewInDialog,
        NewViewInBackgroundTab
    };
    Q_ENUM(NewViewDestination)

    enum Feature {
        MediaAudioCapture,
        MediaVideoCapture,
        MediaAudioVideoCapture,
        Geolocation
    };
    Q_ENUM(Feature)

    enum WebAction {
        NoWebAction = - 1,
        Back,
        Forward,
        Stop,
        Reload,

        Cut,
        Copy,
        Paste,

        Undo,
        Redo,
        SelectAll,
        ReloadAndBypassCache,

        PasteAndMatchStyle,

        OpenLinkInThisWindow,
        OpenLinkInNewWindow,
        OpenLinkInNewTab,
        CopyLinkToClipboard,
        DownloadLinkToDisk,

        CopyImageToClipboard,
        CopyImageUrlToClipboard,
        DownloadImageToDisk,

        CopyMediaUrlToClipboard,
        ToggleMediaControls,
        ToggleMediaLoop,
        ToggleMediaPlayPause,
        ToggleMediaMute,
        DownloadMediaToDisk,

        InspectElement,
        ExitFullScreen,
        RequestClose,
        Unselect,
        SavePage,
        ViewSource,
        WebActionCount
    };
    Q_ENUM(WebAction)

    // must match WebContentsAdapterClient::JavaScriptConsoleMessageLevel
    enum JavaScriptConsoleMessageLevel {
        InfoMessageLevel = 0,
        WarningMessageLevel,
        ErrorMessageLevel
    };
    Q_ENUM(JavaScriptConsoleMessageLevel)

    // must match WebContentsAdapterClient::RenderProcessTerminationStatus
    enum RenderProcessTerminationStatus {
        NormalTerminationStatus = 0,
        AbnormalTerminationStatus,
        CrashedTerminationStatus,
        KilledTerminationStatus
    };
    Q_ENUM(RenderProcessTerminationStatus)

    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    // must match QPageSize::PageSizeId
    enum PrintedPageSizeId {
        // Existing Qt sizes
        A4,
        B5,
        Letter,
        Legal,
        Executive,
        A0,
        A1,
        A2,
        A3,
        A5,
        A6,
        A7,
        A8,
        A9,
        B0,
        B1,
        B10,
        B2,
        B3,
        B4,
        B6,
        B7,
        B8,
        B9,
        C5E,
        Comm10E,
        DLE,
        Folio,
        Ledger,
        Tabloid,
        Custom,

        // New values derived from PPD standard
        A10,
        A3Extra,
        A4Extra,
        A4Plus,
        A4Small,
        A5Extra,
        B5Extra,

        JisB0,
        JisB1,
        JisB2,
        JisB3,
        JisB4,
        JisB5,
        JisB6,
        JisB7,
        JisB8,
        JisB9,
        JisB10,

        // AnsiA = Letter,
        // AnsiB = Ledger,
        AnsiC,
        AnsiD,
        AnsiE,
        LegalExtra,
        LetterExtra,
        LetterPlus,
        LetterSmall,
        TabloidExtra,

        ArchA,
        ArchB,
        ArchC,
        ArchD,
        ArchE,

        Imperial7x9,
        Imperial8x10,
        Imperial9x11,
        Imperial9x12,
        Imperial10x11,
        Imperial10x13,
        Imperial10x14,
        Imperial12x11,
        Imperial15x11,

        ExecutiveStandard,
        Note,
        Quarto,
        Statement,
        SuperA,
        SuperB,
        Postcard,
        DoublePostcard,
        Prc16K,
        Prc32K,
        Prc32KBig,

        FanFoldUS,
        FanFoldGerman,
        FanFoldGermanLegal,

        EnvelopeB4,
        EnvelopeB5,
        EnvelopeB6,
        EnvelopeC0,
        EnvelopeC1,
        EnvelopeC2,
        EnvelopeC3,
        EnvelopeC4,
        // EnvelopeC5 = C5E,
        EnvelopeC6,
        EnvelopeC65,
        EnvelopeC7,
        // EnvelopeDL = DLE,

        Envelope9,
        // Envelope10 = Comm10E,
        Envelope11,
        Envelope12,
        Envelope14,
        EnvelopeMonarch,
        EnvelopePersonal,

        EnvelopeChou3,
        EnvelopeChou4,
        EnvelopeInvite,
        EnvelopeItalian,
        EnvelopeKaku2,
        EnvelopeKaku3,
        EnvelopePrc1,
        EnvelopePrc2,
        EnvelopePrc3,
        EnvelopePrc4,
        EnvelopePrc5,
        EnvelopePrc6,
        EnvelopePrc7,
        EnvelopePrc8,
        EnvelopePrc9,
        EnvelopePrc10,
        EnvelopeYou4,

        // Last item, with commonly used synynoms from QPagedPrintEngine / QPrinter
        LastPageSize = EnvelopeYou4,
        NPageSize = LastPageSize,
        NPaperSize = LastPageSize,

        // Convenience overloads for naming consistency
        AnsiA = Letter,
        AnsiB = Ledger,
        EnvelopeC5 = C5E,
        EnvelopeDL = DLE,
        Envelope10 = Comm10E
    };
    Q_ENUM(PrintedPageSizeId)

    // must match QPageLayout::Orientation
    enum PrintedPageOrientation {
        Portrait,
        Landscape
    };
    Q_ENUM(PrintedPageOrientation)

    // QmlParserStatus
    virtual void componentComplete() Q_DECL_OVERRIDE;

    QQuickWebEngineProfile *profile() const;
    void setProfile(QQuickWebEngineProfile *);
    QQmlListProperty<QQuickWebEngineScript> userScripts();

    QQuickWebEngineSettings *settings() const;
    QQmlWebChannel *webChannel();
    void setWebChannel(QQmlWebChannel *);
    QQuickWebEngineHistory *navigationHistory() const;
    uint webChannelWorld() const;
    void setWebChannelWorld(uint);

    bool isAudioMuted() const;
    void setAudioMuted(bool muted);
    bool recentlyAudible() const;

#ifdef ENABLE_QML_TESTSUPPORT_API
    QQuickWebEngineTestSupport *testSupport() const;
    void setTestSupport(QQuickWebEngineTestSupport *testSupport);
#endif

    bool activeFocusOnPress() const;

public Q_SLOTS:
    void runJavaScript(const QString&, const QJSValue & = QJSValue());
    Q_REVISION(3) void runJavaScript(const QString&, quint32 worldId, const QJSValue & = QJSValue());
    void loadHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void goBack();
    void goForward();
    Q_REVISION(1) void goBackOrForward(int index);
    void reload();
    Q_REVISION(1) void reloadAndBypassCache();
    void stop();
    Q_REVISION(1) void findText(const QString &subString, FindFlags options = 0, const QJSValue &callback = QJSValue());
    Q_REVISION(1) void fullScreenCancelled();
    Q_REVISION(1) void grantFeaturePermission(const QUrl &securityOrigin, Feature, bool granted);
    Q_REVISION(2) void setActiveFocusOnPress(bool arg);
    Q_REVISION(2) void triggerWebAction(WebAction action);
    Q_REVISION(3) void printToPdf(const QString &filePath, PrintedPageSizeId pageSizeId = PrintedPageSizeId::A4, PrintedPageOrientation orientation = PrintedPageOrientation::Portrait);
    Q_REVISION(3) void printToPdf(const QJSValue &callback, PrintedPageSizeId pageSizeId = PrintedPageSizeId::A4, PrintedPageOrientation orientation = PrintedPageOrientation::Portrait);
    Q_REVISION(4) void replaceMisspelledWord(const QString &replacement);

private Q_SLOTS:
    void lazyInitialize();

Q_SIGNALS:
    void titleChanged();
    void urlChanged();
    void iconChanged();
    void loadingChanged(QQuickWebEngineLoadRequest *loadRequest);
    void loadProgressChanged();
    void linkHovered(const QUrl &hoveredUrl);
    void navigationRequested(QQuickWebEngineNavigationRequest *request);
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID);
    Q_REVISION(1) void certificateError(QQuickWebEngineCertificateError *error);
    Q_REVISION(1) void fullScreenRequested(const QQuickWebEngineFullScreenRequest &request);
    Q_REVISION(1) void isFullScreenChanged();
    Q_REVISION(1) void featurePermissionRequested(const QUrl &securityOrigin, Feature feature);
    Q_REVISION(1) void newViewRequested(QQuickWebEngineNewViewRequest *request);
    Q_REVISION(1) void zoomFactorChanged(qreal arg);
    Q_REVISION(1) void profileChanged();
    Q_REVISION(1) void webChannelChanged();
    Q_REVISION(2) void activeFocusOnPressChanged(bool);
    Q_REVISION(2) void backgroundColorChanged();
    Q_REVISION(2) void renderProcessTerminated(RenderProcessTerminationStatus terminationStatus, int exitCode);
    Q_REVISION(2) void windowCloseRequested();
    Q_REVISION(3) void contentsSizeChanged(const QSizeF& size);
    Q_REVISION(3) void scrollPositionChanged(const QPointF& position);
    Q_REVISION(3) void audioMutedChanged(bool muted);
    Q_REVISION(3) void recentlyAudibleChanged(bool recentlyAudible);
    Q_REVISION(3) void webChannelWorldChanged(uint);
    Q_REVISION(4) void contextMenuRequested(QQuickWebEngineContextMenuRequest *request);
    Q_REVISION(4) void authenticationDialogRequested(QQuickWebEngineAuthenticationDialogRequest *request);
    Q_REVISION(4) void javaScriptDialogRequested(QQuickWebEngineJavaScriptDialogRequest *request);
    Q_REVISION(4) void colorDialogRequested(QQuickWebEngineColorDialogRequest *request);
    Q_REVISION(4) void fileDialogRequested(QQuickWebEngineFileDialogRequest *request);
    Q_REVISION(4) void formValidationMessageRequested(QQuickWebEngineFormValidationMessageRequest *request);

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) Q_DECL_OVERRIDE;
    void itemChange(ItemChange, const ItemChangeData &) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *e) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *e) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent *e) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *e) Q_DECL_OVERRIDE;

private:
    Q_DECLARE_PRIVATE(QQuickWebEngineView)
    QScopedPointer<QQuickWebEngineViewPrivate> d_ptr;

    friend class QQuickWebEngineNewViewRequest;
    friend class QQuickWebEngineFaviconProvider;
#ifndef QT_NO_ACCESSIBILITY
    friend class QQuickWebEngineViewAccessible;
#endif // QT_NO_ACCESSIBILITY
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineView)
Q_DECLARE_METATYPE(QQuickWebEngineFullScreenRequest)

#endif // QQUICKWEBENGINEVIEW_P_H
