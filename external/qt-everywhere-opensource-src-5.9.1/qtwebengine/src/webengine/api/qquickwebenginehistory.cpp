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

#include "qquickwebenginehistory_p.h"
#include "qquickwebenginehistory_p_p.h"

#include "qquickwebenginefaviconprovider_p_p.h"
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

int QQuickWebEngineHistoryListModelPrivate::count() const
{
    if (!adapter())
        return 0;
    return adapter()->navigationEntryCount();
}

int QQuickWebEngineHistoryListModelPrivate::index(int index) const
{
    return index;
}

int QQuickWebEngineHistoryListModelPrivate::offsetForIndex(int index) const
{
    if (!adapter())
        return index;
    return index - adapter()->currentNavigationEntryIndex();
}

QtWebEngineCore::WebContentsAdapter *QQuickWebEngineHistoryListModelPrivate::adapter() const
{
    return view->adapter.data();
}

QQuickWebEngineBackHistoryListModelPrivate::QQuickWebEngineBackHistoryListModelPrivate(QQuickWebEngineViewPrivate *view)
    : QQuickWebEngineHistoryListModelPrivate(view)
{
}

int QQuickWebEngineBackHistoryListModelPrivate::count() const
{
    if (!adapter())
        return 0;
    return adapter()->currentNavigationEntryIndex();
}

int QQuickWebEngineBackHistoryListModelPrivate::index(int i) const
{
    Q_ASSERT(i >= 0 && i < count());
    return count() - 1 - i;
}

int QQuickWebEngineBackHistoryListModelPrivate::offsetForIndex(int index) const
{
    return - index - 1;
}

QQuickWebEngineForwardHistoryListModelPrivate::QQuickWebEngineForwardHistoryListModelPrivate(QQuickWebEngineViewPrivate *view)
    : QQuickWebEngineHistoryListModelPrivate(view)
{
}

int QQuickWebEngineForwardHistoryListModelPrivate::count() const
{
    if (!adapter())
        return 0;
    return adapter()->navigationEntryCount() - adapter()->currentNavigationEntryIndex() - 1;
}

int QQuickWebEngineForwardHistoryListModelPrivate::index(int i) const
{
    if (!adapter())
        return i + 1;
    return adapter()->currentNavigationEntryIndex() + i + 1;
}

int QQuickWebEngineForwardHistoryListModelPrivate::offsetForIndex(int index) const
{
    return index + 1;
}

/*!
    \qmltype WebEngineHistoryListModel
    \instantiates QQuickWebEngineHistoryListModel
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1

    \brief A data model that represents the history of a web engine page.

    The WebEngineHistoryListModel type exposes the \e title, \e url, \e icon, and \e offset roles.
    The \e title, \e url and \e icon specify the title, URL, and favicon of the visited page.
    The \e offset specifies
    the position of the page in respect to the current page (0). A positive number indicates that
    the page was visited after the current page, whereas a negative number indicates that the page
    was visited before the current page.

    This type is uncreatable, but it can be accessed by using the
    \l{WebEngineView::navigationHistory}{WebEngineView.navigationHistory} property.

    \sa WebEngineHistory
*/

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
    roles[QQuickWebEngineHistory::OffsetRole] = "offset";
    roles[QQuickWebEngineHistory::IconUrlRole] = "icon";
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

    if (role < QQuickWebEngineHistory::UrlRole || role > QQuickWebEngineHistory::IconUrlRole)
        return QVariant();

    if (role == QQuickWebEngineHistory::UrlRole)
        return QUrl(d->adapter()->getNavigationEntryUrl(d->index(index.row())));

    if (role == QQuickWebEngineHistory::TitleRole)
        return QString(d->adapter()->getNavigationEntryTitle(d->index(index.row())));

    if (role == QQuickWebEngineHistory::OffsetRole)
        return d->offsetForIndex(index.row());

    if (role == QQuickWebEngineHistory::IconUrlRole) {
        QUrl iconUrl = QUrl(d->adapter()->getNavigationEntryIconUrl(d->index(index.row())));
        return QQuickWebEngineFaviconProvider::faviconProviderUrl(iconUrl);
    }

    return QVariant();
}

void QQuickWebEngineHistoryListModel::reset()
{
    beginResetModel();
    endResetModel();
}

QQuickWebEngineHistoryPrivate::QQuickWebEngineHistoryPrivate(QQuickWebEngineViewPrivate *view)
    : m_view(view)
{
}

QQuickWebEngineHistoryPrivate::~QQuickWebEngineHistoryPrivate()
{
}

/*!
    \qmltype WebEngineHistory
    \instantiates QQuickWebEngineHistory
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1

    \brief Provides data models that represent the history of a web engine page.

    The WebEngineHistory type can be accessed by using the
    \l{WebEngineView::navigationHistory}{WebEngineView.navigationHistory} property.

    The WebEngineHistory type provides the following WebEngineHistoryListModel data model objects:

    \list
        \li \c backItems, which contains the URLs of visited pages.
        \li \c forwardItems, which contains the URLs of the pages that were visited after visiting
            the current page.
        \li \c items, which contains the URLs of the back and forward items, as well as the URL of
            the current page.
    \endlist

    The easiest way to use these models is to use them in a ListView as illustrated by the
    following code snippet:

    \code
    ListView {
        id: historyItemsList
        anchors.fill: parent
        model: webEngineView.navigationHistory.items
        delegate:
            Text {
                color: "black"
                text: model.title + " - " + model.url + " (" + model.offset + ")"
            }
    }
    \endcode

    The ListView shows the content of the corresponding model. The delegate is responsible for the
    format of the list items. The appearance of each item of the list in the delegate can be defined
    separately (it is not web engine specific).

    The model roles \e title, \e url, and \e icon specify the title, URL, and favicon of the
    visited page. The \e offset
    role specifies the position of the page in respect to the current page (0). A positive number
    indicates that the page was visited after the current page, whereas a negative number indicates
    that the page was visited before the current page.

    The data models can also be used to create a menu, as illustrated by the following code
    snippet:

    \quotefromfile webengine/quicknanobrowser/BrowserWindow.qml
    \skipto ToolBar
    \printuntil onObjectRemoved
    \printuntil }
    \printuntil }
    \printuntil }

    For the complete example, see \l{WebEngine Quick Nano Browser}.

    \sa WebEngineHistoryListModel
*/

QQuickWebEngineHistory::QQuickWebEngineHistory(QQuickWebEngineViewPrivate *view)
    : d_ptr(new QQuickWebEngineHistoryPrivate(view))
{
}

QQuickWebEngineHistory::~QQuickWebEngineHistory()
{
}

/*!
    \qmlproperty WebEngineHistoryListModel WebEngineHistory::items
    \readonly

    URLs of back items, forward items, and the current item in the history.
*/
QQuickWebEngineHistoryListModel *QQuickWebEngineHistory::items() const
{
    Q_D(const QQuickWebEngineHistory);
    if (!d->m_navigationModel)
        d->m_navigationModel.reset(new QQuickWebEngineHistoryListModel(new QQuickWebEngineHistoryListModelPrivate(d->m_view)));
    return d->m_navigationModel.data();
}

/*!
    \qmlproperty WebEngineHistoryListModel WebEngineHistory::backItems
    \readonly

    URLs of visited pages.
*/
QQuickWebEngineHistoryListModel *QQuickWebEngineHistory::backItems() const
{
    Q_D(const QQuickWebEngineHistory);
    if (!d->m_backNavigationModel)
        d->m_backNavigationModel.reset(new QQuickWebEngineHistoryListModel(new QQuickWebEngineBackHistoryListModelPrivate(d->m_view)));
    return d->m_backNavigationModel.data();
}

/*!
    \qmlproperty WebEngineHistoryListModel WebEngineHistory::forwardItems
    \readonly

    URLs of the pages that were visited after visiting the current page.
*/
QQuickWebEngineHistoryListModel *QQuickWebEngineHistory::forwardItems() const
{
    Q_D(const QQuickWebEngineHistory);
    if (!d->m_forwardNavigationModel)
        d->m_forwardNavigationModel.reset(new QQuickWebEngineHistoryListModel(new QQuickWebEngineForwardHistoryListModelPrivate(d->m_view)));
    return d->m_forwardNavigationModel.data();
}

void QQuickWebEngineHistory::reset()
{
    Q_D(QQuickWebEngineHistory);
    if (d->m_navigationModel)
        d->m_navigationModel->reset();
    if (d->m_backNavigationModel)
        d->m_backNavigationModel->reset();
    if (d->m_forwardNavigationModel)
        d->m_forwardNavigationModel->reset();
}


QT_END_NAMESPACE
