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

#ifndef QQUICKPLATFORMMENU_P_H
#define QQUICKPLATFORMMENU_P_H

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
#include <QtQml/qqmllist.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QIcon;
class QWindow;
class QQuickItem;
class QPlatformMenu;
class QQmlV4Function;
class QQuickPlatformMenuBar;
class QQuickPlatformMenuItem;
class QQuickPlatformIconLoader;
class QQuickPlatformSystemTrayIcon;

class QQuickPlatformMenu : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQmlListProperty<QObject> data READ data FINAL)
    Q_PROPERTY(QQmlListProperty<QQuickPlatformMenuItem> items READ items NOTIFY itemsChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenuBar *menuBar READ menuBar NOTIFY menuBarChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenu *parentMenu READ parentMenu NOTIFY parentMenuChanged FINAL)
    Q_PROPERTY(QQuickPlatformSystemTrayIcon *systemTrayIcon READ systemTrayIcon NOTIFY systemTrayIconChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenuItem *menuItem READ menuItem CONSTANT FINAL)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(int minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged FINAL)
    Q_PROPERTY(QPlatformMenu::MenuType type READ type WRITE setType NOTIFY typeChanged FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged FINAL)
    Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged FINAL)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged FINAL)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged FINAL)
    Q_ENUMS(QPlatformMenu::MenuType)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QQuickPlatformMenu(QObject *parent = nullptr);
    ~QQuickPlatformMenu();

    QPlatformMenu *handle() const;
    QPlatformMenu *create();
    void destroy();
    void sync();

    QQmlListProperty<QObject> data();
    QQmlListProperty<QQuickPlatformMenuItem> items();

    QQuickPlatformMenuBar *menuBar() const;
    void setMenuBar(QQuickPlatformMenuBar *menuBar);

    QQuickPlatformMenu *parentMenu() const;
    void setParentMenu(QQuickPlatformMenu *menu);

    QQuickPlatformSystemTrayIcon *systemTrayIcon() const;
    void setSystemTrayIcon(QQuickPlatformSystemTrayIcon *icon);

    QQuickPlatformMenuItem *menuItem() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isVisible() const;
    void setVisible(bool visible);

    int minimumWidth() const;
    void setMinimumWidth(int width);

    QPlatformMenu::MenuType type() const;
    void setType(QPlatformMenu::MenuType type);

    QString title() const;
    void setTitle(const QString &title);

    QUrl iconSource() const;
    void setIconSource(const QUrl &source);

    QString iconName() const;
    void setIconName(const QString &name);

    QFont font() const;
    void setFont(const QFont &font);

    Q_INVOKABLE void addItem(QQuickPlatformMenuItem *item);
    Q_INVOKABLE void insertItem(int index, QQuickPlatformMenuItem *item);
    Q_INVOKABLE void removeItem(QQuickPlatformMenuItem *item);

    Q_INVOKABLE void addMenu(QQuickPlatformMenu *menu);
    Q_INVOKABLE void insertMenu(int index, QQuickPlatformMenu *menu);
    Q_INVOKABLE void removeMenu(QQuickPlatformMenu *menu);

    Q_INVOKABLE void clear();

public Q_SLOTS:
    void open(QQmlV4Function *args);
    void close();

Q_SIGNALS:
    void aboutToShow();
    void aboutToHide();

    void itemsChanged();
    void menuBarChanged();
    void parentMenuChanged();
    void systemTrayIconChanged();
    void titleChanged();
    void iconSourceChanged();
    void iconNameChanged();
    void enabledChanged();
    void visibleChanged();
    void minimumWidthChanged();
    void fontChanged();
    void typeChanged();

protected:
    void classBegin() override;
    void componentComplete() override;

    QQuickPlatformIconLoader *iconLoader() const;

    QWindow *findWindow(QQuickItem *target, QPoint *offset) const;

    static void data_append(QQmlListProperty<QObject> *property, QObject *object);
    static int data_count(QQmlListProperty<QObject> *property);
    static QObject *data_at(QQmlListProperty<QObject> *property, int index);
    static void data_clear(QQmlListProperty<QObject> *property);

    static void items_append(QQmlListProperty<QQuickPlatformMenuItem> *property, QQuickPlatformMenuItem *item);
    static int items_count(QQmlListProperty<QQuickPlatformMenuItem> *property);
    static QQuickPlatformMenuItem *items_at(QQmlListProperty<QQuickPlatformMenuItem> *property, int index);
    static void items_clear(QQmlListProperty<QQuickPlatformMenuItem> *property);

private Q_SLOTS:
    void updateIcon();

private:
    bool m_complete;
    bool m_enabled;
    bool m_visible;
    int m_minimumWidth;
    QPlatformMenu::MenuType m_type;
    QString m_title;
    QFont m_font;
    QList<QObject *> m_data;
    QList<QQuickPlatformMenuItem *> m_items;
    QQuickPlatformMenuBar *m_menuBar;
    QQuickPlatformMenu *m_parentMenu;
    QQuickPlatformSystemTrayIcon *m_systemTrayIcon;
    mutable QQuickPlatformMenuItem *m_menuItem;
    mutable QQuickPlatformIconLoader *m_iconLoader;
    QPlatformMenu *m_handle;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPlatformMenu)

#endif // QQUICKPLATFORMMENU_P_H
