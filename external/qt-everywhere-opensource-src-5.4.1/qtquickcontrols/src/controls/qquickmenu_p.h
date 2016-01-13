/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
class QQuickMenuPopupWindow;
class QQuickMenuItemContainer;
class QQuickWindow;

typedef QQmlListProperty<QObject> QQuickMenuItems;

class QQuickMenu : public QQuickMenuText
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
    Q_PROPERTY(QQuickAction *__action READ action CONSTANT)
    Q_PROPERTY(QRect __popupGeometry READ popupGeometry NOTIFY __popupGeometryChanged)
    Q_ENUMS(MenuType)

public:
    // MenuType must stay in sync with QPlatformMenu::MenuType
    enum MenuType { DefaultMenu = 0, EditMenu };

    Q_INVOKABLE void popup();
    Q_INVOKABLE QQuickMenuItem *addItem(QString);
    Q_INVOKABLE QQuickMenuItem *insertItem(int, QString);
    Q_INVOKABLE void addSeparator();
    Q_INVOKABLE void insertSeparator(int);

    Q_INVOKABLE void insertItem(int, QQuickMenuBase *);
    Q_INVOKABLE void removeItem(QQuickMenuBase *);
    Q_INVOKABLE void clear();

    Q_INVOKABLE void __popup(const QRectF &targetRect, int atItemIndex = -1, MenuType menuType = DefaultMenu);

public Q_SLOTS:
    void __closeMenu();
    void __dismissMenu();

Q_SIGNALS:
    void itemsChanged();
    void titleChanged();

    void __selectedIndexChanged();
    void __menuClosed();
    void popupVisibleChanged();
    void __popupGeometryChanged();
    void menuContentItemChanged();
    void minimumWidthChanged();

public:
    QQuickMenu(QObject *parent = 0);
    virtual ~QQuickMenu();

    void setVisible(bool);
    void setEnabled(bool);

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    QQuickMenuItems menuItems();
    QQuickMenuBase *menuItemAtIndex(int index) const;
    bool contains(QQuickMenuBase *);
    int indexOfMenuItem(QQuickMenuBase *) const;

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

protected Q_SLOTS:
    void updateSelectedIndex();

    void setMenuContentItem(QQuickItem *);
    void setPopupVisible(bool);

    void updateText();
    void windowVisibleChanged(bool);

private:
    QQuickWindow *findParentWindow();
    void syncParentMenuBar();

    int itemIndexForListIndex(int listIndex) const;
    void itemIndexToListIndex(int, int *, int *) const;

    struct MenuItemIterator
    {
        MenuItemIterator(): index(-1), containerIndex(-1) {}
        int index, containerIndex;
    };

    QQuickMenuBase *nextMenuItem(MenuItemIterator *) const;

    static void append_menuItems(QQuickMenuItems *list, QObject *o);
    static int count_menuItems(QQuickMenuItems *list);
    static QObject *at_menuItems(QQuickMenuItems *list, int index);
    static void clear_menuItems(QQuickMenuItems *list);
    void setupMenuItem(QQuickMenuBase *item, int platformIndex = -1);

    QPlatformMenu *m_platformMenu;
    QList<QQuickMenuBase *> m_menuItems;
    QHash<QObject *, QQuickMenuItemContainer *> m_containers;
    int m_itemsCount;
    int m_selectedIndex;
    int m_highlightedIndex;
    QQuickWindow *m_parentWindow;
    int m_minimumWidth;
    QQuickMenuPopupWindow *m_popupWindow;
    QQuickItem * m_menuContentItem;
    bool m_popupVisible;
    int m_containersCount;
    qreal m_xOffset;
    qreal m_yOffset;
    QFont m_font;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickMenu)

#endif // QQUICKMENU_P_H
