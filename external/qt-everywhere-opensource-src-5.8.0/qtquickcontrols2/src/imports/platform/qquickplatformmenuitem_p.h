/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
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

#ifndef QQUICKPLATFORMMENUITEM_P_H
#define QQUICKPLATFORMMENUITEM_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtGui/qfont.h>
#include <QtGui/qpa/qplatformmenu.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QPlatformMenuItem;
class QQuickPlatformMenu;
class QQuickPlatformIconLoader;
class QQuickPlatformMenuItemGroup;

class QQuickPlatformMenuItem : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQuickPlatformMenu *menu READ menu NOTIFY menuChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenu *subMenu READ subMenu NOTIFY subMenuChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenuItemGroup *group READ group WRITE setGroup NOTIFY groupChanged FINAL)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(bool separator READ isSeparator WRITE setSeparator NOTIFY separatorChanged FINAL)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY checkableChanged FINAL)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY checkedChanged FINAL)
    Q_PROPERTY(QPlatformMenuItem::MenuRole role READ role WRITE setRole NOTIFY roleChanged FINAL)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)
    Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged FINAL)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged FINAL)
    Q_PROPERTY(QVariant shortcut READ shortcut WRITE setShortcut NOTIFY shortcutChanged FINAL)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged FINAL)
    Q_ENUMS(QPlatformMenuItem::MenuRole)

public:
    explicit QQuickPlatformMenuItem(QObject *parent = nullptr);
    ~QQuickPlatformMenuItem();

    QPlatformMenuItem *handle() const;
    QPlatformMenuItem *create();
    void sync();

    QQuickPlatformMenu *menu() const;
    void setMenu(QQuickPlatformMenu* menu);

    QQuickPlatformMenu *subMenu() const;
    void setSubMenu(QQuickPlatformMenu *menu);

    QQuickPlatformMenuItemGroup *group() const;
    void setGroup(QQuickPlatformMenuItemGroup *group);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isVisible() const;
    void setVisible(bool visible);

    bool isSeparator() const;
    void setSeparator(bool separator);

    bool isCheckable() const;
    void setCheckable(bool checkable);

    bool isChecked() const;
    void setChecked(bool checked);

    QPlatformMenuItem::MenuRole role() const;
    void setRole(QPlatformMenuItem::MenuRole role);

    QString text() const;
    void setText(const QString &text);

    QUrl iconSource() const;
    void setIconSource(const QUrl &source);

    QString iconName() const;
    void setIconName(const QString &name);

    QVariant shortcut() const;
    void setShortcut(const QVariant& shortcut);

    QFont font() const;
    void setFont(const QFont &font);

public Q_SLOTS:
    void toggle();

Q_SIGNALS:
    void triggered();
    void hovered();

    void menuChanged();
    void subMenuChanged();
    void groupChanged();
    void enabledChanged();
    void visibleChanged();
    void separatorChanged();
    void checkableChanged();
    void checkedChanged();
    void roleChanged();
    void textChanged();
    void iconSourceChanged();
    void iconNameChanged();
    void shortcutChanged();
    void fontChanged();

protected:
    void classBegin() override;
    void componentComplete() override;

    QQuickPlatformIconLoader *iconLoader() const;

private Q_SLOTS:
    void updateIcon();

private:
    bool m_complete;
    bool m_enabled;
    bool m_visible;
    bool m_separator;
    bool m_checkable;
    bool m_checked;
    QPlatformMenuItem::MenuRole m_role;
    QString m_text;
    QVariant m_shortcut;
    QFont m_font;
    QQuickPlatformMenu *m_menu;
    QQuickPlatformMenu *m_subMenu;
    QQuickPlatformMenuItemGroup *m_group;
    mutable QQuickPlatformIconLoader *m_iconLoader;
    QPlatformMenuItem *m_handle;

    friend class QQuickPlatformMenu;
    friend class QQuickPlatformMenuItemGroup;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPlatformMenuItem)

#endif // QQUICKPLATFORMMENUITEM_P_H
