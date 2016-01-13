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

#include "qquickwebenginehistory_p.h"
#include "qquickwebenginehistory_p_p.h"
#include "qquickwebengineloadrequest_p.h"
#include "qquickwebengineview_p_p.h"
#include "web_contents_adapter.h"

QT_BEGIN_NAMESPACE

QQuickWebEngineHistoryListModelPrivate::QQuickWebEngineHistoryListModelPrivate(QQuickWebEngineViewPrivate *view)
    : view(view)
{
}

QQuickWebEngineHistoryListModelPrivate::~QQuickWebEngineHistoryListModelPrivate()
{
}

WebContentsAdapter *QQuickWebEngineHistoryListModelPrivate::adapter() const
{
    return view->adapter.data();
}

QQuickWebEngineBackHistoryListModelPrivate::QQuickWebEngineBackHistoryListModelPrivate(QQuickWebEngineViewPrivate *view)
    : QQuickWebEngineHistoryListModelPrivate(view)
{
}

int QQuickWebEngineBackHistoryListModelPrivate::count() const
{
    return adapter()->currentNavigationEntryIndex();
}

int QQuickWebEngineBackHistoryListModelPrivate::index(int i) const
{
    Q_ASSERT(i >= 0 && i < count());
    return count() - 1 - i;
}

QQuickWebEngineForwardHistoryListModelPrivate::QQuickWebEngineForwardHistoryListModelPrivate(QQuickWebEngineViewPrivate *view)
    : QQuickWebEngineHistoryListModelPrivate(view)
{
}

int QQuickWebEngineForwardHistoryListModelPrivate::count() const
{
    return adapter()->navigationEntryCount() - adapter()->currentNavigationEntryIndex() - 1;
}

int QQuickWebEngineForwardHistoryListModelPrivate::index(int i) const
{
    return adapter()->currentNavigationEntryIndex() + i + 1;
}

QQuickWebEngineHistoryListModel::QQuickWebEngineHistoryListModel()
    : QAbstractListModel()
{
}

QQuickWebEngineHistoryListModel::QQuickWebEngineHistoryListModel(QQuickWebEngineHistoryListModelPrivate *d)
    : QAbstractListModel()
    , d_ptr(d)
{
}

QQuickWebEngineHistoryListModel::~QQuickWebEngineHistoryListModel()
{
}

QHash<int, QByteArray> QQuickWebEngineHistoryListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[QQuickWebEngineHistory::UrlRole] = "url";
    roles[QQuickWebEngineHistory::TitleRole] = "title";
    return roles;
}

int QQuickWebEngineHistoryListModel::rowCount(const QModelIndex &index) const
{
    Q_UNUSED(index);
    Q_D(const QQuickWebEngineHistoryListModel);
    return d->count();
}

QVariant QQuickWebEngineHistoryListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QQuickWebEngineHistoryListModel);

    if (!index.isValid())
        return QVariant();

    if (role < QQuickWebEngineHistory::UrlRole || role > QQuickWebEngineHistory::TitleRole)
        return QVariant();

    if (role == QQuickWebEngineHistory::UrlRole)
        return QUrl(d->adapter()->getNavigationEntryUrl(d->index(index.row())));

    if (role == QQuickWebEngineHistory::TitleRole)
        return QString(d->adapter()->getNavigationEntryTitle(d->index(index.row())));

    return QVariant();
}

void QQuickWebEngineHistoryListModel::reset()
{
    beginResetModel();
    endResetModel();
}

QQuickWebEngineHistoryPrivate::QQuickWebEngineHistoryPrivate(QQuickWebEngineViewPrivate *view)
    : m_backNavigationModel(new QQuickWebEngineHistoryListModel(new QQuickWebEngineBackHistoryListModelPrivate(view)))
    , m_forwardNavigationModel(new QQuickWebEngineHistoryListModel(new QQuickWebEngineForwardHistoryListModelPrivate(view)))
{
}

QQuickWebEngineHistoryPrivate::~QQuickWebEngineHistoryPrivate()
{
}

QQuickWebEngineHistory::QQuickWebEngineHistory(QQuickWebEngineViewPrivate *view)
    : d_ptr(new QQuickWebEngineHistoryPrivate(view))
{
}

QQuickWebEngineHistory::~QQuickWebEngineHistory()
{
}

QQuickWebEngineHistoryListModel *QQuickWebEngineHistory::backItems() const
{
    Q_D(const QQuickWebEngineHistory);
    return d->m_backNavigationModel.data();
}

QQuickWebEngineHistoryListModel *QQuickWebEngineHistory::forwardItems() const
{
    Q_D(const QQuickWebEngineHistory);
    return d->m_forwardNavigationModel.data();
}

void QQuickWebEngineHistory::reset()
{
    Q_D(QQuickWebEngineHistory);
    d->m_backNavigationModel->reset();
    d->m_forwardNavigationModel->reset();
}


QT_END_NAMESPACE
