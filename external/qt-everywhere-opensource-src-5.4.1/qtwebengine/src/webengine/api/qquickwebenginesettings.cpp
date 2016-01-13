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

#include "qquickwebenginesettings_p.h"
#include "qquickwebenginesettings_p_p.h"

#include <QtCore/QList>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QList<QQuickWebEngineSettingsPrivate*>, allSettings)

QQuickWebEngineSettingsPrivate::QQuickWebEngineSettingsPrivate()
    : coreSettings(new WebEngineSettings(this))
{
    allSettings->append(this);
}

void QQuickWebEngineSettingsPrivate::apply()
{
    coreSettings->scheduleApply();
    QQuickWebEngineSettingsPrivate *globals = QQuickWebEngineSettings::globalSettings()->d_func();
    Q_ASSERT((this == globals) != (allSettings->contains(this)));
    if (this == globals)
        Q_FOREACH (QQuickWebEngineSettingsPrivate *settings, *allSettings)
            settings->coreSettings->scheduleApply();
}

WebEngineSettings *QQuickWebEngineSettingsPrivate::fallbackSettings() const
{
    return QQuickWebEngineSettings::globalSettings()->d_func()->coreSettings.data();
}


QQuickWebEngineSettings *QQuickWebEngineSettings::globalSettings()
{
    static QQuickWebEngineSettings *globals = 0;
    if (!globals) {
        globals = new QQuickWebEngineSettings;
        allSettings->removeAll(globals->d_func());
        globals->d_func()->coreSettings->initDefaults();
    }
    return globals;
}

QQuickWebEngineSettings::~QQuickWebEngineSettings()
{
    allSettings->removeAll(this->d_func());
}

bool QQuickWebEngineSettings::autoLoadImages() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::AutoLoadImages);
}

bool QQuickWebEngineSettings::javascriptEnabled() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::JavascriptEnabled);
}

bool QQuickWebEngineSettings::javascriptCanOpenWindows() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::JavascriptCanOpenWindows);
}

bool QQuickWebEngineSettings::javascriptCanAccessClipboard() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::JavascriptCanAccessClipboard);
}

bool QQuickWebEngineSettings::linksIncludedInFocusChain() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::LinksIncludedInFocusChain);
}

bool QQuickWebEngineSettings::localStorageEnabled() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::LocalStorageEnabled);
}

bool QQuickWebEngineSettings::localContentCanAccessRemoteUrls() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls);
}

bool QQuickWebEngineSettings::spatialNavigationEnabled() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::SpatialNavigationEnabled);
}

bool QQuickWebEngineSettings::localContentCanAccessFileUrls() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::LocalContentCanAccessFileUrls);
}

bool QQuickWebEngineSettings::hyperlinkAuditingEnabled() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::HyperlinkAuditingEnabled);
}

bool QQuickWebEngineSettings::errorPageEnabled() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->testAttribute(WebEngineSettings::ErrorPageEnabled);
}

QString QQuickWebEngineSettings::defaultTextEncoding() const
{
    Q_D(const QQuickWebEngineSettings);
    return d->coreSettings->defaultTextEncoding();
}

void QQuickWebEngineSettings::setAutoLoadImages(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::AutoLoadImages);
    // Set unconditionally as it sets the override for the current settings while the current setting
    // could be from the fallback and is prone to changing later on.
    d->coreSettings->setAttribute(WebEngineSettings::AutoLoadImages, on);
    if (wasOn ^ on)
        Q_EMIT autoLoadImagesChanged(on);
}

void QQuickWebEngineSettings::setJavascriptEnabled(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::JavascriptEnabled);
    d->coreSettings->setAttribute(WebEngineSettings::JavascriptEnabled, on);
    if (wasOn ^ on)
        Q_EMIT javascriptEnabledChanged(on);
}

void QQuickWebEngineSettings::setJavascriptCanOpenWindows(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::JavascriptCanOpenWindows);
    d->coreSettings->setAttribute(WebEngineSettings::JavascriptCanOpenWindows, on);
    if (wasOn ^ on)
        Q_EMIT javascriptCanOpenWindowsChanged(on);
}

void QQuickWebEngineSettings::setJavascriptCanAccessClipboard(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::JavascriptCanAccessClipboard);
    d->coreSettings->setAttribute(WebEngineSettings::JavascriptCanAccessClipboard, on);
    if (wasOn ^ on)
        Q_EMIT javascriptCanAccessClipboardChanged(on);
}

void QQuickWebEngineSettings::setLinksIncludedInFocusChain(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::LinksIncludedInFocusChain);
    d->coreSettings->setAttribute(WebEngineSettings::LinksIncludedInFocusChain, on);
    if (wasOn ^ on)
        Q_EMIT linksIncludedInFocusChainChanged(on);
}

void QQuickWebEngineSettings::setLocalStorageEnabled(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::LocalStorageEnabled);
    d->coreSettings->setAttribute(WebEngineSettings::LocalStorageEnabled, on);
    if (wasOn ^ on)
        Q_EMIT localStorageEnabledChanged(on);
}

void QQuickWebEngineSettings::setLocalContentCanAccessRemoteUrls(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls);
    d->coreSettings->setAttribute(WebEngineSettings::LocalContentCanAccessRemoteUrls, on);
    if (wasOn ^ on)
        Q_EMIT localContentCanAccessRemoteUrlsChanged(on);
}


void QQuickWebEngineSettings::setSpatialNavigationEnabled(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::SpatialNavigationEnabled);
    d->coreSettings->setAttribute(WebEngineSettings::SpatialNavigationEnabled, on);
    if (wasOn ^ on)
        Q_EMIT spatialNavigationEnabledChanged(on);
}

void QQuickWebEngineSettings::setLocalContentCanAccessFileUrls(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::LocalContentCanAccessFileUrls);
    d->coreSettings->setAttribute(WebEngineSettings::LocalContentCanAccessFileUrls, on);
    if (wasOn ^ on)
        Q_EMIT localContentCanAccessFileUrlsChanged(on);
}

void QQuickWebEngineSettings::setHyperlinkAuditingEnabled(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::HyperlinkAuditingEnabled);
    d->coreSettings->setAttribute(WebEngineSettings::HyperlinkAuditingEnabled, on);
    if (wasOn ^ on)
        Q_EMIT hyperlinkAuditingEnabledChanged(on);
}

void QQuickWebEngineSettings::setErrorPageEnabled(bool on)
{
    Q_D(QQuickWebEngineSettings);
    bool wasOn = d->coreSettings->testAttribute(WebEngineSettings::ErrorPageEnabled);
    d->coreSettings->setAttribute(WebEngineSettings::ErrorPageEnabled, on);
    if (wasOn ^ on)
        Q_EMIT errorPageEnabledChanged(on);
}

void QQuickWebEngineSettings::setDefaultTextEncoding(QString encoding)
{
    Q_D(QQuickWebEngineSettings);
    const QString oldDefaultTextEncoding = d->coreSettings->defaultTextEncoding();
    d->coreSettings->setDefaultTextEncoding(encoding);
    if (oldDefaultTextEncoding.compare(encoding))
        Q_EMIT defaultTextEncodingChanged(encoding);
}

QQuickWebEngineSettings::QQuickWebEngineSettings()
    : d_ptr(new QQuickWebEngineSettingsPrivate)
{
}

QT_END_NAMESPACE
