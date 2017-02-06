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

#ifndef QQUICKWEBENGINESETTINGS_P_H
#define QQUICKWEBENGINESETTINGS_P_H

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
#include <QObject>
#include <QScopedPointer>

namespace QtWebEngineCore {
class WebEngineSettings;
}

QT_BEGIN_NAMESPACE

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autoLoadImages READ autoLoadImages WRITE setAutoLoadImages NOTIFY autoLoadImagesChanged)
    Q_PROPERTY(bool javascriptEnabled READ javascriptEnabled WRITE setJavascriptEnabled NOTIFY javascriptEnabledChanged)
    Q_PROPERTY(bool javascriptCanOpenWindows READ javascriptCanOpenWindows WRITE setJavascriptCanOpenWindows NOTIFY javascriptCanOpenWindowsChanged)
    Q_PROPERTY(bool javascriptCanAccessClipboard READ javascriptCanAccessClipboard WRITE setJavascriptCanAccessClipboard NOTIFY javascriptCanAccessClipboardChanged)
    Q_PROPERTY(bool linksIncludedInFocusChain READ linksIncludedInFocusChain WRITE setLinksIncludedInFocusChain NOTIFY linksIncludedInFocusChainChanged)
    Q_PROPERTY(bool localStorageEnabled READ localStorageEnabled WRITE setLocalStorageEnabled NOTIFY localStorageEnabledChanged)
    Q_PROPERTY(bool localContentCanAccessRemoteUrls READ localContentCanAccessRemoteUrls WRITE setLocalContentCanAccessRemoteUrls NOTIFY localContentCanAccessRemoteUrlsChanged)
    Q_PROPERTY(bool spatialNavigationEnabled READ spatialNavigationEnabled WRITE setSpatialNavigationEnabled NOTIFY spatialNavigationEnabledChanged)
    Q_PROPERTY(bool localContentCanAccessFileUrls READ localContentCanAccessFileUrls WRITE setLocalContentCanAccessFileUrls NOTIFY localContentCanAccessFileUrlsChanged)
    Q_PROPERTY(bool hyperlinkAuditingEnabled READ hyperlinkAuditingEnabled WRITE setHyperlinkAuditingEnabled NOTIFY hyperlinkAuditingEnabledChanged)
    Q_PROPERTY(bool errorPageEnabled READ errorPageEnabled WRITE setErrorPageEnabled NOTIFY errorPageEnabledChanged)
    Q_PROPERTY(bool pluginsEnabled READ pluginsEnabled WRITE setPluginsEnabled NOTIFY pluginsEnabledChanged)
    Q_PROPERTY(bool fullScreenSupportEnabled READ fullScreenSupportEnabled WRITE setFullScreenSupportEnabled NOTIFY fullScreenSupportEnabledChanged REVISION 1)
    Q_PROPERTY(QString defaultTextEncoding READ defaultTextEncoding WRITE setDefaultTextEncoding NOTIFY defaultTextEncodingChanged)
    Q_PROPERTY(bool screenCaptureEnabled READ screenCaptureEnabled WRITE setScreenCaptureEnabled NOTIFY screenCaptureEnabledChanged REVISION 2)
    Q_PROPERTY(bool webGLEnabled READ webGLEnabled WRITE setWebGLEnabled NOTIFY webGLEnabledChanged REVISION 2)
    Q_PROPERTY(bool accelerated2dCanvasEnabled READ accelerated2dCanvasEnabled WRITE setAccelerated2dCanvasEnabled NOTIFY accelerated2dCanvasEnabledChanged REVISION 2)
    Q_PROPERTY(bool autoLoadIconsForPage READ autoLoadIconsForPage WRITE setAutoLoadIconsForPage NOTIFY autoLoadIconsForPageChanged REVISION 2)
    Q_PROPERTY(bool touchIconsEnabled READ touchIconsEnabled WRITE setTouchIconsEnabled NOTIFY touchIconsEnabledChanged REVISION 2)
    Q_PROPERTY(bool focusOnNavigationEnabled READ focusOnNavigationEnabled WRITE setFocusOnNavigationEnabled NOTIFY focusOnNavigationEnabledChanged REVISION 3)
    Q_PROPERTY(bool printElementBackgrounds READ printElementBackgrounds WRITE setPrintElementBackgrounds NOTIFY printElementBackgroundsChanged REVISION 3)
    Q_PROPERTY(bool allowRunningInsecureContent READ allowRunningInsecureContent WRITE setAllowRunningInsecureContent NOTIFY allowRunningInsecureContentChanged REVISION 3)

public:
    ~QQuickWebEngineSettings();

    bool autoLoadImages() const;
    bool javascriptEnabled() const;
    bool javascriptCanOpenWindows() const;
    bool javascriptCanAccessClipboard() const;
    bool linksIncludedInFocusChain() const;
    bool localStorageEnabled() const;
    bool localContentCanAccessRemoteUrls() const;
    bool spatialNavigationEnabled() const;
    bool localContentCanAccessFileUrls() const;
    bool hyperlinkAuditingEnabled() const;
    bool errorPageEnabled() const;
    bool pluginsEnabled() const;
    bool fullScreenSupportEnabled() const;
    QString defaultTextEncoding() const;
    bool screenCaptureEnabled() const;
    bool webGLEnabled() const;
    bool accelerated2dCanvasEnabled() const;
    bool autoLoadIconsForPage() const;
    bool touchIconsEnabled() const;
    bool focusOnNavigationEnabled() const;
    bool printElementBackgrounds() const;
    bool allowRunningInsecureContent() const;

    void setAutoLoadImages(bool on);
    void setJavascriptEnabled(bool on);
    void setJavascriptCanOpenWindows(bool on);
    void setJavascriptCanAccessClipboard(bool on);
    void setLinksIncludedInFocusChain(bool on);
    void setLocalStorageEnabled(bool on);
    void setLocalContentCanAccessRemoteUrls(bool on);
    void setSpatialNavigationEnabled(bool on);
    void setLocalContentCanAccessFileUrls(bool on);
    void setHyperlinkAuditingEnabled(bool on);
    void setErrorPageEnabled(bool on);
    void setPluginsEnabled(bool on);
    void setFullScreenSupportEnabled(bool on);
    void setDefaultTextEncoding(QString encoding);
    void setScreenCaptureEnabled(bool on);
    void setWebGLEnabled(bool on);
    void setAccelerated2dCanvasEnabled(bool on);
    void setAutoLoadIconsForPage(bool on);
    void setTouchIconsEnabled(bool on);
    void setFocusOnNavigationEnabled(bool on);
    void setPrintElementBackgrounds(bool on);
    void setAllowRunningInsecureContent(bool on);

signals:
    void autoLoadImagesChanged();
    void javascriptEnabledChanged();
    void javascriptCanOpenWindowsChanged();
    void javascriptCanAccessClipboardChanged();
    void linksIncludedInFocusChainChanged();
    void localStorageEnabledChanged();
    void localContentCanAccessRemoteUrlsChanged();
    void spatialNavigationEnabledChanged();
    void localContentCanAccessFileUrlsChanged();
    void hyperlinkAuditingEnabledChanged();
    void errorPageEnabledChanged();
    void pluginsEnabledChanged();
    Q_REVISION(1) void fullScreenSupportEnabledChanged();
    void defaultTextEncodingChanged();
    Q_REVISION(2) void screenCaptureEnabledChanged();
    Q_REVISION(2) void webGLEnabledChanged();
    Q_REVISION(2) void accelerated2dCanvasEnabledChanged();
    Q_REVISION(2) void autoLoadIconsForPageChanged();
    Q_REVISION(2) void touchIconsEnabledChanged();
    Q_REVISION(3) void focusOnNavigationEnabledChanged();
    Q_REVISION(3) void printElementBackgroundsChanged();
    Q_REVISION(3) void allowRunningInsecureContentChanged();

private:
    explicit QQuickWebEngineSettings(QQuickWebEngineSettings *parentSettings = 0);
    Q_DISABLE_COPY(QQuickWebEngineSettings)
    friend class QQuickWebEngineProfilePrivate;
    friend class QQuickWebEngineViewPrivate;

    void setParentSettings(QQuickWebEngineSettings *parentSettings);

    QScopedPointer<QtWebEngineCore::WebEngineSettings> d_ptr;
};

QT_END_NAMESPACE

#endif // QQUICKWEBENGINESETTINGS_P_H
