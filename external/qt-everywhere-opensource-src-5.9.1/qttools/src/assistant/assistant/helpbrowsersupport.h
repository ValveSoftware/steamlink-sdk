/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef HELPBROWSERSUPPORT_H
#define HELPBROWSERSUPPORT_H

#include <QtCore/QtGlobal>

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QObject;
class QString;
class QByteArray;
class QUrl;

// Provide helper functions for feeding the QtHelp data stored in the help database
// into various browsers.
class HelpBrowserSupport
{
public:
    enum ResolveUrlResult {
        UrlRedirect,
        UrlLocalData,
        UrlResolveError
    };

    static QString msgError404();
    static QString msgPageNotFound();
    static QString msgAllDocumentationSets();
    static QString msgLoadError(const QUrl &url);
    static QString msgHtmlErrorPage(const QUrl &url);

    static ResolveUrlResult resolveUrl(const QUrl &url, QUrl *targetUrl,
                                       QByteArray *data);
    static QByteArray fileDataForLocalUrl(const QUrl &url);

    // Create an instance of QNetworkAccessManager for WebKit-type browsers.
    static QNetworkAccessManager *createNetworkAccessManager(QObject *parent = 0);
};

QT_END_NAMESPACE

#endif // HELPBROWSERSUPPORT_H
