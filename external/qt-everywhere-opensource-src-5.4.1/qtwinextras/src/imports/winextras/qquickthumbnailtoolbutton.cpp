/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickthumbnailtoolbutton_p.h"
#include "qquickiconloader_p.h"

#include <QWinThumbnailToolButton>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ThumbnailToolButton
    \instantiates QQuickThumbnailToolButton
    \inqmlmodule QtWinExtras

    \brief Represents a button in a thumbnail toolbar.

    The ThumbnailToolButton represents a button in a thumbnail toolbar.
    \since QtWinExtras 1.0
 */

/*!
    \qmlsignal ThumbnailToolButton::clicked()
    This signal is emitted when the user clicks the button. The corresponding handler is \c onClicked.
*/

QQuickThumbnailToolButton::QQuickThumbnailToolButton(QObject *parent) :
    QObject(parent), m_button(new QWinThumbnailToolButton(this))
{
    connect(m_button, SIGNAL(clicked()), SIGNAL(clicked()));
}

QQuickThumbnailToolButton::~QQuickThumbnailToolButton()
{
}

/*!
    \qmlproperty url ThumbnailToolButton::iconSource

    The button icon path.
 */
void QQuickThumbnailToolButton::setIconSource(const QUrl &iconSource)
{
    if (m_iconSource == iconSource)
        return;

    if (iconSource.isEmpty()) {
        m_button->setIcon(QIcon());
        m_iconSource = iconSource;
        emit iconSourceChanged();
    }

    if (QQuickIconLoader::load(iconSource, qmlEngine(this), QVariant::Icon, QSize(),
                               this, &QQuickThumbnailToolButton::iconLoaded) != QQuickIconLoader::LoadError) {
        m_iconSource = iconSource;
        emit iconSourceChanged();
    }
}

QUrl QQuickThumbnailToolButton::iconSource()
{
    return m_iconSource;
}

/*!
    \qmlproperty string ThumbnailToolButton::tooltip

    The tooltip of the button.
 */
void QQuickThumbnailToolButton::setTooltip(const QString &tooltip)
{
    if (m_button->toolTip() != tooltip) {
        m_button->setToolTip(tooltip);
        emit tooltipChanged();
    }
}

QString QQuickThumbnailToolButton::tooltip() const
{
    return m_button->toolTip();
}

/*!
    \qmlproperty bool ThumbnailToolButton::enabled

    This property holds whether the button is enabled.

    By default, this property is set to true.
 */
void QQuickThumbnailToolButton::setEnabled(bool enabled)
{
    if (m_button->isEnabled() != enabled) {
        m_button->setEnabled(enabled);
        emit enabledChanged();
    }
}

bool QQuickThumbnailToolButton::isEnabled() const
{
    return m_button->isEnabled();
}

/*!
    \qmlproperty bool ThumbnailToolButton::interactive

    This property holds whether the button is interactive.
    If not interactive, the button remains enabled, but no pressed or mouse-over
    states are drawn. Set this property to false to use this button as a
    notification icon.

    By default, this property is set to true.
 */
void QQuickThumbnailToolButton::setInteractive(bool interactive)
{
    if (m_button->isInteractive() != interactive) {
        m_button->setInteractive(interactive);
        emit interactiveChanged();
    }
}

bool QQuickThumbnailToolButton::isInteractive() const
{
    return m_button->isInteractive();
}

/*!
    \qmlproperty bool ThumbnailToolButton::visible

    This property holds whether the button is visible.

    By default, this property is set to true.
 */
void QQuickThumbnailToolButton::setVisible(bool visible)
{
    if (m_button->isVisible() != visible) {
        m_button->setVisible(visible);
        emit visibleChanged();
    }
}

bool QQuickThumbnailToolButton::isVisible() const
{
    return m_button->isVisible();
}

/*!
    \qmlproperty bool ThumbnailToolButton::dismissOnClick

    This property holds whether the window thumbnail is dismissed after a button click.

    By default, this property is set to false.
 */
void QQuickThumbnailToolButton::setDismissOnClick(bool dismiss)
{
    if (m_button->dismissOnClick() != dismiss) {
        m_button->setDismissOnClick(dismiss);
        emit dismissOnClickChanged();
    }
}

bool QQuickThumbnailToolButton::dismissOnClick() const
{
    return m_button->dismissOnClick();
}

/*!
    \qmlproperty bool ThumbnailToolButton::flat

    This property holds whether the button background and frame are not drawn.

    By default, this property is set to false.
 */
void QQuickThumbnailToolButton::setFlat(bool flat)
{
    if (m_button->isFlat() != flat) {
        m_button->setFlat(flat);
        emit flatChanged();
    }
}

bool QQuickThumbnailToolButton::isFlat() const
{
    return m_button->isFlat();
}

void QQuickThumbnailToolButton::iconLoaded(const QVariant &value)
{
    if (value.isValid())
        m_button->setIcon(value.value<QIcon>());
}

QT_END_NAMESPACE
