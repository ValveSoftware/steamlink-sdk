/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "settings.h"
#include <QtCore/private/qobject_p.h>

namespace QtVirtualKeyboard {

class SettingsPrivate : public QObjectPrivate
{
public:
    SettingsPrivate() :
        QObjectPrivate(),
        style(),
        styleName(),
        locale(),
        availableLocales(),
        activeLocales()
    {}

    QString style;
    QString styleName;
    QString locale;
    QStringList availableLocales;
    QStringList activeLocales;
};

static QScopedPointer<Settings> s_settingsInstance;

/*!
    \class QtVirtualKeyboard::Settings
    \internal
*/

Settings::Settings(QObject *parent) :
    QObject(*new SettingsPrivate(), parent)
{
}

Settings *Settings::instance()
{
    if (!s_settingsInstance)
        s_settingsInstance.reset(new Settings());
    return s_settingsInstance.data();
}

QString Settings::style() const
{
    Q_D(const Settings);
    return d->style;
}

void Settings::setStyle(const QString &style)
{
    Q_D(Settings);
    if (d->style != style) {
        d->style = style;
        emit styleChanged();
    }
}

QString Settings::styleName() const
{
    Q_D(const Settings);
    return d->styleName;
}

void Settings::setStyleName(const QString &styleName)
{
    Q_D(Settings);
    if (d->styleName != styleName) {
        d->styleName = styleName;
        emit styleNameChanged();
    }
}

QString Settings::locale() const
{
    Q_D(const Settings);
    return d->locale;
}

void Settings::setLocale(const QString &locale)
{
    Q_D(Settings);
    if (d->locale != locale) {
        d->locale = locale;
        emit localeChanged();
    }
}

QStringList Settings::availableLocales() const
{
    Q_D(const Settings);
    return d->availableLocales;
}

void Settings::setAvailableLocales(const QStringList &availableLocales)
{
    Q_D(Settings);
    if (d->availableLocales != availableLocales) {
        d->availableLocales = availableLocales;
        emit availableLocalesChanged();
    }
}

QStringList Settings::activeLocales() const
{
    Q_D(const Settings);
    return d->activeLocales;
}

void Settings::setActiveLocales(const QStringList &activeLocales)
{
    Q_D(Settings);
    if (d->activeLocales != activeLocales) {
        d->activeLocales = activeLocales;
        emit activeLocalesChanged();
    }
}

} // namespace QtVirtualKeyboard
