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

#ifndef QQUICKWEBENGINEHISTORY_P_H
#define QQUICKWEBENGINEHISTORY_P_H

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

#include <qtwebengineglobal.h>
#include <QAbstractListModel>
#include <QtCore/qshareddata.h>
#include <QQuickItem>
#include <QUrl>
#include <QVariant>

QT_BEGIN_NAMESPACE

class QQuickWebEngineHistory;
class QQuickWebEngineHistoryPrivate;
class QQuickWebEngineHistoryListModelPrivate;
class QQuickWebEngineLoadRequest;
class QQuickWebEngineViewPrivate;

class Q_WEBENGINE_EXPORT QQuickWebEngineHistoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    QQuickWebEngineHistoryListModel(QQuickWebEngineHistoryListModelPrivate*);
    virtual ~QQuickWebEngineHistoryListModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    void reset();

private:
    QQuickWebEngineHistoryListModel();

    Q_DECLARE_PRIVATE(QQuickWebEngineHistoryListModel)
    QScopedPointer<QQuickWebEngineHistoryListModelPrivate> d_ptr;

    friend class QQuickWebEngineHistory;
};

class Q_WEBENGINE_EXPORT QQuickWebEngineHistory : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QQuickWebEngineHistoryListModel *items READ items CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineHistoryListModel *backItems READ backItems CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineHistoryListModel *forwardItems READ forwardItems CONSTANT FINAL)

public:
    QQuickWebEngineHistory(QQuickWebEngineViewPrivate*);
    virtual ~QQuickWebEngineHistory();

    enum NavigationHistoryRoles {
        UrlRole = Qt::UserRole + 1,
        TitleRole = Qt::UserRole + 2,
        OffsetRole = Qt::UserRole + 3,
        IconUrlRole = Qt::UserRole + 4,
    };

    QQuickWebEngineHistoryListModel *items() const;
    QQuickWebEngineHistoryListModel *backItems() const;
    QQuickWebEngineHistoryListModel *forwardItems() const;

    void reset();

private:
    QQuickWebEngineHistory();

    Q_DECLARE_PRIVATE(QQuickWebEngineHistory)
    QScopedPointer<QQuickWebEngineHistoryPrivate> d_ptr;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineHistory)

#endif // QQUICKWEBENGINEHISTORY_P_H
