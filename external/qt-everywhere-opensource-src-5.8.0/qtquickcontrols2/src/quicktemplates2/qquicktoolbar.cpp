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

#include "qquicktoolbar_p.h"
#include "qquickpane_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype ToolBar
    \inherits Pane
    \instantiates QQuickToolBar
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-containers
    \brief Container for context-sensitive controls.

    ToolBar is a container of application-wide and context sensitive
    actions and controls, such as navigation buttons and search fields.
    ToolBar is commonly used as a \l {ApplicationWindow::header}{header}
    or a \l {ApplicationWindow::footer}{footer} of an \l ApplicationWindow.

    ToolBar does not provide a layout of its own, but requires you to
    position its contents, for instance by creating a \l RowLayout. If only
    a single item is used within the ToolBar, it will resize to fit the
    implicit size of its contained item. This makes it particularly suitable
    for use together with layouts.

    \image qtquickcontrols2-toolbar.png

    \code
    ApplicationWindow {
        visible:true

        header: ToolBar {
            RowLayout {
                anchors.fill: parent
                ToolButton {
                    text: qsTr("‹")
                    onClicked: stack.pop()
                }
                Label {
                    text: "Title"
                    elide: Label.ElideRight
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: qsTr("⋮")
                    onClicked: menu.open()
                }
            }
        }

        StackView {
            id: stack
            anchors.fill: parent
        }
    }
    \endcode

    \sa ApplicationWindow, ToolButton, {Customizing ToolBar}, {Container Controls}
*/

class QQuickToolBarPrivate : public QQuickPanePrivate
{
public:
    QQuickToolBarPrivate() : position(QQuickToolBar::Header) { }

    QQuickToolBar::Position position;
};

QQuickToolBar::QQuickToolBar(QQuickItem *parent) :
    QQuickPane(*(new QQuickToolBarPrivate), parent)
{
}

/*!
    \qmlproperty enumeration QtQuick.Controls::ToolBar::position

    This property holds the position of the toolbar.

    \note If the toolbar is assigned as a header or footer of \l ApplicationWindow
    or \l Page, the appropriate position is set automatically.

    Possible values:
    \value ToolBar.Header The toolbar is at the top, as a window or page header.
    \value ToolBar.Footer The toolbar is at the bottom, as a window or page footer.

    The default value is style-specific.

    \sa ApplicationWindow::header, ApplicationWindow::footer, Page::header, Page::footer
*/
QQuickToolBar::Position QQuickToolBar::position() const
{
    Q_D(const QQuickToolBar);
    return d->position;
}

void QQuickToolBar::setPosition(Position position)
{
    Q_D(QQuickToolBar);
    if (d->position == position)
        return;

    d->position = position;
    emit positionChanged();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickToolBar::accessibleRole() const
{
    return QAccessible::ToolBar;
}
#endif

QT_END_NAMESPACE
