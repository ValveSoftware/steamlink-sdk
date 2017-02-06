/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QQmlListProperty>
#include <QtTest/QtTest>
#include <QtWebEngine/QQuickWebEngineProfile>
#include <private/qquickwebengineview_p.h>
#include <private/qquickwebenginecertificateerror_p.h>
#include <private/qquickwebenginedialogrequests_p.h>
#include <private/qquickwebenginedownloaditem_p.h>
#include <private/qquickwebenginehistory_p.h>
#include <private/qquickwebengineloadrequest_p.h>
#include <private/qquickwebenginenavigationrequest_p.h>
#include <private/qquickwebenginenewviewrequest_p.h>
#include <private/qquickwebenginescript_p.h>
#include <private/qquickwebenginesettings_p.h>
#include <private/qquickwebenginesingleton_p.h>
#include <private/qquickwebenginecontextmenurequest_p.h>

class tst_publicapi : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void publicAPI();
};

static QList<const QMetaObject *> typesToCheck = QList<const QMetaObject *>()
    << &QQuickWebEngineView::staticMetaObject
    << &QQuickWebEngineCertificateError::staticMetaObject
    << &QQuickWebEngineDownloadItem::staticMetaObject
    << &QQuickWebEngineHistory::staticMetaObject
    << &QQuickWebEngineHistoryListModel::staticMetaObject
    << &QQuickWebEngineLoadRequest::staticMetaObject
    << &QQuickWebEngineNavigationRequest::staticMetaObject
    << &QQuickWebEngineNewViewRequest::staticMetaObject
    << &QQuickWebEngineProfile::staticMetaObject
    << &QQuickWebEngineScript::staticMetaObject
    << &QQuickWebEngineSettings::staticMetaObject
    << &QQuickWebEngineFullScreenRequest::staticMetaObject
    << &QQuickWebEngineSingleton::staticMetaObject
    << &QQuickWebEngineAuthenticationDialogRequest::staticMetaObject
    << &QQuickWebEngineJavaScriptDialogRequest::staticMetaObject
    << &QQuickWebEngineColorDialogRequest::staticMetaObject
    << &QQuickWebEngineFileDialogRequest::staticMetaObject
    << &QQuickWebEngineFormValidationMessageRequest::staticMetaObject
    << &QQuickWebEngineContextMenuRequest::staticMetaObject
    ;

static QList<const char *> knownEnumNames = QList<const char *>();

static QStringList hardcodedTypes = QStringList()
    << "QJSValue"
    << "QQmlListProperty<QQuickWebEngineScript>"
    << "QQmlWebChannel*"
    // Ignore the testSupport types without making a fuss.
    << "QQuickWebEngineTestSupport*"
    << "QQuickWebEngineErrorPage*"
    << "const QQuickWebEngineContextMenuData*"
    << "QWebEngineCookieStore*"
    ;

static QStringList expectedAPI = QStringList()
    << "QQuickWebEngineView.AcceptRequest --> NavigationRequestAction"
    << "QQuickWebEngineView.IgnoreRequest --> NavigationRequestAction"
    << "QQuickWebEngineView.LinkClickedNavigation --> NavigationType"
    << "QQuickWebEngineView.TypedNavigation --> NavigationType"
    << "QQuickWebEngineView.FormSubmittedNavigation --> NavigationType"
    << "QQuickWebEngineView.BackForwardNavigation --> NavigationType"
    << "QQuickWebEngineView.ReloadNavigation --> NavigationType"
    << "QQuickWebEngineView.OtherNavigation --> NavigationType"
    << "QQuickWebEngineView.LoadStartedStatus --> LoadStatus"
    << "QQuickWebEngineView.LoadStoppedStatus --> LoadStatus"
    << "QQuickWebEngineView.LoadSucceededStatus --> LoadStatus"
    << "QQuickWebEngineView.LoadFailedStatus --> LoadStatus"
    << "QQuickWebEngineView.NoErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.InternalErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.ConnectionErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.CertificateErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.HttpErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.FtpErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.DnsErrorDomain --> ErrorDomain"
    << "QQuickWebEngineView.NewViewInWindow --> NewViewDestination"
    << "QQuickWebEngineView.NewViewInTab --> NewViewDestination"
    << "QQuickWebEngineView.NewViewInDialog --> NewViewDestination"
    << "QQuickWebEngineView.NewViewInBackgroundTab --> NewViewDestination"
    << "QQuickWebEngineView.MediaAudioCapture --> Feature"
    << "QQuickWebEngineView.MediaVideoCapture --> Feature"
    << "QQuickWebEngineView.MediaAudioVideoCapture --> Feature"
    << "QQuickWebEngineView.Geolocation --> Feature"
    << "QQuickWebEngineView.NoWebAction --> WebAction"
    << "QQuickWebEngineView.Back --> WebAction"
    << "QQuickWebEngineView.Forward --> WebAction"
    << "QQuickWebEngineView.Stop --> WebAction"
    << "QQuickWebEngineView.Reload --> WebAction"
    << "QQuickWebEngineView.Cut --> WebAction"
    << "QQuickWebEngineView.Copy --> WebAction"
    << "QQuickWebEngineView.Paste --> WebAction"
    << "QQuickWebEngineView.Undo --> WebAction"
    << "QQuickWebEngineView.Redo --> WebAction"
    << "QQuickWebEngineView.SelectAll --> WebAction"
    << "QQuickWebEngineView.ReloadAndBypassCache --> WebAction"
    << "QQuickWebEngineView.PasteAndMatchStyle --> WebAction"
    << "QQuickWebEngineView.OpenLinkInThisWindow --> WebAction"
    << "QQuickWebEngineView.OpenLinkInNewWindow --> WebAction"
    << "QQuickWebEngineView.OpenLinkInNewTab --> WebAction"
    << "QQuickWebEngineView.CopyLinkToClipboard --> WebAction"
    << "QQuickWebEngineView.DownloadLinkToDisk --> WebAction"
    << "QQuickWebEngineView.CopyImageToClipboard --> WebAction"
    << "QQuickWebEngineView.CopyImageUrlToClipboard --> WebAction"
    << "QQuickWebEngineView.DownloadImageToDisk --> WebAction"
    << "QQuickWebEngineView.CopyMediaUrlToClipboard --> WebAction"
    << "QQuickWebEngineView.ToggleMediaControls --> WebAction"
    << "QQuickWebEngineView.ToggleMediaLoop --> WebAction"
    << "QQuickWebEngineView.ToggleMediaPlayPause --> WebAction"
    << "QQuickWebEngineView.ToggleMediaMute --> WebAction"
    << "QQuickWebEngineView.DownloadMediaToDisk --> WebAction"
    << "QQuickWebEngineView.InspectElement --> WebAction"
    << "QQuickWebEngineView.ExitFullScreen --> WebAction"
    << "QQuickWebEngineView.RequestClose --> WebAction"
    << "QQuickWebEngineView.WebActionCount --> WebAction"
    << "QQuickWebEngineView.InfoMessageLevel --> JavaScriptConsoleMessageLevel"
    << "QQuickWebEngineView.WarningMessageLevel --> JavaScriptConsoleMessageLevel"
    << "QQuickWebEngineView.ErrorMessageLevel --> JavaScriptConsoleMessageLevel"
    << "QQuickWebEngineView.NormalTerminationStatus --> RenderProcessTerminationStatus"
    << "QQuickWebEngineView.AbnormalTerminationStatus --> RenderProcessTerminationStatus"
    << "QQuickWebEngineView.CrashedTerminationStatus --> RenderProcessTerminationStatus"
    << "QQuickWebEngineView.KilledTerminationStatus --> RenderProcessTerminationStatus"
    << "QQuickWebEngineView.FindBackward --> FindFlags"
    << "QQuickWebEngineView.FindCaseSensitively --> FindFlags"
    << "QQuickWebEngineView.url --> QUrl"
    << "QQuickWebEngineView.icon --> QUrl"
    << "QQuickWebEngineView.loading --> bool"
    << "QQuickWebEngineView.loadProgress --> int"
    << "QQuickWebEngineView.title --> QString"
    << "QQuickWebEngineView.canGoBack --> bool"
    << "QQuickWebEngineView.canGoForward --> bool"
    << "QQuickWebEngineView.isFullScreen --> bool"
    << "QQuickWebEngineView.zoomFactor --> double"
    << "QQuickWebEngineView.profile --> QQuickWebEngineProfile*"
    << "QQuickWebEngineView.settings --> QQuickWebEngineSettings*"
    << "QQuickWebEngineView.navigationHistory --> QQuickWebEngineHistory*"
    << "QQuickWebEngineView.webChannel --> QQmlWebChannel*"
    << "QQuickWebEngineView.userScripts --> QQmlListProperty<QQuickWebEngineScript>"
    << "QQuickWebEngineView.activeFocusOnPress --> bool"
    << "QQuickWebEngineView.backgroundColor --> QColor"
    << "QQuickWebEngineView.testSupport --> QQuickWebEngineTestSupport*"
    << "QQuickWebEngineView.titleChanged() --> void"
    << "QQuickWebEngineView.urlChanged() --> void"
    << "QQuickWebEngineView.iconChanged() --> void"
    << "QQuickWebEngineView.loadingChanged(QQuickWebEngineLoadRequest*) --> void"
    << "QQuickWebEngineView.loadProgressChanged() --> void"
    << "QQuickWebEngineView.linkHovered(QUrl) --> void"
    << "QQuickWebEngineView.navigationRequested(QQuickWebEngineNavigationRequest*) --> void"
    << "QQuickWebEngineView.javaScriptConsoleMessage(JavaScriptConsoleMessageLevel,QString,int,QString) --> void"
    << "QQuickWebEngineView.certificateError(QQuickWebEngineCertificateError*) --> void"
    << "QQuickWebEngineView.fullScreenRequested(QQuickWebEngineFullScreenRequest) --> void"
    << "QQuickWebEngineView.isFullScreenChanged() --> void"
    << "QQuickWebEngineView.featurePermissionRequested(QUrl,Feature) --> void"
    << "QQuickWebEngineView.newViewRequested(QQuickWebEngineNewViewRequest*) --> void"
    << "QQuickWebEngineView.zoomFactorChanged(double) --> void"
    << "QQuickWebEngineView.profileChanged() --> void"
    << "QQuickWebEngineView.webChannelChanged() --> void"
    << "QQuickWebEngineView.activeFocusOnPressChanged(bool) --> void"
    << "QQuickWebEngineView.backgroundColorChanged() --> void"
    << "QQuickWebEngineView.renderProcessTerminated(RenderProcessTerminationStatus,int) --> void"
    << "QQuickWebEngineView.windowCloseRequested() --> void"
    << "QQuickWebEngineView.runJavaScript(QString,QJSValue) --> void"
    << "QQuickWebEngineView.runJavaScript(QString) --> void"
    << "QQuickWebEngineView.loadHtml(QString,QUrl) --> void"
    << "QQuickWebEngineView.loadHtml(QString) --> void"
    << "QQuickWebEngineView.goBack() --> void"
    << "QQuickWebEngineView.goForward() --> void"
    << "QQuickWebEngineView.goBackOrForward(int) --> void"
    << "QQuickWebEngineView.reload() --> void"
    << "QQuickWebEngineView.reloadAndBypassCache() --> void"
    << "QQuickWebEngineView.stop() --> void"
    << "QQuickWebEngineView.findText(QString,FindFlags,QJSValue) --> void"
    << "QQuickWebEngineView.findText(QString,FindFlags) --> void"
    << "QQuickWebEngineView.findText(QString) --> void"
    << "QQuickWebEngineView.fullScreenCancelled() --> void"
    << "QQuickWebEngineView.grantFeaturePermission(QUrl,Feature,bool) --> void"
    << "QQuickWebEngineView.setActiveFocusOnPress(bool) --> void"
    << "QQuickWebEngineView.triggerWebAction(WebAction) --> void"
    << "QQuickWebEngineView.Unselect --> WebAction"
    << "QQuickWebEngineView.SavePage --> WebAction"
    << "QQuickWebEngineView.A4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Letter --> PrintedPageSizeId"
    << "QQuickWebEngineView.Legal --> PrintedPageSizeId"
    << "QQuickWebEngineView.Executive --> PrintedPageSizeId"
    << "QQuickWebEngineView.A0 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A1 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A7 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A8 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B0 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B1 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B7 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B8 --> PrintedPageSizeId"
    << "QQuickWebEngineView.B9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.C5E --> PrintedPageSizeId"
    << "QQuickWebEngineView.Comm10E --> PrintedPageSizeId"
    << "QQuickWebEngineView.DLE --> PrintedPageSizeId"
    << "QQuickWebEngineView.Folio --> PrintedPageSizeId"
    << "QQuickWebEngineView.Ledger --> PrintedPageSizeId"
    << "QQuickWebEngineView.Tabloid --> PrintedPageSizeId"
    << "QQuickWebEngineView.Custom --> PrintedPageSizeId"
    << "QQuickWebEngineView.A10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.A3Extra --> PrintedPageSizeId"
    << "QQuickWebEngineView.A4Extra --> PrintedPageSizeId"
    << "QQuickWebEngineView.A4Plus --> PrintedPageSizeId"
    << "QQuickWebEngineView.A4Small --> PrintedPageSizeId"
    << "QQuickWebEngineView.A5Extra --> PrintedPageSizeId"
    << "QQuickWebEngineView.B5Extra --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB0 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB1 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB7 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB8 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.JisB10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.AnsiC --> PrintedPageSizeId"
    << "QQuickWebEngineView.AnsiD --> PrintedPageSizeId"
    << "QQuickWebEngineView.AnsiE --> PrintedPageSizeId"
    << "QQuickWebEngineView.LegalExtra --> PrintedPageSizeId"
    << "QQuickWebEngineView.LetterExtra --> PrintedPageSizeId"
    << "QQuickWebEngineView.LetterPlus --> PrintedPageSizeId"
    << "QQuickWebEngineView.LetterSmall --> PrintedPageSizeId"
    << "QQuickWebEngineView.TabloidExtra --> PrintedPageSizeId"
    << "QQuickWebEngineView.ArchA --> PrintedPageSizeId"
    << "QQuickWebEngineView.ArchB --> PrintedPageSizeId"
    << "QQuickWebEngineView.ArchC --> PrintedPageSizeId"
    << "QQuickWebEngineView.ArchD --> PrintedPageSizeId"
    << "QQuickWebEngineView.ArchE --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial7x9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial8x10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial9x11 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial9x12 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial10x11 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial10x13 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial10x14 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial12x11 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Imperial15x11 --> PrintedPageSizeId"
    << "QQuickWebEngineView.ExecutiveStandard --> PrintedPageSizeId"
    << "QQuickWebEngineView.Note --> PrintedPageSizeId"
    << "QQuickWebEngineView.Quarto --> PrintedPageSizeId"
    << "QQuickWebEngineView.Statement --> PrintedPageSizeId"
    << "QQuickWebEngineView.SuperA --> PrintedPageSizeId"
    << "QQuickWebEngineView.SuperB --> PrintedPageSizeId"
    << "QQuickWebEngineView.Postcard --> PrintedPageSizeId"
    << "QQuickWebEngineView.DoublePostcard --> PrintedPageSizeId"
    << "QQuickWebEngineView.Prc16K --> PrintedPageSizeId"
    << "QQuickWebEngineView.Prc32K --> PrintedPageSizeId"
    << "QQuickWebEngineView.Prc32KBig --> PrintedPageSizeId"
    << "QQuickWebEngineView.FanFoldUS --> PrintedPageSizeId"
    << "QQuickWebEngineView.FanFoldGerman --> PrintedPageSizeId"
    << "QQuickWebEngineView.FanFoldGermanLegal --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeB4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeB5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeB6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC0 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC1 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC65 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC7 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Envelope9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Envelope11 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Envelope12 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Envelope14 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeMonarch --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePersonal --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeChou3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeChou4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeInvite --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeItalian --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeKaku2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeKaku3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc1 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc2 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc3 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc6 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc7 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc8 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc9 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopePrc10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeYou4 --> PrintedPageSizeId"
    << "QQuickWebEngineView.LastPageSize --> PrintedPageSizeId"
    << "QQuickWebEngineView.NPageSize --> PrintedPageSizeId"
    << "QQuickWebEngineView.NPaperSize --> PrintedPageSizeId"
    << "QQuickWebEngineView.AnsiA --> PrintedPageSizeId"
    << "QQuickWebEngineView.AnsiB --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeC5 --> PrintedPageSizeId"
    << "QQuickWebEngineView.EnvelopeDL --> PrintedPageSizeId"
    << "QQuickWebEngineView.Envelope10 --> PrintedPageSizeId"
    << "QQuickWebEngineView.Portrait --> PrintedPageOrientation"
    << "QQuickWebEngineView.Landscape --> PrintedPageOrientation"
    << "QQuickWebEngineView.contentsSize --> QSizeF"
    << "QQuickWebEngineView.scrollPosition --> QPointF"
    << "QQuickWebEngineView.audioMuted --> bool"
    << "QQuickWebEngineView.recentlyAudible --> bool"
    << "QQuickWebEngineView.webChannelWorld --> uint"
    << "QQuickWebEngineView.contentsSizeChanged(QSizeF) --> void"
    << "QQuickWebEngineView.scrollPositionChanged(QPointF) --> void"
    << "QQuickWebEngineView.audioMutedChanged(bool) --> void"
    << "QQuickWebEngineView.recentlyAudibleChanged(bool) --> void"
    << "QQuickWebEngineView.webChannelWorldChanged(uint) --> void"
    << "QQuickWebEngineView.runJavaScript(QString,uint,QJSValue) --> void"
    << "QQuickWebEngineView.runJavaScript(QString,uint) --> void"
    << "QQuickWebEngineView.printToPdf(QString,PrintedPageSizeId,PrintedPageOrientation) --> void"
    << "QQuickWebEngineView.printToPdf(QString,PrintedPageSizeId) --> void"
    << "QQuickWebEngineView.printToPdf(QString) --> void"
    << "QQuickWebEngineView.printToPdf(QJSValue,PrintedPageSizeId,PrintedPageOrientation) --> void"
    << "QQuickWebEngineView.printToPdf(QJSValue,PrintedPageSizeId) --> void"
    << "QQuickWebEngineView.printToPdf(QJSValue) --> void"
    << "QQuickWebEngineView.canViewSource --> bool"
    << "QQuickWebEngineView.replaceMisspelledWord(QString) --> void"
    << "QQuickWebEngineView.viewSource() --> void"
    << "QQuickWebEngineCertificateError.SslPinnedKeyNotInCertificateChain --> Error"
    << "QQuickWebEngineCertificateError.CertificateCommonNameInvalid --> Error"
    << "QQuickWebEngineCertificateError.CertificateDateInvalid --> Error"
    << "QQuickWebEngineCertificateError.CertificateAuthorityInvalid --> Error"
    << "QQuickWebEngineCertificateError.CertificateContainsErrors --> Error"
    << "QQuickWebEngineCertificateError.CertificateNoRevocationMechanism --> Error"
    << "QQuickWebEngineCertificateError.CertificateUnableToCheckRevocation --> Error"
    << "QQuickWebEngineCertificateError.CertificateRevoked --> Error"
    << "QQuickWebEngineCertificateError.CertificateInvalid --> Error"
    << "QQuickWebEngineCertificateError.CertificateWeakSignatureAlgorithm --> Error"
    << "QQuickWebEngineCertificateError.CertificateNonUniqueName --> Error"
    << "QQuickWebEngineCertificateError.CertificateWeakKey --> Error"
    << "QQuickWebEngineCertificateError.CertificateNameConstraintViolation --> Error"
    << "QQuickWebEngineCertificateError.CertificateValidityTooLong --> Error"
    << "QQuickWebEngineCertificateError.url --> QUrl"
    << "QQuickWebEngineCertificateError.error --> Error"
    << "QQuickWebEngineCertificateError.description --> QString"
    << "QQuickWebEngineCertificateError.overridable --> bool"
    << "QQuickWebEngineCertificateError.defer() --> void"
    << "QQuickWebEngineCertificateError.ignoreCertificateError() --> void"
    << "QQuickWebEngineCertificateError.rejectCertificate() --> void"
    << "QQuickWebEngineDownloadItem.DownloadRequested --> DownloadState"
    << "QQuickWebEngineDownloadItem.DownloadInProgress --> DownloadState"
    << "QQuickWebEngineDownloadItem.DownloadCompleted --> DownloadState"
    << "QQuickWebEngineDownloadItem.DownloadCancelled --> DownloadState"
    << "QQuickWebEngineDownloadItem.DownloadInterrupted --> DownloadState"
    << "QQuickWebEngineDownloadItem.id --> uint"
    << "QQuickWebEngineDownloadItem.state --> DownloadState"
    << "QQuickWebEngineDownloadItem.totalBytes --> qlonglong"
    << "QQuickWebEngineDownloadItem.receivedBytes --> qlonglong"
    << "QQuickWebEngineDownloadItem.mimeType --> QString"
    << "QQuickWebEngineDownloadItem.path --> QString"
    << "QQuickWebEngineDownloadItem.stateChanged() --> void"
    << "QQuickWebEngineDownloadItem.receivedBytesChanged() --> void"
    << "QQuickWebEngineDownloadItem.totalBytesChanged() --> void"
    << "QQuickWebEngineDownloadItem.mimeTypeChanged() --> void"
    << "QQuickWebEngineDownloadItem.pathChanged() --> void"
    << "QQuickWebEngineDownloadItem.accept() --> void"
    << "QQuickWebEngineDownloadItem.cancel() --> void"
    << "QQuickWebEngineDownloadItem.UnknownSaveFormat --> SavePageFormat"
    << "QQuickWebEngineDownloadItem.SingleHtmlSaveFormat --> SavePageFormat"
    << "QQuickWebEngineDownloadItem.CompleteHtmlSaveFormat --> SavePageFormat"
    << "QQuickWebEngineDownloadItem.MimeHtmlSaveFormat --> SavePageFormat"
    << "QQuickWebEngineDownloadItem.savePageFormat --> SavePageFormat"
    << "QQuickWebEngineDownloadItem.savePageFormatChanged() --> void"
    << "QQuickWebEngineDownloadItem.Attachment --> DownloadType"
    << "QQuickWebEngineDownloadItem.DownloadAttribute --> DownloadType"
    << "QQuickWebEngineDownloadItem.UserRequested --> DownloadType"
    << "QQuickWebEngineDownloadItem.SavePage --> DownloadType"
    << "QQuickWebEngineDownloadItem.type --> DownloadType"
    << "QQuickWebEngineDownloadItem.typeChanged() --> void"
    << "QQuickWebEngineHistory.items --> QQuickWebEngineHistoryListModel*"
    << "QQuickWebEngineHistory.backItems --> QQuickWebEngineHistoryListModel*"
    << "QQuickWebEngineHistory.forwardItems --> QQuickWebEngineHistoryListModel*"
    << "QQuickWebEngineLoadRequest.url --> QUrl"
    << "QQuickWebEngineLoadRequest.status --> QQuickWebEngineView::LoadStatus"
    << "QQuickWebEngineLoadRequest.errorString --> QString"
    << "QQuickWebEngineLoadRequest.errorDomain --> QQuickWebEngineView::ErrorDomain"
    << "QQuickWebEngineLoadRequest.errorCode --> int"
    << "QQuickWebEngineNavigationRequest.url --> QUrl"
    << "QQuickWebEngineNavigationRequest.isMainFrame --> bool"
    << "QQuickWebEngineNavigationRequest.action --> QQuickWebEngineView::NavigationRequestAction"
    << "QQuickWebEngineNavigationRequest.navigationType --> QQuickWebEngineView::NavigationType"
    << "QQuickWebEngineNavigationRequest.actionChanged() --> void"
    << "QQuickWebEngineNewViewRequest.destination --> QQuickWebEngineView::NewViewDestination"
    << "QQuickWebEngineNewViewRequest.userInitiated --> bool"
    << "QQuickWebEngineNewViewRequest.openIn(QQuickWebEngineView*) --> void"
    << "QQuickWebEngineProfile.MemoryHttpCache --> HttpCacheType"
    << "QQuickWebEngineProfile.DiskHttpCache --> HttpCacheType"
    << "QQuickWebEngineProfile.NoPersistentCookies --> PersistentCookiesPolicy"
    << "QQuickWebEngineProfile.AllowPersistentCookies --> PersistentCookiesPolicy"
    << "QQuickWebEngineProfile.ForcePersistentCookies --> PersistentCookiesPolicy"
    << "QQuickWebEngineProfile.storageName --> QString"
    << "QQuickWebEngineProfile.offTheRecord --> bool"
    << "QQuickWebEngineProfile.persistentStoragePath --> QString"
    << "QQuickWebEngineProfile.cachePath --> QString"
    << "QQuickWebEngineProfile.httpUserAgent --> QString"
    << "QQuickWebEngineProfile.httpCacheType --> HttpCacheType"
    << "QQuickWebEngineProfile.httpAcceptLanguage --> QString"
    << "QQuickWebEngineProfile.persistentCookiesPolicy --> PersistentCookiesPolicy"
    << "QQuickWebEngineProfile.httpCacheMaximumSize --> int"
    << "QQuickWebEngineProfile.storageNameChanged() --> void"
    << "QQuickWebEngineProfile.offTheRecordChanged() --> void"
    << "QQuickWebEngineProfile.persistentStoragePathChanged() --> void"
    << "QQuickWebEngineProfile.cachePathChanged() --> void"
    << "QQuickWebEngineProfile.httpUserAgentChanged() --> void"
    << "QQuickWebEngineProfile.httpCacheTypeChanged() --> void"
    << "QQuickWebEngineProfile.persistentCookiesPolicyChanged() --> void"
    << "QQuickWebEngineProfile.httpCacheMaximumSizeChanged() --> void"
    << "QQuickWebEngineProfile.httpAcceptLanguageChanged() --> void"
    << "QQuickWebEngineProfile.downloadRequested(QQuickWebEngineDownloadItem*) --> void"
    << "QQuickWebEngineProfile.downloadFinished(QQuickWebEngineDownloadItem*) --> void"
    << "QQuickWebEngineProfile.NoCache --> HttpCacheType"
    << "QQuickWebEngineProfile.spellCheckLanguage --> QString"
    << "QQuickWebEngineProfile.spellCheckEnabled --> bool"
    << "QQuickWebEngineProfile.spellCheckLanguageChanged() --> void"
    << "QQuickWebEngineProfile.spellCheckEnabledChanged() --> void"
    << "QQuickWebEngineProfile.clearHttpCache() --> void"
    << "QQuickWebEngineScript.Deferred --> InjectionPoint"
    << "QQuickWebEngineScript.DocumentReady --> InjectionPoint"
    << "QQuickWebEngineScript.DocumentCreation --> InjectionPoint"
    << "QQuickWebEngineScript.MainWorld --> ScriptWorldId"
    << "QQuickWebEngineScript.ApplicationWorld --> ScriptWorldId"
    << "QQuickWebEngineScript.UserWorld --> ScriptWorldId"
    << "QQuickWebEngineScript.name --> QString"
    << "QQuickWebEngineScript.sourceUrl --> QUrl"
    << "QQuickWebEngineScript.sourceCode --> QString"
    << "QQuickWebEngineScript.injectionPoint --> InjectionPoint"
    << "QQuickWebEngineScript.worldId --> ScriptWorldId"
    << "QQuickWebEngineScript.runOnSubframes --> bool"
    << "QQuickWebEngineScript.nameChanged(QString) --> void"
    << "QQuickWebEngineScript.sourceUrlChanged(QUrl) --> void"
    << "QQuickWebEngineScript.sourceCodeChanged(QString) --> void"
    << "QQuickWebEngineScript.injectionPointChanged(InjectionPoint) --> void"
    << "QQuickWebEngineScript.worldIdChanged(ScriptWorldId) --> void"
    << "QQuickWebEngineScript.runOnSubframesChanged(bool) --> void"
    << "QQuickWebEngineScript.setName(QString) --> void"
    << "QQuickWebEngineScript.setSourceUrl(QUrl) --> void"
    << "QQuickWebEngineScript.setSourceCode(QString) --> void"
    << "QQuickWebEngineScript.setInjectionPoint(InjectionPoint) --> void"
    << "QQuickWebEngineScript.setWorldId(ScriptWorldId) --> void"
    << "QQuickWebEngineScript.setRunOnSubframes(bool) --> void"
    << "QQuickWebEngineScript.toString() --> QString"
    << "QQuickWebEngineSettings.autoLoadImages --> bool"
    << "QQuickWebEngineSettings.javascriptEnabled --> bool"
    << "QQuickWebEngineSettings.javascriptCanOpenWindows --> bool"
    << "QQuickWebEngineSettings.javascriptCanAccessClipboard --> bool"
    << "QQuickWebEngineSettings.linksIncludedInFocusChain --> bool"
    << "QQuickWebEngineSettings.localStorageEnabled --> bool"
    << "QQuickWebEngineSettings.localContentCanAccessRemoteUrls --> bool"
    << "QQuickWebEngineSettings.spatialNavigationEnabled --> bool"
    << "QQuickWebEngineSettings.localContentCanAccessFileUrls --> bool"
    << "QQuickWebEngineSettings.hyperlinkAuditingEnabled --> bool"
    << "QQuickWebEngineSettings.errorPageEnabled --> bool"
    << "QQuickWebEngineSettings.pluginsEnabled --> bool"
    << "QQuickWebEngineSettings.fullScreenSupportEnabled --> bool"
    << "QQuickWebEngineSettings.defaultTextEncoding --> QString"
    << "QQuickWebEngineSettings.autoLoadImagesChanged() --> void"
    << "QQuickWebEngineSettings.javascriptEnabledChanged() --> void"
    << "QQuickWebEngineSettings.javascriptCanOpenWindowsChanged() --> void"
    << "QQuickWebEngineSettings.javascriptCanAccessClipboardChanged() --> void"
    << "QQuickWebEngineSettings.linksIncludedInFocusChainChanged() --> void"
    << "QQuickWebEngineSettings.localStorageEnabledChanged() --> void"
    << "QQuickWebEngineSettings.localContentCanAccessRemoteUrlsChanged() --> void"
    << "QQuickWebEngineSettings.spatialNavigationEnabledChanged() --> void"
    << "QQuickWebEngineSettings.localContentCanAccessFileUrlsChanged() --> void"
    << "QQuickWebEngineSettings.hyperlinkAuditingEnabledChanged() --> void"
    << "QQuickWebEngineSettings.errorPageEnabledChanged() --> void"
    << "QQuickWebEngineSettings.pluginsEnabledChanged() --> void"
    << "QQuickWebEngineSettings.fullScreenSupportEnabledChanged() --> void"
    << "QQuickWebEngineSettings.defaultTextEncodingChanged() --> void"
    << "QQuickWebEngineSettings.screenCaptureEnabled --> bool"
    << "QQuickWebEngineSettings.webGLEnabled --> bool"
    << "QQuickWebEngineSettings.accelerated2dCanvasEnabled --> bool"
    << "QQuickWebEngineSettings.autoLoadIconsForPage --> bool"
    << "QQuickWebEngineSettings.touchIconsEnabled --> bool"
    << "QQuickWebEngineSettings.screenCaptureEnabledChanged() --> void"
    << "QQuickWebEngineSettings.webGLEnabledChanged() --> void"
    << "QQuickWebEngineSettings.accelerated2dCanvasEnabledChanged() --> void"
    << "QQuickWebEngineSettings.autoLoadIconsForPageChanged() --> void"
    << "QQuickWebEngineSettings.touchIconsEnabledChanged() --> void"
    << "QQuickWebEngineSettings.focusOnNavigationEnabled --> bool"
    << "QQuickWebEngineSettings.focusOnNavigationEnabledChanged() --> void"
    << "QQuickWebEngineFullScreenRequest.origin --> QUrl"
    << "QQuickWebEngineFullScreenRequest.toggleOn --> bool"
    << "QQuickWebEngineFullScreenRequest.accept() --> void"
    << "QQuickWebEngineFullScreenRequest.reject() --> void"
    << "QQuickWebEngineSingleton.settings --> QQuickWebEngineSettings*"
    << "QQuickWebEngineSingleton.defaultProfile --> QQuickWebEngineProfile*"
    ;

static bool isCheckedEnum(const QByteArray &typeName)
{
    QList<QByteArray> tokens = typeName.split(':');
    if (tokens.size() == 3) {
        QByteArray &enumClass = tokens[0];
        QByteArray &enumName = tokens[2];
        foreach (const QMetaObject *mo, typesToCheck) {
            if (mo->className() != enumClass)
                continue;
            for (int i = mo->enumeratorOffset(); i < mo->enumeratorCount(); ++i)
                if (mo->enumerator(i).name() == enumName)
                    return true;
        }
    } else if (tokens.size() == 1) {
        QByteArray &enumName = tokens[0];
        foreach (const char *knownEnumName, knownEnumNames) {
            if (enumName == knownEnumName)
                return true;
        }
    }
    return false;
}

static bool isCheckedClass(const QByteArray &typeName)
{
    foreach (const QMetaObject *mo, typesToCheck) {
        QByteArray moTypeName(mo->className());
        if (moTypeName == typeName || moTypeName + "*" == typeName)
            return true;
    }
    return false;
}

static void checkKnownType(const QByteArray &typeName)
{
    if ((!hardcodedTypes.contains(typeName) && !QMetaType::type(typeName)) || QMetaType::type(typeName) >= QMetaType::User) {
        bool knownEnum = isCheckedEnum(typeName);
        bool knownClass = isCheckedClass(typeName);
        QVERIFY2(knownEnum || knownClass, qPrintable(QString("The API uses an unknown type [%1], you might have to add it to the typesToCheck list.").arg(typeName.constData())));
    }
}

static void gatherAPI(const QString &prefix, const QMetaEnum &metaEnum, QStringList *output)
{
    for (int i = 0; i < metaEnum.keyCount(); ++i)
        *output << QString::fromLatin1("%1%2 --> %3").arg(prefix).arg(metaEnum.key(i)).arg(metaEnum.name());
}

static void gatherAPI(const QString &prefix, const QMetaProperty &property, QStringList *output)
{
    *output << QString::fromLatin1("%1%2 --> %3").arg(prefix).arg(property.name()).arg(property.typeName());
    checkKnownType(property.typeName());
}

static void gatherAPI(const QString &prefix, const QMetaMethod &method, QStringList *output)
{
    if (method.access() != QMetaMethod::Private) {
        const char *methodTypeName = !!strlen(method.typeName()) ? method.typeName() : "void";
        *output << QString::fromLatin1("%1%2 --> %3").arg(prefix).arg(QString::fromLatin1(method.methodSignature())).arg(QString::fromLatin1(methodTypeName));

        checkKnownType(methodTypeName);
        foreach (QByteArray paramType, method.parameterTypes())
            checkKnownType(paramType);
    }
}

static void gatherAPI(const QString &prefix, const QMetaObject *meta, QStringList *output)
{
    // *Offset points us only at the leaf class members, we don't have inheritance in our API yet anyway.
    for (int i = meta->enumeratorOffset(); i < meta->enumeratorCount(); ++i) {
        knownEnumNames << meta->enumerator(i).name();
        gatherAPI(prefix, meta->enumerator(i), output);
    }
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
        gatherAPI(prefix, meta->property(i), output);
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i)
        gatherAPI(prefix, meta->method(i), output);
}

void tst_publicapi::publicAPI()
{
    QStringList actualAPI;
    foreach (const QMetaObject *meta, typesToCheck)
        gatherAPI(QString::fromLatin1(meta->className()) + ".", meta, &actualAPI);

    // Uncomment to print the actual API.
    // foreach (QString actual, actualAPI)
    //     printf("    << \"%s\"\n", qPrintable(actual));

    // Make sure that nothing slips in the public API unintentionally.
    foreach (QString actual, actualAPI) {
        if (!expectedAPI.contains(actual))
            QEXPECT_FAIL("", qPrintable("Expected list is not up-to-date: " + actual), Continue);
        QVERIFY2(expectedAPI.contains(actual), qPrintable(actual));
    }
    // Make sure that the expected list is up-to-date with intentionally added APIs.
    foreach (QString expected, expectedAPI) {
        if (!actualAPI.contains(expected))
            QEXPECT_FAIL("", qPrintable("Not implemented: " + expected), Continue);
        QVERIFY2(actualAPI.contains(expected), qPrintable(expected));
    }
}

QTEST_MAIN(tst_publicapi)

#include "tst_publicapi.moc"

