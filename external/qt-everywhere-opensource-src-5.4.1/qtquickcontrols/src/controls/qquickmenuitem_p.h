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
class QQuickAction;
class QQuickExclusiveGroup;
class QQuickMenu;
class QQuickMenuItemContainer;

class QQuickMenuItemType
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

class QQuickMenuBase: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QQuickMenuItemType::MenuItemType type READ type CONSTANT)

    Q_PROPERTY(QObject *__parentMenu READ parentMenuOrMenuBar CONSTANT)
    Q_PROPERTY(bool __isNative READ isNative CONSTANT)
    Q_PROPERTY(QQuickItem *__visualItem READ visualItem WRITE setVisualItem)

Q_SIGNALS:
    void visibleChanged();

public:
    QQuickMenuBase(QObject *parent, int type);
    ~QQuickMenuBase();

    bool visible() const { return m_visible; }
    virtual void setVisible(bool);

    QQuickMenu *parentMenu() const;
    QObject *parentMenuOrMenuBar() const;
    virtual void setParentMenu(QQuickMenu *parentMenu);

    QQuickMenuItemContainer *container() const;
    void setContainer(QQuickMenuItemContainer *);

    inline QPlatformMenuItem *platformItem() { return m_platformItem; }
    void syncWithPlatformMenu();

    QQuickItem *visualItem() const;
    void setVisualItem(QQuickItem *item);

    QQuickMenuItemType::MenuItemType type() { return m_type; }
    virtual bool isNative() { return m_platformItem != 0; }

private:
    bool m_visible;
    QQuickMenuItemType::MenuItemType m_type;
    QQuickMenu *m_parentMenu;
    QQuickMenuItemContainer *m_container;
    QPlatformMenuItem *m_platformItem;
    QPointer<QQuickItem> m_visualItem;
};

class QQuickMenuSeparator : public QQuickMenuBase
{
    Q_OBJECT
public:
    QQuickMenuSeparator(QObject *parent = 0);
};

class QQuickMenuText : public QQuickMenuBase
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
    QQuickMenuText(QObject *parent, QQuickMenuItemType::MenuItemType type);
    ~QQuickMenuText();

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
    virtual QQuickAction *action() const { return m_action; }

protected Q_SLOTS:
    virtual void updateText();
    void updateEnabled();
    void updateIcon();

private:
    QQuickAction *m_action;
};

class QQuickMenuItem : public QQuickMenuText
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool checkable READ checkable WRITE setCheckable NOTIFY checkableChanged)
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY toggled)
    Q_PROPERTY(QQuickExclusiveGroup *exclusiveGroup READ exclusiveGroup WRITE setExclusiveGroup NOTIFY exclusiveGroupChanged)
    Q_PROPERTY(QVariant shortcut READ shortcut WRITE setShortcut NOTIFY shortcutChanged)
    Q_PROPERTY(QQuickAction *action READ boundAction WRITE setBoundAction NOTIFY actionChanged)

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
    QQuickMenuItem(QObject *parent = 0);
    ~QQuickMenuItem();

    void setEnabled(bool enabled);

    QString text() const;

    QUrl iconSource() const;
    QString iconName() const;

    QQuickAction *boundAction() { return m_boundAction; }
    void setBoundAction(QQuickAction *a);

    QVariant shortcut() const;
    void setShortcut(const QVariant &shortcut);

    bool checkable() const;
    void setCheckable(bool checkable);

    bool checked() const;
    void setChecked(bool checked);

    QQuickExclusiveGroup *exclusiveGroup() const;
    void setExclusiveGroup(QQuickExclusiveGroup *);

    void setParentMenu(QQuickMenu *parentMenu);

protected Q_SLOTS:
    void updateShortcut();
    void updateCheckable();
    void updateChecked();
    void bindToAction(QQuickAction *action);
    void unbindFromAction(QObject *action);

protected:
    QIcon icon() const;
    QQuickAction *action() const;

private:
    QQuickAction *m_boundAction;
};

QT_END_NAMESPACE

#endif // QQUICKMENUITEM_P_H
