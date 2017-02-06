/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquicktabbar_p.h"
#include "qquicktabbutton_p.h"
#include "qquickcontainer_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype TabBar
    \inherits Container
    \instantiates QQuickTabBar
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-navigation
    \ingroup qtquickcontrols2-containers
    \brief Allows the user to switch between different views or subtasks.

    TabBar provides a tab-based navigation model.

    \image qtquickcontrols2-tabbar-wireframe.png

    TabBar is populated with TabButton controls, and can be used together with
    any layout or container control that provides \c currentIndex -property,
    such as \l StackLayout or \l SwipeView

    \snippet qtquickcontrols2-tabbar.qml 1

    As shown above, TabBar is typically populated with a static set of tab buttons
    that are defined inline as children of the tab bar. It is also possible to
    \l {Container::addItem()}{add}, \l {Container::insertItem()}{insert},
    \l {Container::moveItem()}{move}, and \l {Container::removeItem()}{remove}
    items dynamically at run time. The items can be accessed using
    \l {Container::}{itemAt()} or \l {Container::}{contentChildren}.

    \section2 Resizing Tabs

    By default, TabBar resizes its buttons to fit the width of the control.
    The available space is distributed equally to each button. The default
    resizing behavior can be overridden by setting an explicit width for the
    buttons.

    The following example illustrates how to keep each tab button at their
    implicit size instead of being resized to fit the tabbar:

    \borderedimage qtquickcontrols2-tabbar-explicit.png

    \snippet qtquickcontrols2-tabbar-explicit.qml 1

    \section2 Flickable Tabs

    If the total width of the buttons exceeds the available width of the tab bar,
    it automatically becomes flickable.

    \image qtquickcontrols2-tabbar-flickable.png

    \snippet qtquickcontrols2-tabbar-flickable.qml 1

    \sa TabButton, {Customizing TabBar}, {Navigation Controls}, {Container Controls}
*/

class QQuickTabBarPrivate : public QQuickContainerPrivate
{
    Q_DECLARE_PUBLIC(QQuickTabBar)

public:
    QQuickTabBarPrivate();

    void updateCurrentItem();
    void updateCurrentIndex();
    void updateLayout();

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &diff) override;

    bool updatingLayout;
    QQuickTabBar::Position position;
};

QQuickTabBarPrivate::QQuickTabBarPrivate() : updatingLayout(false), position(QQuickTabBar::Header)
{
    changeTypes |= Geometry;
}

void QQuickTabBarPrivate::updateCurrentItem()
{
    QQuickTabButton *button = qobject_cast<QQuickTabButton *>(contentModel->get(currentIndex));
    if (button)
        button->setChecked(true);
}

void QQuickTabBarPrivate::updateCurrentIndex()
{
    Q_Q(QQuickTabBar);
    QQuickTabButton *button = qobject_cast<QQuickTabButton *>(q->sender());
    if (button && button->isChecked())
        q->setCurrentIndex(contentModel->indexOf(button, nullptr));
}

void QQuickTabBarPrivate::updateLayout()
{
    Q_Q(QQuickTabBar);
    const int count = contentModel->count();
    if (count > 0 && contentItem) {
        qreal reservedWidth = 0;
        QVector<QQuickItem *> resizableItems;
        resizableItems.reserve(count);

        for (int i = 0; i < count; ++i) {
            QQuickItem *item = q->itemAt(i);
            if (item) {
                QQuickItemPrivate *p = QQuickItemPrivate::get(item);
                if (!p->widthValid)
                    resizableItems += item;
                else
                    reservedWidth += item->width();
            }
        }

        if (!resizableItems.isEmpty()) {
            const qreal totalSpacing = qMax(0, count - 1) * spacing;
            const qreal itemWidth = (contentItem->width() - reservedWidth - totalSpacing) / resizableItems.count();

            updatingLayout = true;
            for (QQuickItem *item : qAsConst(resizableItems)) {
                item->setWidth(itemWidth);
                QQuickItemPrivate::get(item)->widthValid = false;
            }
            updatingLayout = false;
        }
    }
}

void QQuickTabBarPrivate::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    if (!updatingLayout)
        updateLayout();
}

QQuickTabBar::QQuickTabBar(QQuickItem *parent) :
    QQuickContainer(*(new QQuickTabBarPrivate), parent)
{
    Q_D(QQuickTabBar);
    setFlag(ItemIsFocusScope);
    QObjectPrivate::connect(this, &QQuickTabBar::currentIndexChanged, d, &QQuickTabBarPrivate::updateCurrentItem);
}

/*!
    \qmlproperty enumeration QtQuick.Controls::TabBar::position

    This property holds the position of the tab bar.

    \note If the tab bar is assigned as a header or footer of \l ApplicationWindow
    or \l Page, the appropriate position is set automatically.

    Possible values:
    \value TabBar.Header The tab bar is at the top, as a window or page header.
    \value TabBar.Footer The tab bar is at the bottom, as a window or page footer.

    The default value is style-specific.

    \sa ApplicationWindow::header, ApplicationWindow::footer, Page::header, Page::footer
*/
QQuickTabBar::Position QQuickTabBar::position() const
{
    Q_D(const QQuickTabBar);
    return d->position;
}

void QQuickTabBar::setPosition(Position position)
{
    Q_D(QQuickTabBar);
    if (d->position == position)
        return;

    d->position = position;
    emit positionChanged();
}

void QQuickTabBar::updatePolish()
{
    Q_D(QQuickTabBar);
    QQuickContainer::updatePolish();
    d->updateLayout();
}

void QQuickTabBar::componentComplete()
{
    Q_D(QQuickTabBar);
    QQuickContainer::componentComplete();
    d->updateCurrentItem();
    d->updateLayout();
}

void QQuickTabBar::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTabBar);
    QQuickContainer::geometryChanged(newGeometry, oldGeometry);
    d->updateLayout();
}

bool QQuickTabBar::isContent(QQuickItem *item) const
{
    return qobject_cast<QQuickTabButton *>(item);
}

void QQuickTabBar::itemAdded(int index, QQuickItem *item)
{
    Q_D(QQuickTabBar);
    Q_UNUSED(index);
    QQuickItemPrivate::get(item)->setCulled(true); // QTBUG-55129
    if (QQuickTabButton *button = qobject_cast<QQuickTabButton *>(item))
        QObjectPrivate::connect(button, &QQuickTabButton::checkedChanged, d, &QQuickTabBarPrivate::updateCurrentIndex);
    if (isComponentComplete())
        polish();
}

void QQuickTabBar::itemRemoved(int index, QQuickItem *item)
{
    Q_D(QQuickTabBar);
    Q_UNUSED(index);
    if (QQuickTabButton *button = qobject_cast<QQuickTabButton *>(item))
        QObjectPrivate::disconnect(button, &QQuickTabButton::checkedChanged, d, &QQuickTabBarPrivate::updateCurrentIndex);
    if (isComponentComplete())
        polish();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickTabBar::accessibleRole() const
{
    return QAccessible::PageTabList;
}
#endif

QT_END_NAMESPACE
