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

#include "qquickpane_p.h"
#include "qquickpane_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Pane
    \inherits Control
    \instantiates QQuickPane
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-containers
    \brief Provides a background matching with the application style and theme.

    Pane provides a background color that matches with the application style
    and theme. Pane does not provide a layout of its own, but requires you to
    position its contents, for instance by creating a \l RowLayout or a
    \l ColumnLayout.

    Items declared as children of a Pane are automatically parented to the
    Pane's \l {Control::}{contentItem}. Items created dynamically need to be
    explicitly parented to the contentItem.

    \section1 Content Sizing

    If only a single item is used within a Pane, it will resize to fit the
    implicit size of its contained item. This makes it particularly suitable
    for use together with layouts.

    \image qtquickcontrols2-pane.png

    \snippet qtquickcontrols2-pane.qml 1

    Sometimes there might be two items within the pane:

    \code
    Pane {
        SwipeView {
            // ...
        }
        PageIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
        }
    }
    \endcode

    In this case, Pane cannot calculate a sensible implicit size. Since we're
    anchoring the \l PageIndicator over the \l SwipeView, we can simply set the
    content size to the view's implicit size:

    \code
    Pane {
        contentWidth: view.implicitWidth
        contentHeight: view.implicitHeight

        SwipeView {
            id: view
            // ...
        }
        PageIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
        }
     }
    \endcode

    \sa {Customizing Pane}, {Container Controls}
*/

QQuickPanePrivate::QQuickPanePrivate() : contentWidth(0), contentHeight(0)
{
}

QQuickItem *QQuickPanePrivate::getContentItem()
{
    Q_Q(QQuickPane);
    if (!contentItem)
        contentItem = new QQuickItem(q);
    return contentItem;
}

QQuickPane::QQuickPane(QQuickItem *parent) :
    QQuickControl(*(new QQuickPanePrivate), parent)
{
    setFlag(QQuickItem::ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);
}

QQuickPane::QQuickPane(QQuickPanePrivate &dd, QQuickItem *parent) :
    QQuickControl(dd, parent)
{
    setFlag(QQuickItem::ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);
}

/*!
    \qmlproperty real QtQuick.Controls::Pane::contentWidth

    This property holds the content width. It is used for calculating the total
    implicit width of the pane.

    For more information, see \l {Content Sizing}.

    \sa contentHeight
*/
qreal QQuickPane::contentWidth() const
{
    Q_D(const QQuickPane);
    return d->contentWidth;
}

void QQuickPane::setContentWidth(qreal width)
{
    Q_D(QQuickPane);
    if (qFuzzyCompare(d->contentWidth, width))
        return;

    d->contentWidth = width;
    emit contentWidthChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Pane::contentHeight

    This property holds the content height. It is used for calculating the total
    implicit height of the pane.

    For more information, see \l {Content Sizing}.

    \sa contentWidth
*/
qreal QQuickPane::contentHeight() const
{
    Q_D(const QQuickPane);
    return d->contentHeight;
}

void QQuickPane::setContentHeight(qreal height)
{
    Q_D(QQuickPane);
    if (qFuzzyCompare(d->contentHeight, height))
        return;

    d->contentHeight = height;
    emit contentHeightChanged();
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::Pane::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children
    of the pane.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML
    objects.

    \sa Item::data, contentChildren
*/
QQmlListProperty<QObject> QQuickPane::contentData()
{
    Q_D(QQuickPane);
    return QQmlListProperty<QObject>(d->getContentItem(), nullptr,
                                     QQuickItemPrivate::data_append,
                                     QQuickItemPrivate::data_count,
                                     QQuickItemPrivate::data_at,
                                     QQuickItemPrivate::data_clear);
}

/*!
    \qmlproperty list<Item> QtQuick.Controls::Pane::contentChildren

    This property holds the list of content children.

    The list contains all items that have been declared in QML as children
    of the pane.

    \note Unlike \c contentData, \c contentChildren does not include non-visual
    QML objects.

    \sa Item::children, contentData
*/
QQmlListProperty<QQuickItem> QQuickPane::contentChildren()
{
    Q_D(QQuickPane);
    return QQmlListProperty<QQuickItem>(d->getContentItem(), nullptr,
                                        QQuickItemPrivate::children_append,
                                        QQuickItemPrivate::children_count,
                                        QQuickItemPrivate::children_at,
                                        QQuickItemPrivate::children_clear);
}

void QQuickPane::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    QQuickControl::contentItemChange(newItem, oldItem);
    if (oldItem)
        disconnect(oldItem, &QQuickItem::childrenChanged, this, &QQuickPane::contentChildrenChanged);
    if (newItem)
        connect(newItem, &QQuickItem::childrenChanged, this, &QQuickPane::contentChildrenChanged);
    emit contentChildrenChanged();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPane::accessibleRole() const
{
    return QAccessible::Pane;
}
#endif

QT_END_NAMESPACE
