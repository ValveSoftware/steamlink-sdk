/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebView module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickviewcontroller_p.h"
#include "qwebview_p.h"

#include <QtGui/QWindow>
#include <QtQuick/QQuickWindow>
#include <QtCore/QDebug>

#include <QtQuick/qquickrendercontrol.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes changeMask = QQuickItemPrivate::Geometry
                                                         | QQuickItemPrivate::Children
                                                         | QQuickItemPrivate::Parent;

class QQuickViewChangeListener : public QQuickItemChangeListener
{
public:
    explicit QQuickViewChangeListener(QQuickViewController *item);
    ~QQuickViewChangeListener();

    inline void itemGeometryChanged(QQuickItem *,
                                    QQuickGeometryChange,
                                    const QRectF &) Q_DECL_OVERRIDE;
    void itemChildRemoved(QQuickItem *item, QQuickItem *child) Q_DECL_OVERRIDE;
    void itemParentChanged(QQuickItem *item, QQuickItem *parent) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(QQuickViewChangeListener)
    QQuickViewController *m_item;
    void addAncestorListeners(QQuickItem *item, QQuickItemPrivate::ChangeTypes changeType);
    void removeAncestorListeners(QQuickItem *item, QQuickItemPrivate::ChangeTypes changeType);
    bool isAncestor(QQuickItem *item);
};

QQuickViewChangeListener::QQuickViewChangeListener(QQuickViewController *item)
    : m_item(item)
{
    // Only listen for parent changes on the view controller item.
    QQuickItemPrivate::get(item)->addItemChangeListener(this, QQuickItemPrivate::Parent);
    // Listen to all changes, that are relevant, on all ancestors.
    addAncestorListeners(item->parentItem(), changeMask);
}

QQuickViewChangeListener::~QQuickViewChangeListener()
{
    if (m_item == 0)
        return;

    QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, QQuickItemPrivate::Parent);
    removeAncestorListeners(m_item->parentItem(), changeMask);
}

void QQuickViewChangeListener::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    m_item->polish();
}

void QQuickViewChangeListener::itemChildRemoved(QQuickItem *item, QQuickItem *child)
{
    Q_UNUSED(item)
    Q_ASSERT(item != m_item);

    const bool remove = (child == m_item) || isAncestor(child);

    // if the child isn't the view item or its ancestor, then we don't care.
    if (!remove)
        return;

    // Remove any listener we attached to the child and its ancestors.
    removeAncestorListeners(item, changeMask);
}

void QQuickViewChangeListener::itemParentChanged(QQuickItem *item, QQuickItem *newParent)
{
    removeAncestorListeners(item->parentItem(), changeMask);
    // Adds this as a listener for newParent and its ancestors.
    addAncestorListeners(newParent, changeMask);
}

void QQuickViewChangeListener::addAncestorListeners(QQuickItem *item,
                                                    QQuickItemPrivate::ChangeTypes changeType)
{
    QQuickItem *p = item;
    while (p != 0) {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, changeType);
        p = p->parentItem();
    }
}

void QQuickViewChangeListener::removeAncestorListeners(QQuickItem *item,
                                                       QQuickItemPrivate::ChangeTypes changeType)
{
    QQuickItem *p = item;
    while (p != 0) {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, changeType);
        p = p->parentItem();
    }
}

bool QQuickViewChangeListener::isAncestor(QQuickItem *item)
{
    Q_ASSERT(m_item != 0);

    if (item == 0)
        return false;

    QQuickItem *p = m_item->parentItem();
    while (p != 0) {
        if (p == item)
            return true;
        p = p->parentItem();
    }

    return false;
}

///
/// \brief QQuickViewController::QQuickViewController
/// \param parent
///

QQuickViewController::QQuickViewController(QQuickItem *parent)
    : QQuickItem(parent)
    , m_view(0)
    , m_changeListener(new QQuickViewChangeListener(this))
{
    connect(this, &QQuickViewController::windowChanged, this, &QQuickViewController::onWindowChanged);
    connect(this, &QQuickViewController::visibleChanged, this, &QQuickViewController::onVisibleChanged);
}

QQuickViewController::~QQuickViewController()
{
}

void QQuickViewController::componentComplete()
{
   QQuickItem::componentComplete();
   m_view->init();
   m_view->setVisibility(QWindow::Windowed);
}

void QQuickViewController::updatePolish()
{
    if (m_view == 0)
        return;

    QSize itemSize = QSize(width(), height());
    if (!itemSize.isValid())
        return;

    QQuickWindow *w = window();
    if (w == 0)
        return;

    // Find this item's geometry in the scene.
    QRect itemGeometry = mapRectToScene(QRect(QPoint(0, 0), itemSize)).toRect();
    // Check if we should be clipped to our parent's shape
    // Note: This is crude but it should give an acceptable result on all platforms.
    QQuickItem *p = parentItem();
    const bool clip = p != 0 ? p->clip() : false;
    if (clip) {
        const QSize &parentSize = QSize(p->width(), p->height());
        const QRect &parentGeometry = p->mapRectToScene(QRect(QPoint(0, 0), parentSize)).toRect();
        itemGeometry &= parentGeometry;
        itemSize = itemGeometry.size();
    }

    // Find the top left position of this item, in global coordinates.
    const QPoint &tl = w->mapToGlobal(itemGeometry.topLeft());
    // Get the actual render window, in case we're rendering into a off-screen window.
    QWindow *rw = QQuickRenderControl::renderWindowFor(w);

    m_view->setGeometry(rw ? QRect(rw->mapFromGlobal(tl), itemSize) : itemGeometry);
    m_view->setVisible(isVisible());
}

void QQuickViewController::setView(QNativeViewController *view)
{
    Q_ASSERT(m_view == 0);
    m_view = view;
}

void QQuickViewController::scheduleUpdatePolish()
{
    polish();
}

void QQuickViewController::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.isValid())
        polish();
}

void QQuickViewController::onWindowChanged(QQuickWindow* window)
{
    QQuickWindow *oldParent = qobject_cast<QQuickWindow *>(m_view->parentView());
    if (oldParent != 0)
        oldParent->disconnect(this);

    if (window != 0) {
        connect(window, &QQuickWindow::widthChanged, this, &QQuickViewController::scheduleUpdatePolish);
        connect(window, &QQuickWindow::heightChanged, this, &QQuickViewController::scheduleUpdatePolish);
        connect(window, &QQuickWindow::xChanged, this, &QQuickViewController::scheduleUpdatePolish);
        connect(window, &QQuickWindow::yChanged, this, &QQuickViewController::scheduleUpdatePolish);
        connect(window, &QQuickWindow::sceneGraphInitialized, this, &QQuickViewController::scheduleUpdatePolish);
        connect(window, &QQuickWindow::sceneGraphInvalidated, this, &QQuickViewController::onSceneGraphInvalidated);
    }

    // Check if there's an actual window available.
    QWindow *rw = QQuickRenderControl::renderWindowFor(window);
    m_view->setParentView(rw ? rw : window);
}

void QQuickViewController::onVisibleChanged()
{
    m_view->setVisible(isVisible());
}

void QQuickViewController::onSceneGraphInvalidated()
{
    if (m_view == 0)
        return;

    m_view->setVisible(false);
}

QT_END_NAMESPACE
