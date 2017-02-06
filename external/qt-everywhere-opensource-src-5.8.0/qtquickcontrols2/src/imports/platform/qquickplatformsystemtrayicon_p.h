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

#ifndef QQUICKPLATFORMSYSTEMTRAYICON_P_H
#define QQUICKPLATFORMSYSTEMTRAYICON_P_H

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

#include <QtCore/qurl.h>
#include <QtGui/qpa/qplatformsystemtrayicon.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickPlatformMenu;
class QQuickPlatformIconLoader;

class QQuickPlatformSystemTrayIcon : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(bool available READ isAvailable CONSTANT FINAL)
    Q_PROPERTY(bool supportsMessages READ supportsMessages CONSTANT FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged FINAL)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged FINAL)
    Q_PROPERTY(QString tooltip READ tooltip WRITE setTooltip NOTIFY tooltipChanged FINAL)
    Q_PROPERTY(QQuickPlatformMenu *menu READ menu WRITE setMenu NOTIFY menuChanged FINAL)
    Q_ENUMS(QPlatformSystemTrayIcon::ActivationReason QPlatformSystemTrayIcon::MessageIcon)

public:
    explicit QQuickPlatformSystemTrayIcon(QObject *parent = nullptr);
    ~QQuickPlatformSystemTrayIcon();

    QPlatformSystemTrayIcon *handle() const;

    bool isAvailable() const;
    bool supportsMessages() const;

    bool isVisible() const;
    void setVisible(bool visible);

    QUrl iconSource() const;
    void setIconSource(const QUrl &source);

    QString iconName() const;
    void setIconName(const QString &name);

    QString tooltip() const;
    void setTooltip(const QString &tooltip);

    QQuickPlatformMenu *menu() const;
    void setMenu(QQuickPlatformMenu *menu);

public Q_SLOTS:
    void show();
    void hide();

    void showMessage(const QString &title, const QString &message,
                     QPlatformSystemTrayIcon::MessageIcon iconType = QPlatformSystemTrayIcon::Information, int msecs = 10000);

Q_SIGNALS:
    void activated(QPlatformSystemTrayIcon::ActivationReason reason);
    void messageClicked();
    void visibleChanged();
    void iconSourceChanged();
    void iconNameChanged();
    void tooltipChanged();
    void menuChanged();

protected:
    void init();
    void cleanup();

    void classBegin() override;
    void componentComplete() override;

    QQuickPlatformIconLoader *iconLoader() const;

private Q_SLOTS:
    void updateIcon();

private:
    bool m_complete;
    bool m_visible;
    QString m_tooltip;
    QQuickPlatformMenu *m_menu;
    mutable QQuickPlatformIconLoader *m_iconLoader;
    QPlatformSystemTrayIcon *m_handle;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPlatformSystemTrayIcon)

#endif // QQUICKPLATFORMSYSTEMTRAYICON_P_H
