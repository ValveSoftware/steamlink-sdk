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

#ifndef QQUICKWEBENGINEHISTORY_P_H
#define QQUICKWEBENGINEHISTORY_P_H

#include <qtwebengineglobal_p.h>
#include <QAbstractListModel>
#include <QtCore/qshareddata.h>
#include <QQuickItem>
#include <QUrl>
#include <QVariant>

QT_BEGIN_NAMESPACE

class WebEngineHistory;
class WebEngineHistoryItem;
class QQuickWebEngineHistory;
class QQuickWebEngineHistoryPrivate;
class QQuickWebEngineHistoryListModelPrivate;
class QQuickWebEngineLoadRequest;
class QQuickWebEngineViewPrivate;

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineHistoryListModel : public QAbstractListModel {
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

    Q_DECLARE_PRIVATE(QQuickWebEngineHistoryListModel);
    QScopedPointer<QQuickWebEngineHistoryListModelPrivate> d_ptr;

    friend class QQuickWebEngineHistory;
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineHistory : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QQuickWebEngineHistoryListModel *backItems READ backItems CONSTANT FINAL)
    Q_PROPERTY(QQuickWebEngineHistoryListModel *forwardItems READ forwardItems CONSTANT FINAL)

public:
    QQuickWebEngineHistory(QQuickWebEngineViewPrivate*);
    virtual ~QQuickWebEngineHistory();

    enum NavigationHistoryRoles {
        UrlRole = Qt::UserRole + 1,
        TitleRole = Qt::UserRole + 2
    };

    QQuickWebEngineHistoryListModel *backItems() const;
    QQuickWebEngineHistoryListModel *forwardItems() const;

    void reset();

private:
    QQuickWebEngineHistory();

    Q_DECLARE_PRIVATE(QQuickWebEngineHistory);
    QScopedPointer<QQuickWebEngineHistoryPrivate> d_ptr;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWebEngineHistory)

#endif // QQUICKWEBENGINEHISTORY_P_H
