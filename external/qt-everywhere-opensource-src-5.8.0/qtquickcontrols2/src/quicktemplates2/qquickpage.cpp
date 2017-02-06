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

#include "qquickpage_p.h"
#include "qquickcontrol_p_p.h"
#include "qquickpagelayout_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Page
    \inherits Control
    \instantiates QQuickPage
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-containers
    \brief Styled page control with support for a header and footer.

    Page is a container control which makes it convenient to add
    a \l header and \l footer item to a page.

    \image qtquickcontrols2-page-wireframe.png

    The following example snippet illustrates how to use a page-specific
    toolbar header and an application-wide tabbar footer.

    \qml
    import QtQuick.Controls 2.1

    ApplicationWindow {
        visible: true

        StackView {
            anchors.fill: parent

            initialItem: Page {
                header: ToolBar {
                    // ...
                }
            }
        }

        footer: TabBar {
            // ...
        }
    }
    \endqml

    \sa ApplicationWindow, {Container Controls}
*/

class QQuickPagePrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickPage)

public:
    QQuickPagePrivate();

    QQuickItem *getContentItem() override;

    qreal contentWidth;
    qreal contentHeight;
    QString title;
    QScopedPointer<QQuickPageLayout> layout;
};

QQuickPagePrivate::QQuickPagePrivate()
    : contentWidth(0),
      contentHeight(0)
{
}

QQuickItem *QQuickPagePrivate::getContentItem()
{
    Q_Q(QQuickPage);
    if (!contentItem)
        contentItem = new QQuickItem(q);
    return contentItem;
}

QQuickPage::QQuickPage(QQuickItem *parent) :
    QQuickControl(*(new QQuickPagePrivate), parent)
{
    Q_D(QQuickPage);
    setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);
    d->layout.reset(new QQuickPageLayout(this));
}

/*!
    \qmlproperty string QtQuick.Controls::Page::title

    This property holds the page title.

    The title is often displayed at the top of a page to give
    the user context about the page they are viewing.

    \code
    ApplicationWindow {
        visible: true
        width: 400
        height: 400

        header: Label {
            text: view.currentItem.title
            horizontalAlignment: Text.AlignHCenter
        }

        SwipeView {
            id: view
            anchors.fill: parent

            Page {
                title: qsTr("Home")
            }
            Page {
                title: qsTr("Discover")
            }
            Page {
                title: qsTr("Activity")
            }
        }
    }
    \endcode
*/

QString QQuickPage::title() const
{
    return d_func()->title;
}

void QQuickPage::setTitle(const QString &title)
{
    Q_D(QQuickPage);
    if (d->title == title)
        return;

    d->title = title;
    setAccessibleName(title);
    emit titleChanged();
}

/*!
    \qmlproperty Item QtQuick.Controls::Page::header

    This property holds the page header item. The header item is positioned to
    the top, and resized to the width of the page. The default value is \c null.

    \note Assigning a ToolBar, TabBar, or DialogButtonBox as a page header
    automatically sets the respective \l ToolBar::position, \l TabBar::position,
    or \l DialogButtonBox::position property to \c Header.

    \sa footer, ApplicationWindow::header
*/
QQuickItem *QQuickPage::header() const
{
    Q_D(const QQuickPage);
    return d->layout->header();
}

void QQuickPage::setHeader(QQuickItem *header)
{
    Q_D(QQuickPage);
    if (!d->layout->setHeader(header))
        return;
    if (isComponentComplete())
        d->layout->update();
    emit headerChanged();
}

/*!
    \qmlproperty Item QtQuick.Controls::Page::footer

    This property holds the page footer item. The footer item is positioned to
    the bottom, and resized to the width of the page. The default value is \c null.

    \note Assigning a ToolBar, TabBar, or DialogButtonBox as a page footer
    automatically sets the respective \l ToolBar::position, \l TabBar::position,
    or \l DialogButtonBox::position property to \c Footer.

    \sa header, ApplicationWindow::footer
*/
QQuickItem *QQuickPage::footer() const
{
    Q_D(const QQuickPage);
    return d->layout->footer();
}

void QQuickPage::setFooter(QQuickItem *footer)
{
    Q_D(QQuickPage);
    if (!d->layout->setFooter(footer))
        return;
    if (isComponentComplete())
        d->layout->update();
    emit footerChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Page::contentWidth
    \since QtQuick.Controls 2.1

    This property holds the content width. It is used for calculating the total
    implicit width of the page.

    \sa contentHeight
*/
qreal QQuickPage::contentWidth() const
{
    Q_D(const QQuickPage);
    return d->contentWidth;
}

void QQuickPage::setContentWidth(qreal width)
{
    Q_D(QQuickPage);
    if (qFuzzyCompare(d->contentWidth, width))
        return;

    d->contentWidth = width;
    emit contentWidthChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Page::contentHeight
    \since QtQuick.Controls 2.1

    This property holds the content height. It is used for calculating the total
    implicit height of the page.

    \sa contentWidth
*/
qreal QQuickPage::contentHeight() const
{
    Q_D(const QQuickPage);
    return d->contentHeight;
}

void QQuickPage::setContentHeight(qreal height)
{
    Q_D(QQuickPage);
    if (qFuzzyCompare(d->contentHeight, height))
        return;

    d->contentHeight = height;
    emit contentHeightChanged();
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::Page::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children
    of the container.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML
    objects.

    \sa Item::data, contentChildren
*/
QQmlListProperty<QObject> QQuickPage::contentData()
{
    Q_D(QQuickPage);
    return QQmlListProperty<QObject>(d->getContentItem(), nullptr,
                                     QQuickItemPrivate::data_append,
                                     QQuickItemPrivate::data_count,
                                     QQuickItemPrivate::data_at,
                                     QQuickItemPrivate::data_clear);
}

/*!
    \qmlproperty list<Item> QtQuick.Controls::Page::contentChildren

    This property holds the list of content children.

    The list contains all items that have been declared in QML as children
    of the page.

    \note Unlike \c contentData, \c contentChildren does not include non-visual
    QML objects.

    \sa Item::children, contentData
*/
QQmlListProperty<QQuickItem> QQuickPage::contentChildren()
{
    Q_D(QQuickPage);
    return QQmlListProperty<QQuickItem>(d->getContentItem(), nullptr,
                                        QQuickItemPrivate::children_append,
                                        QQuickItemPrivate::children_count,
                                        QQuickItemPrivate::children_at,
                                        QQuickItemPrivate::children_clear);
}

void QQuickPage::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    QQuickControl::contentItemChange(newItem, oldItem);
    if (oldItem)
        disconnect(oldItem, &QQuickItem::childrenChanged, this, &QQuickPage::contentChildrenChanged);
    if (newItem)
        connect(newItem, &QQuickItem::childrenChanged, this, &QQuickPage::contentChildrenChanged);
    emit contentChildrenChanged();
}

void QQuickPage::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickPage);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    d->layout->update();
}

void QQuickPage::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickPage);
    QQuickControl::paddingChange(newPadding, oldPadding);
    d->layout->update();
}

void QQuickPage::spacingChange(qreal newSpacing, qreal oldSpacing)
{
    Q_D(QQuickPage);
    QQuickControl::spacingChange(newSpacing, oldSpacing);
    d->layout->update();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPage::accessibleRole() const
{
    return QAccessible::PageTab;
}

void QQuickPage::accessibilityActiveChanged(bool active)
{
    Q_D(QQuickPage);
    QQuickControl::accessibilityActiveChanged(active);

    if (active)
        setAccessibleName(d->title);
}
#endif

QT_END_NAMESPACE
