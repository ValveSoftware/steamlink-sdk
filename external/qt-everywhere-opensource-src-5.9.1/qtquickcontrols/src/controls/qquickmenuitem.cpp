/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickmenuitem_p.h"
#include "qquickaction_p.h"
#include "qquickmenu_p.h"
#include "qquickmenuitemcontainer_p.h"

#include <private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformmenu.h>
#include <qquickitem.h>

QT_BEGIN_NAMESPACE

QQuickMenuBase1::QQuickMenuBase1(QObject *parent, int type)
    : QObject(parent), m_visible(true), m_type(static_cast<QQuickMenuItemType1::MenuItemType>(type))
    , m_parentMenu(0), m_container(0), m_platformItem(0), m_visualItem(0)
{
    if (type >= 0 && QGuiApplication::platformName() != QStringLiteral("xcb")) { // QTBUG-51372)
        m_platformItem = QGuiApplicationPrivate::platformTheme()->createPlatformMenuItem();
        if (m_platformItem)
            m_platformItem->setRole(QPlatformMenuItem::TextHeuristicRole);
    }
}

QQuickMenuBase1::~QQuickMenuBase1()
{
    if (parentMenu())
        parentMenu()->removeItem(this);
    setParentMenu(0);
    if (m_platformItem) {
        delete m_platformItem;
        m_platformItem = 0;
    }
}

void QQuickMenuBase1::setVisible(bool v)
{
    if (v != m_visible) {
        m_visible = v;

        if (m_platformItem) {
            m_platformItem->setVisible(m_visible);
            syncWithPlatformMenu();
        }

        emit visibleChanged();
    }
}

QObject *QQuickMenuBase1::parentMenuOrMenuBar() const
{
    if (!m_parentMenu)
        return parent();
    return m_parentMenu;
}

QQuickMenu1 *QQuickMenuBase1::parentMenu() const
{
    return m_parentMenu;
}

void QQuickMenuBase1::setParentMenu(QQuickMenu1 *parentMenu)
{
    if (m_platformItem && m_parentMenu && m_parentMenu->platformMenu())
        m_parentMenu->platformMenu()->removeMenuItem(m_platformItem);

    m_parentMenu = parentMenu;
}

QQuickMenuItemContainer1 *QQuickMenuBase1::container() const
{
    return m_container;
}

void QQuickMenuBase1::setContainer(QQuickMenuItemContainer1 *c)
{
    m_container = c;
}

void QQuickMenuBase1::syncWithPlatformMenu()
{
    QQuickMenu1 *menu = parentMenu();
    if (menu && menu->platformMenu() && platformItem()
        && menu->contains(this)) // If not, it'll be added later and then sync'ed
        menu->platformMenu()->syncMenuItem(platformItem());
}

QQuickItem *QQuickMenuBase1::visualItem() const
{
    return m_visualItem;
}

void QQuickMenuBase1::setVisualItem(QQuickItem *item)
{
    m_visualItem = item;
}

/*!
    \qmltype MenuSeparator
    \instantiates QQuickMenuSeparator1
    \inqmlmodule QtQuick.Controls
    \ingroup menus
    \ingroup controls
    \brief MenuSeparator provides a separator for items inside a menu.

    \image menu.png

    \code
    Menu {
        text: "Edit"

        MenuItem {
            text: "Cut"
            shortcut: "Ctrl+X"
            onTriggered: ...
        }

        MenuItem {
            text: "Copy"
            shortcut: "Ctrl+C"
            onTriggered: ...
        }

        MenuItem {
            text: "Paste"
            shortcut: "Ctrl+V"
            onTriggered: ...
        }
        MenuSeparator { }
        MenuItem {
            text: "Do Nothing"
            shortcut: "Ctrl+E,Shift+Ctrl+X"
            enabled: false
        }
    }
    \endcode

    \sa Menu, MenuItem
*/

/*!
    \qmlproperty bool MenuSeparator::visible

    Whether the menu separator should be visible.
*/

/*!
    \qmlproperty enumeration MenuSeparator::type
    \readonly

    This property is read-only and constant, and its value is \c MenuItemType.Separator.
*/

QQuickMenuSeparator1::QQuickMenuSeparator1(QObject *parent)
    : QQuickMenuBase1(parent, QQuickMenuItemType1::Separator)
{
    if (platformItem())
        platformItem()->setIsSeparator(true);
}

QQuickMenuText1::QQuickMenuText1(QObject *parent, QQuickMenuItemType1::MenuItemType type)
    : QQuickMenuBase1(parent, type), m_action(new QQuickAction1(this))
{
    connect(m_action, SIGNAL(enabledChanged()), this, SLOT(updateEnabled()));
    connect(m_action, SIGNAL(textChanged()), this, SLOT(updateText()));
    connect(m_action, SIGNAL(iconNameChanged()), this, SLOT(updateIcon()));
    connect(m_action, SIGNAL(iconNameChanged()), this, SIGNAL(iconNameChanged()));
    connect(m_action, SIGNAL(iconSourceChanged()), this, SLOT(updateIcon()));
    connect(m_action, SIGNAL(iconSourceChanged()), this, SIGNAL(iconSourceChanged()));
}

QQuickMenuText1::~QQuickMenuText1()
{
    delete m_action;
}

bool QQuickMenuText1::enabled() const
{
    return action()->isEnabled();
}

void QQuickMenuText1::setEnabled(bool e)
{
    action()->setEnabled(e);
}

QString QQuickMenuText1::text() const
{
    return m_action->text();
}

void QQuickMenuText1::setText(const QString &t)
{
    m_action->setText(t);
}

QUrl QQuickMenuText1::iconSource() const
{
    return m_action->iconSource();
}

void QQuickMenuText1::setIconSource(const QUrl &iconSource)
{
    m_action->setIconSource(iconSource);
}

QString QQuickMenuText1::iconName() const
{
    return m_action->iconName();
}

void QQuickMenuText1::setIconName(const QString &iconName)
{
    m_action->setIconName(iconName);
}

QIcon QQuickMenuText1::icon() const
{
    return m_action->icon();
}

void QQuickMenuText1::updateText()
{
    if (platformItem()) {
        platformItem()->setText(text());
        syncWithPlatformMenu();
    }
    emit __textChanged();
}

void QQuickMenuText1::updateEnabled()
{
    if (platformItem()) {
        platformItem()->setEnabled(enabled());
        syncWithPlatformMenu();
    }
    emit enabledChanged();
}

void QQuickMenuText1::updateIcon()
{
    if (platformItem()) {
        platformItem()->setIcon(icon());
        syncWithPlatformMenu();
    }
    emit __iconChanged();
}

/*!
    \qmltype MenuItem
    \instantiates QQuickMenuItem1
    \ingroup menus
    \ingroup controls
    \inqmlmodule QtQuick.Controls
    \brief MenuItem provides an item to add in a menu or a menu bar.

    \image menu.png

    \code
    Menu {
        text: "Edit"

        MenuItem {
            text: "Cut"
            shortcut: "Ctrl+X"
            onTriggered: ...
        }

        MenuItem {
            text: "Copy"
            shortcut: "Ctrl+C"
            onTriggered: ...
        }

        MenuItem {
            text: "Paste"
            shortcut: "Ctrl+V"
            onTriggered: ...
        }
    }
    \endcode

    \sa MenuBar, Menu, MenuSeparator, Action
*/

/*!
    \qmlproperty bool MenuItem::visible

    Whether the menu item should be visible. Defaults to \c true.
*/

/*!
    \qmlproperty enumeration MenuItem::type
    \readonly

    This property is read-only and constant, and its value is \c MenuItemType.Item.
*/

/*!
    \qmlproperty string MenuItem::text

    Text for the menu item. Overrides the item's bound action \c text property.

    Mnemonics are supported by prefixing the shortcut letter with \&.
    For instance, \c "\&Open" will bind the \c Alt-O shortcut to the
    \c "Open" menu item. Note that not all platforms support mnemonics.

    Defaults to an empty string.

    \sa Action::text
*/

/*!
    \qmlproperty bool MenuItem::enabled

    Whether the menu item is enabled, and responsive to user interaction. Defaults to \c true.
*/

/*!
    \qmlproperty url MenuItem::iconSource

    Sets the icon file or resource url for the \l MenuItem icon.
    Overrides the item's bound action \c iconSource property. Defaults to an empty URL.

    \sa iconName, Action::iconSource
*/

/*!
    \qmlproperty string MenuItem::iconName

    Sets the icon name for the \l MenuItem icon. This will pick the icon
    with the given name from the current theme. Overrides the item's bound
    action \c iconName property. Defaults to an empty string.

    \include icons.qdocinc iconName

    \sa iconSource, Action::iconName
*/

/*! \qmlsignal MenuItem::triggered()

    Emitted when either the menu item or its bound action have been activated.

    \sa trigger(), Action::triggered, Action::toggled

    The corresponding handler is \c onTriggered.
*/

/*! \qmlmethod void MenuItem::trigger()

    Manually trigger a menu item. Will also trigger the item's bound action.

    \sa triggered, Action::trigger()
*/

/*!
    \qmlproperty keysequence MenuItem::shortcut

    Shortcut bound to the menu item. The keysequence can be a string
    or a \l {QKeySequence::StandardKey}{standard key}.

    Defaults to an empty string.

    \qml
    MenuItem {
        id: copyItem
        text: qsTr("&Copy")
        shortcut: StandardKey.Copy
    }
    \endqml

    \sa Action::shortcut
*/

/*!
    \qmlproperty bool MenuItem::checkable

    Whether the menu item can be checked, or toggled. Defaults to \c false.

    \sa checked
*/

/*!
    \qmlproperty bool MenuItem::checked

    If the menu item is checkable, this property reflects its checked state. Defaults to \c false.

    \sa checkable, Action::toggled
*/

/*! \qmlproperty ExclusiveGroup MenuItem::exclusiveGroup

    If a menu item is checkable, an \l ExclusiveGroup can be attached to it.
    All the menu items sharing the same exclusive group, and by extension, any \l Action sharing it,
    become mutually exclusive selectable, meaning that only the last checked menu item will
    actually be checked.

    Defaults to \c null, meaning no exclusive behavior is to be expected.

    \sa checked, checkable
*/

/*! \qmlsignal MenuItem::toggled(checked)

    Emitted whenever a menu item's \c checked property changes.
    This usually happens at the same time as \l triggered.

    \sa checked, triggered, Action::triggered, Action::toggled

    The corresponding handler is \c onToggled.
*/

/*!
    \qmlproperty Action MenuItem::action

    The action bound to this menu item. It will provide values for all the properties of the menu item.
    However, it is possible to override the action's \c text, \c iconSource, and \c iconName
    properties by just assigning these properties, allowing some customization.

    In addition, the menu item \c triggered() and \c toggled() signals will not be emitted.
    Instead, the action \c triggered() and \c toggled() signals will be.

    Defaults to \c null, meaning no action is bound to the menu item.
*/

QQuickMenuItem1::QQuickMenuItem1(QObject *parent)
    : QQuickMenuText1(parent, QQuickMenuItemType1::Item), m_boundAction(0)
{
    connect(this, SIGNAL(__textChanged()), this, SIGNAL(textChanged()));

    connect(action(), SIGNAL(shortcutChanged(QVariant)), this, SLOT(updateShortcut()));
    connect(action(), SIGNAL(triggered()), this, SIGNAL(triggered()));
    connect(action(), SIGNAL(checkableChanged()), this, SLOT(updateCheckable()));
    connect(action(), SIGNAL(toggled(bool)), this, SLOT(updateChecked()));
    if (platformItem())
        connect(platformItem(), SIGNAL(activated()), this, SLOT(trigger()), Qt::QueuedConnection);
}

QQuickMenuItem1::~QQuickMenuItem1()
{
    unbindFromAction(m_boundAction);
    if (platformItem())
        disconnect(platformItem(), SIGNAL(activated()), this, SLOT(trigger()));
}

void QQuickMenuItem1::setParentMenu(QQuickMenu1 *parentMenu)
{
    QQuickMenuText1::setParentMenu(parentMenu);
    if (parentMenu)
        connect(this, SIGNAL(triggered()), parentMenu, SLOT(updateSelectedIndex()));
}

void QQuickMenuItem1::bindToAction(QQuickAction1 *action)
{
    m_boundAction = action;

    connect(m_boundAction, SIGNAL(destroyed(QObject*)), this, SLOT(unbindFromAction(QObject*)));

    connect(m_boundAction, SIGNAL(triggered()), this, SIGNAL(triggered()));
    connect(m_boundAction, SIGNAL(toggled(bool)), this, SLOT(updateChecked()));
    connect(m_boundAction, SIGNAL(exclusiveGroupChanged()), this, SIGNAL(exclusiveGroupChanged()));
    connect(m_boundAction, SIGNAL(enabledChanged()), this, SLOT(updateEnabled()));
    connect(m_boundAction, SIGNAL(textChanged()), this, SLOT(updateText()));
    connect(m_boundAction, SIGNAL(shortcutChanged(QVariant)), this, SLOT(updateShortcut()));
    connect(m_boundAction, SIGNAL(checkableChanged()), this, SLOT(updateCheckable()));
    connect(m_boundAction, SIGNAL(iconNameChanged()), this, SLOT(updateIcon()));
    connect(m_boundAction, SIGNAL(iconNameChanged()), this, SIGNAL(iconNameChanged()));
    connect(m_boundAction, SIGNAL(iconSourceChanged()), this, SLOT(updateIcon()));
    connect(m_boundAction, SIGNAL(iconSourceChanged()), this, SIGNAL(iconSourceChanged()));

    if (m_boundAction->parent() != this) {
        updateText();
        updateShortcut();
        updateEnabled();
        updateIcon();
        if (checkable())
            updateChecked();
    }
}

void QQuickMenuItem1::unbindFromAction(QObject *o)
{
    if (!o)
        return;

    if (o == m_boundAction)
        m_boundAction = 0;

    QQuickAction1 *action = qobject_cast<QQuickAction1 *>(o);
    if (!action)
        return;

    disconnect(action, SIGNAL(destroyed(QObject*)), this, SLOT(unbindFromAction(QObject*)));

    disconnect(action, SIGNAL(triggered()), this, SIGNAL(triggered()));
    disconnect(action, SIGNAL(toggled(bool)), this, SLOT(updateChecked()));
    disconnect(action, SIGNAL(exclusiveGroupChanged()), this, SIGNAL(exclusiveGroupChanged()));
    disconnect(action, SIGNAL(enabledChanged()), this, SLOT(updateEnabled()));
    disconnect(action, SIGNAL(textChanged()), this, SLOT(updateText()));
    disconnect(action, SIGNAL(shortcutChanged(QVariant)), this, SLOT(updateShortcut()));
    disconnect(action, SIGNAL(checkableChanged()), this, SLOT(updateCheckable()));
    disconnect(action, SIGNAL(iconNameChanged()), this, SLOT(updateIcon()));
    disconnect(action, SIGNAL(iconNameChanged()), this, SIGNAL(iconNameChanged()));
    disconnect(action, SIGNAL(iconSourceChanged()), this, SLOT(updateIcon()));
    disconnect(action, SIGNAL(iconSourceChanged()), this, SIGNAL(iconSourceChanged()));
}

QQuickAction1 *QQuickMenuItem1::action() const
{
    if (m_boundAction)
        return m_boundAction;
    return QQuickMenuText1::action();
}

void QQuickMenuItem1::setBoundAction(QQuickAction1 *a)
{
    if (a == m_boundAction)
        return;

    unbindFromAction(m_boundAction);

    bindToAction(a);
    emit actionChanged();
}

QString QQuickMenuItem1::text() const
{
    QString ownText = QQuickMenuText1::text();
    if (!ownText.isNull())
        return ownText;
    return m_boundAction ? m_boundAction->text() : QString();
}

QUrl QQuickMenuItem1::iconSource() const
{
    QUrl ownIconSource = QQuickMenuText1::iconSource();
    if (!ownIconSource.isEmpty())
        return ownIconSource;
    return m_boundAction ? m_boundAction->iconSource() : QUrl();
}

QString QQuickMenuItem1::iconName() const
{
    QString ownIconName = QQuickMenuText1::iconName();
    if (!ownIconName.isEmpty())
        return ownIconName;
    return m_boundAction ? m_boundAction->iconName() : QString();
}

QIcon QQuickMenuItem1::icon() const
{
    QIcon ownIcon = QQuickMenuText1::icon();
    if (!ownIcon.isNull())
        return ownIcon;
    return m_boundAction ? m_boundAction->icon() : QIcon();
}

QVariant QQuickMenuItem1::shortcut() const
{
    return action()->shortcut();
}

void QQuickMenuItem1::setShortcut(const QVariant &shortcut)
{
    if (!m_boundAction)
        action()->setShortcut(shortcut);
}

void QQuickMenuItem1::updateShortcut()
{
#if QT_CONFIG(shortcut)
    if (platformItem()) {
        QKeySequence sequence;
        QVariant var = shortcut();
        if (var.type() == QVariant::Int)
            sequence = QKeySequence(static_cast<QKeySequence::StandardKey>(var.toInt()));
        else
            sequence = QKeySequence::fromString(var.toString(), QKeySequence::NativeText);
        platformItem()->setShortcut(sequence);
        syncWithPlatformMenu();
    }
    emit shortcutChanged();
#endif // QT_CONFIG(shortcut)
}

bool QQuickMenuItem1::checkable() const
{
    return action()->isCheckable();
}

void QQuickMenuItem1::setCheckable(bool checkable)
{
    if (!m_boundAction)
        action()->setCheckable(checkable);
}

void QQuickMenuItem1::updateCheckable()
{
    if (platformItem()) {
        platformItem()->setCheckable(checkable());
        syncWithPlatformMenu();
    }

    emit checkableChanged();
}

bool QQuickMenuItem1::checked() const
{
    return action()->isChecked();
}

void QQuickMenuItem1::setChecked(bool checked)
{
    if (!m_boundAction)
        action()->setChecked(checked);
}

void QQuickMenuItem1::updateChecked()
{
    bool checked = this->checked();
    if (platformItem()) {
        platformItem()->setChecked(checked);
        syncWithPlatformMenu();
    }

    emit toggled(checked);
}

QQuickExclusiveGroup1 *QQuickMenuItem1::exclusiveGroup() const
{
    return action()->exclusiveGroup();
}

void QQuickMenuItem1::setExclusiveGroup(QQuickExclusiveGroup1 *eg)
{
    if (!m_boundAction)
        action()->setExclusiveGroup(eg);
}

void QQuickMenuItem1::setEnabled(bool enabled)
{
    if (!m_boundAction)
        action()->setEnabled(enabled);
}

void QQuickMenuItem1::trigger()
{
    QPointer<QQuickMenu1> menu(parentMenu());
    if (menu)
        menu->prepareItemTrigger(this);
    action()->trigger(this);
    if (menu)
        menu->concludeItemTrigger(this);
}

QT_END_NAMESPACE
