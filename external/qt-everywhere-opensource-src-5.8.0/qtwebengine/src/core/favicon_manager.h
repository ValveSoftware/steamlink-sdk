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

#ifndef FAVICON_MANAGER_H
#define FAVICON_MANAGER_H

#include "qtwebenginecoreglobal.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QUrl>
#include <QtGui/QIcon>

#include "web_engine_settings.h"

namespace QtWebEngineCore {

class FaviconManagerPrivate;

// Based on src/3rdparty/chromium/content/public/common/favicon_url.h
class QWEBENGINE_EXPORT FaviconInfo {
public:
    enum FaviconType {
        InvalidIcon,
        Favicon,
        TouchIcon,
        TouchPrecomposedIcon
    };

    FaviconInfo();
    FaviconInfo(const FaviconInfo &);
    FaviconInfo(const QUrl &, FaviconInfo::FaviconType);
    ~FaviconInfo();

    bool isValid() const;
    bool isDownloaded() const;

    QUrl url;
    FaviconType type;
    // Stores the largest size in case of multi-size icon
    QSize size;
    bool candidate;
    bool multiSize;
};


class QWEBENGINE_EXPORT FaviconManager : public QObject {
    Q_OBJECT
public:
    ~FaviconManager();

    QIcon getIcon(const QUrl &url = QUrl()) const;
    FaviconInfo getFaviconInfo(const QUrl &) const;
    QList<FaviconInfo> getFaviconInfoList(bool) const;

private:
    FaviconManager(FaviconManagerPrivate *);

    void update(const QList<FaviconInfo> &);
    void updateCandidates(const QList<FaviconInfo> &);
    void resetCandidates();
    bool hasCandidate() const;

    QUrl candidateIconUrl(bool touchIconsEnabled) const;
    void generateCandidateIcon(bool touchIconsEnabled);

    QMap<QUrl, FaviconInfo> m_faviconInfoMap;
    int m_candidateCount;
    QIcon m_candidateIcon;

    Q_DISABLE_COPY(FaviconManager)
    Q_DECLARE_PRIVATE(FaviconManager)
    QScopedPointer<FaviconManagerPrivate> d_ptr;

    friend class WebContentsDelegateQt;
};

} // namespace QtWebEngineCore

#endif // FAVICON_MANAGER_H
