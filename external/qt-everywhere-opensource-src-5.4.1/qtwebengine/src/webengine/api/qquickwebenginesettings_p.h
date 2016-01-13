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

#ifndef QQUICKWEBENGINESETTINGS_P_H
#define QQUICKWEBENGINESETTINGS_P_H

#include <private/qtwebengineglobal_p.h>
#include <QObject>
#include <QScopedPointer>

QT_BEGIN_NAMESPACE

class QQuickWebEngineSettingsPrivate;

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
    Q_PROPERTY(QString defaultTextEncoding READ defaultTextEncoding WRITE setDefaultTextEncoding NOTIFY defaultTextEncodingChanged)

public:
    static QQuickWebEngineSettings *globalSettings();

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
    QString defaultTextEncoding() const;

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
    void setDefaultTextEncoding(QString encoding);

signals:
    void autoLoadImagesChanged(bool on);
    void javascriptEnabledChanged(bool on);
    void javascriptCanOpenWindowsChanged(bool on);
    void javascriptCanAccessClipboardChanged(bool on);
    void linksIncludedInFocusChainChanged(bool on);
    void localStorageEnabledChanged(bool on);
    void localContentCanAccessRemoteUrlsChanged(bool on);
    void spatialNavigationEnabledChanged(bool on);
    void localContentCanAccessFileUrlsChanged(bool on);
    void hyperlinkAuditingEnabledChanged(bool on);
    void errorPageEnabledChanged(bool on);
    void defaultTextEncodingChanged(QString encoding);

private:
    QQuickWebEngineSettings();
    Q_DISABLE_COPY(QQuickWebEngineSettings)
    Q_DECLARE_PRIVATE(QQuickWebEngineSettings)
    friend class QQuickWebEngineViewPrivate;

    QScopedPointer<QQuickWebEngineSettingsPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QQUICKWEBENGINESETTINGS_P_H
