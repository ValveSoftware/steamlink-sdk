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

#ifndef QQUICKWEBENGINELOADREQUEST_P_H
#define QQUICKWEBENGINELOADREQUEST_P_H

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

#include "qtwebengineglobal_p.h"
#include "qquickwebengineview_p.h"

QT_BEGIN_NAMESPACE

class QQuickWebEngineLoadRequestPrivate;

class Q_WEBENGINE_EXPORT QQuickWebEngineLoadRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineView::LoadStatus status READ status CONSTANT FINAL)
    Q_PROPERTY(QString errorString READ errorString CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineView::ErrorDomain errorDomain READ errorDomain CONSTANT FINAL)
    Q_PROPERTY(int errorCode READ errorCode CONSTANT FINAL)

public:
    QQuickWebEngineLoadRequest(const QUrl& url, QQuickWebEngineView::LoadStatus status, const QString& errorString = QString(), int errorCode = 0, QQuickWebEngineView::ErrorDomain errorDomain = QQuickWebEngineView::NoErrorDomain, QObject* parent = 0);
    ~QQuickWebEngineLoadRequest();
    QUrl url() const;
    QQuickWebEngineView::LoadStatus status() const;
    QString errorString() const;
    QQuickWebEngineView::ErrorDomain errorDomain() const;
    int errorCode() const;

private:
    Q_DECLARE_PRIVATE(QQuickWebEngineLoadRequest)
    QScopedPointer<QQuickWebEngineLoadRequestPrivate> d_ptr;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineLoadRequest)

#endif // QQUICKWEBENGINELOADREQUEST_P_H
