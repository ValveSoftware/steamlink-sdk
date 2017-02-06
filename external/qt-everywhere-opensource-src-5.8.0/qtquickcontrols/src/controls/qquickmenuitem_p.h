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

#ifndef QQUICKMENUITEM_P_H
#define QQUICKMENUITEM_P_H

#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>
#include <QtCore/qpointer.h>
#include <QtCore/qurl.h>
#include <QtGui/qicon.h>
#include <QtQml/QQmlListProperty>

QT_BEGIN_NAMESPACE

class QUrl;
class QPlatformMenuItem;
class QQuickItem;
class QQuickAction1;
class QQuickExclusiveGroup1;
class QQuickMenu1;
class QQuickMenuItemContainer1;

class QQuickMenuItemType1
{
    Q_GADGET
    Q_ENUMS(MenuItemType)

public:
    enum MenuItemType {
        Separator = 0,
        Item,
        Menu,
        ScrollIndicator
    };
};

class QQuickMenuBase1: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QQuickMenuItemType1::MenuItemType type READ type CONSTANT)

    Q_PROPERTY(QObject *__parentMenu READ parentMenuOrMenuBar CONSTANT)
    Q_PROPERTY(bool __isNative READ isNative CONSTANT)
    Q_PROPERTY(QQuickItem *__visualItem READ visualItem WRITE setVisualItem)

Q_SIGNALS:
    void visibleChanged();

public:
    QQuickMenuBase1(QObject *parent, int type);
    ~QQuickMenuBase1();

    bool visible() const { return m_visible; }
    virtual void setVisible(bool);

    QQuickMenu1 *parentMenu() const;
    QObject *parentMenuOrMenuBar() const;
    virtual void setParentMenu(QQuickMenu1 *parentMenu);

    QQuickMenuItemContainer1 *container() const;
    void setContainer(QQuickMenuItemContainer1 *);

    inline QPlatformMenuItem *platformItem() { return m_platformItem; }
    void syncWithPlatformMenu();

    QQuickItem *visualItem() const;
    void setVisualItem(QQuickItem *item);

    QQuickMenuItemType1::MenuItemType type() { return m_type; }
    virtual bool isNative() { return m_platformItem != 0; }

private:
    bool m_visible;
    QQuickMenuItemType1::MenuItemType m_type;
    QQuickMenu1 *m_parentMenu;
    QQuickMenuItemContainer1 *m_container;
    QPlatformMenuItem *m_platformItem;
    QPointer<QQuickItem> m_visualItem;
};

class QQuickMenuSeparator1 : public QQuickMenuBase1
{
    Q_OBJECT
public:
    QQuickMenuSeparator1(QObject *parent = 0);
};

class QQuickMenuText1 : public QQuickMenuBase1
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)

    Q_PROPERTY(QVariant __icon READ iconVariant NOTIFY __iconChanged)

Q_SIGNALS:
    void enabledChanged();
    void iconSourceChanged();
    void iconNameChanged();

    void __textChanged();
    void __iconChanged();

public:
    QQuickMenuText1(QObject *parent, QQuickMenuItemType1::MenuItemType type);
    ~QQuickMenuText1();

    bool enabled() const;
    virtual void setEnabled(bool enabled);

    virtual QString text() const;
    void setText(const QString &text);

    virtual QUrl iconSource() const;
    void setIconSource(const QUrl &icon);
    virtual QString iconName() const;
    void setIconName(const QString &icon);

    QVariant iconVariant() const { return QVariant(icon()); }

protected:
    virtual QIcon icon() const;
    virtual QQuickAction1 *action() const { return m_action; }

protected Q_SLOTS:
    virtual void updateText();
    void updateEnabled();
    void updateIcon();

private:
    QQuickAction1 *m_action;
};

class QQuickMenuItem1 : public QQuickMenuText1
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool checkable READ checkable WRITE setCheckable NOTIFY checkableChanged)
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY toggled)
    Q_PROPERTY(QQuickExclusiveGroup1 *exclusiveGroup READ exclusiveGroup WRITE setExclusiveGroup NOTIFY exclusiveGroupChanged)
    Q_PROPERTY(QVariant shortcut READ shortcut WRITE setShortcut NOTIFY shortcutChanged)
    Q_PROPERTY(QQuickAction1 *action READ boundAction WRITE setBoundAction NOTIFY actionChanged)

public Q_SLOTS:
    void trigger();

Q_SIGNALS:
    void triggered();
    void toggled(bool checked);

    void textChanged();
    void checkableChanged();
    void exclusiveGroupChanged();
    void shortcutChanged();
    void actionChanged();

public:
    QQuickMenuItem1(QObject *parent = 0);
    ~QQuickMenuItem1();

    void setEnabled(bool enabled);

    QString text() const;

    QUrl iconSource() const;
    QString iconName() const;

    QQuickAction1 *boundAction() { return m_boundAction; }
    void setBoundAction(QQuickAction1 *a);

    QVariant shortcut() const;
    void setShortcut(const QVariant &shortcut);

    bool checkable() const;
    void setCheckable(bool checkable);

    bool checked() const;
    void setChecked(bool checked);

    QQuickExclusiveGroup1 *exclusiveGroup() const;
    void setExclusiveGroup(QQuickExclusiveGroup1 *);

    void setParentMenu(QQuickMenu1 *parentMenu);

protected Q_SLOTS:
    void updateShortcut();
    void updateCheckable();
    void updateChecked();
    void bindToAction(QQuickAction1 *action);
    void unbindFromAction(QObject *action);

protected:
    QIcon icon() const;
    QQuickAction1 *action() const;

private:
    QQuickAction1 *m_boundAction;
};

QT_END_NAMESPACE

#endif // QQUICKMENUITEM_P_H
