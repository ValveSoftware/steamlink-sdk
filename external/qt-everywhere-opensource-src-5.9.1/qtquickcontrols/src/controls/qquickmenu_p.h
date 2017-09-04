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

#ifndef QQUICKMENU_P_H
#define QQUICKMENU_P_H

#include "qquickmenuitem_p.h"

#include <QtCore/qglobal.h>
#include <QtCore/qvariant.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmllist.h>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class QPlatformMenu;
class QQuickMenuPopupWindow1;
class QQuickMenuItemContainer1;
class QQuickWindow;
class QQuickMenuBar1;

typedef QQmlListProperty<QObject> QQuickMenuItems;

class QQuickMenu1 : public QQuickMenuText1
{
    Q_OBJECT
    Q_PROPERTY(QString title READ text WRITE setText NOTIFY titleChanged)
    Q_PROPERTY(QQmlListProperty<QObject> items READ menuItems NOTIFY itemsChanged)
    Q_CLASSINFO("DefaultProperty", "items")

    Q_PROPERTY(int __selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY __selectedIndexChanged)
    Q_PROPERTY(bool __popupVisible READ popupVisible NOTIFY popupVisibleChanged)
    Q_PROPERTY(QQuickItem *__contentItem READ menuContentItem WRITE setMenuContentItem NOTIFY menuContentItemChanged)
    Q_PROPERTY(int __minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged)
    Q_PROPERTY(QFont __font READ font WRITE setFont)
    Q_PROPERTY(qreal __xOffset READ xOffset WRITE setXOffset)
    Q_PROPERTY(qreal __yOffset READ yOffset WRITE setYOffset)
    Q_PROPERTY(QQuickAction1 *__action READ action CONSTANT)
    Q_PROPERTY(QRect __popupGeometry READ popupGeometry NOTIFY __popupGeometryChanged)
    Q_PROPERTY(bool __isProxy READ isProxy WRITE setProxy NOTIFY __proxyChanged)
    Q_ENUMS(MenuType)

public:
    // MenuType must stay in sync with QPlatformMenu::MenuType
    enum MenuType { DefaultMenu = 0, EditMenu };

    Q_INVOKABLE void popup();
    Q_INVOKABLE QQuickMenuItem1 *addItem(const QString &);
    Q_INVOKABLE QQuickMenuItem1 *insertItem(int, const QString &);
    Q_INVOKABLE void addSeparator();
    Q_INVOKABLE void insertSeparator(int);

    Q_INVOKABLE void insertItem(int, QQuickMenuBase1 *);
    Q_INVOKABLE void removeItem(QQuickMenuBase1 *);
    Q_INVOKABLE void clear();

    Q_INVOKABLE void __popup(const QRectF &targetRect, int atItemIndex = -1, MenuType menuType = DefaultMenu);

public Q_SLOTS:
    void __dismissMenu();

    void __closeAndDestroy();
    void __dismissAndDestroy();

Q_SIGNALS:
    void itemsChanged();
    void titleChanged();

    void __selectedIndexChanged();
    void aboutToShow();
    void aboutToHide();
    void popupVisibleChanged();
    void __menuPopupDestroyed();
    void __popupGeometryChanged();
    void menuContentItemChanged();
    void minimumWidthChanged();
    void __proxyChanged();

public:
    QQuickMenu1(QObject *parent = 0);
    virtual ~QQuickMenu1();

    void setVisible(bool);
    void setEnabled(bool);

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    QQuickMenuItems menuItems();
    QQuickMenuBase1 *menuItemAtIndex(int index) const;
    bool contains(QQuickMenuBase1 *);
    int indexOfMenuItem(QQuickMenuBase1 *) const;

    QPlatformMenu *platformMenu() const { return m_platformMenu; }

    int minimumWidth() const { return m_minimumWidth; }
    void setMinimumWidth(int w);

    void setFont(const QFont &font);
    QFont font() const { return m_font; }

    qreal xOffset() const { return m_xOffset; }
    void setXOffset(qreal);
    qreal yOffset() const { return m_yOffset; }
    void setYOffset(qreal);

    QQuickItem *menuContentItem() const { return m_menuContentItem; }
    bool popupVisible() const { return m_popupVisible; }

    bool isNative() { return m_platformMenu != 0; }

    QRect popupGeometry() const;

    bool isProxy() const { return m_proxy; }
    void setProxy(bool proxy) { if (m_proxy != proxy) { m_proxy = proxy; emit __proxyChanged(); } }

    void prepareItemTrigger(QQuickMenuItem1 *);
    void concludeItemTrigger(QQuickMenuItem1 *);
    void destroyMenuPopup();
    void destroyAllMenuPopups();

    QQuickMenuBar1 *menuBar();

protected Q_SLOTS:
    void updateSelectedIndex();

    void setMenuContentItem(QQuickItem *);
    void setPopupVisible(bool);
    void hideMenu();
    void clearPopupWindow();

    void updateText();
    void windowVisibleChanged(bool);
    void platformMenuWindowVisibleChanged(bool);

private:
    QQuickWindow *findParentWindow();
    void syncParentMenuBar();
    QQuickMenuPopupWindow1 *topMenuPopup() const;

    int itemIndexForListIndex(int listIndex) const;
    void itemIndexToListIndex(int, int *, int *) const;

    struct MenuItemIterator
    {
        MenuItemIterator(): index(-1), containerIndex(-1) {}
        int index, containerIndex;
    };

    QQuickMenuBase1 *nextMenuItem(MenuItemIterator *) const;

    static void append_menuItems(QQuickMenuItems *list, QObject *o);
    static int count_menuItems(QQuickMenuItems *list);
    static QObject *at_menuItems(QQuickMenuItems *list, int index);
    static void clear_menuItems(QQuickMenuItems *list);
    void setupMenuItem(QQuickMenuBase1 *item, int platformIndex = -1);

    QPlatformMenu *m_platformMenu;
    QList<QQuickMenuBase1 *> m_menuItems;
    QHash<QObject *, QQuickMenuItemContainer1 *> m_containers;
    int m_itemsCount;
    int m_selectedIndex;
    int m_highlightedIndex;
    QQuickWindow *m_parentWindow;
    int m_minimumWidth;
    QQuickMenuPopupWindow1 *m_popupWindow;
    QQuickItem * m_menuContentItem;
    bool m_popupVisible;
    int m_containersCount;
    qreal m_xOffset;
    qreal m_yOffset;
    QFont m_font;
    int m_triggerCount;
    bool m_proxy;
    QMetaObject::Connection m_windowConnection;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickMenu1)

#endif // QQUICKMENU_P_H
