/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquickdelaybutton_p.h"
#include "qquickabstractbutton_p_p.h"

#include <QtQuick/private/qquickanimation_p.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQuick/private/qquicktransitionmanager_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype DelayButton
    \inherits AbstractButton
    \instantiates QQuickDelayButton
    \inqmlmodule QtQuick.Controls
    \since 5.9
    \ingroup qtquickcontrols2-buttons
    \brief Check button that triggers when held down long enough.

    \image qtquickcontrols2-delaybutton.gif

    DelayButton is a checkable button that incorporates a delay before the
    button becomes \l {AbstractButton::}{checked} and the \l activated()
    signal is emitted. This delay prevents accidental presses.

    The current progress is expressed as a decimal value between \c 0.0
    and \c 1.0. The time it takes for \l activated() to be emitted is
    measured in milliseconds, and can be set with the \l delay property.

    The progress is indicated by a progress indicator on the button.

    \sa {Customizing DelayButton}, {Button Controls}
*/

/*!
    \qmlsignal QtQuick.Controls::DelayButton::activated()

    This signal is emitted when \l progress reaches \c 1.0.
*/

class QQuickDelayTransitionManager;

class QQuickDelayButtonPrivate : public QQuickAbstractButtonPrivate
{
    Q_DECLARE_PUBLIC(QQuickDelayButton)

public:
    QQuickDelayButtonPrivate();

    void beginTransition(qreal to);
    void finishTransition();
    void cancelTransition();

    int delay;
    qreal progress;
    QQuickTransition *transition;
    QScopedPointer<QQuickDelayTransitionManager> transitionManager;
};

class QQuickDelayTransitionManager : public QQuickTransitionManager
{
public:
    QQuickDelayTransitionManager(QQuickDelayButton *button) : m_button(button) { }

    void transition(QQuickTransition *transition, qreal progress);

protected:
    void finished() override;

private:
    QQuickDelayButton *m_button;
};

void QQuickDelayTransitionManager::transition(QQuickTransition *transition, qreal progress)
{
    qmlExecuteDeferred(transition);

    QQmlProperty defaultTarget(m_button, QLatin1String("progress"));
    QQmlListProperty<QQuickAbstractAnimation> animations = transition->animations();
    const int count = animations.count(&animations);
    for (int i = 0; i < count; ++i) {
        QQuickAbstractAnimation *anim = animations.at(&animations, i);
        anim->setDefaultTarget(defaultTarget);
    }

    QList<QQuickStateAction> actions;
    actions << QQuickStateAction(m_button, QLatin1String("progress"), progress);
    QQuickTransitionManager::transition(actions, transition, m_button);
}

void QQuickDelayTransitionManager::finished()
{
    if (qFuzzyCompare(m_button->progress(), 1.0))
        emit m_button->activated();
}

QQuickDelayButtonPrivate::QQuickDelayButtonPrivate()
    : delay(3000),
      progress(0.0),
      transition(nullptr)
{
}

void QQuickDelayButtonPrivate::beginTransition(qreal to)
{
    Q_Q(QQuickDelayButton);
    if (!transition) {
        q->setProgress(to);
        finishTransition();
        return;
    }

    if (!transitionManager)
        transitionManager.reset(new QQuickDelayTransitionManager(q));

    transitionManager->transition(transition, to);
}

void QQuickDelayButtonPrivate::finishTransition()
{
    Q_Q(QQuickDelayButton);
    if (qFuzzyCompare(progress, 1.0))
        emit q->activated();
}

void QQuickDelayButtonPrivate::cancelTransition()
{
    if (transitionManager)
        transitionManager->cancel();
}

QQuickDelayButton::QQuickDelayButton(QQuickItem *parent)
    : QQuickAbstractButton(*(new QQuickDelayButtonPrivate), parent)
{
    setCheckable(true);
}

/*!
    \qmlproperty int QtQuick.Controls::DelayButton::delay

    This property holds the time it takes (in milliseconds) for \l progress
    to reach \c 1.0 and emit \l activated().

    The default value is \c 3000 ms.
*/
int QQuickDelayButton::delay() const
{
    Q_D(const QQuickDelayButton);
    return d->delay;
}

void QQuickDelayButton::setDelay(int delay)
{
    Q_D(QQuickDelayButton);
    if (d->delay == delay)
        return;

    d->delay = delay;
    emit delayChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::DelayButton::progress
    \readonly

    This property holds the current progress as displayed by the progress
    indicator, in the range \c 0.0 - \c 1.0.
*/
qreal QQuickDelayButton::progress() const
{
    Q_D(const QQuickDelayButton);
    return d->progress;
}

void QQuickDelayButton::setProgress(qreal progress)
{
    Q_D(QQuickDelayButton);
    if (qFuzzyCompare(d->progress, progress))
        return;

    d->progress = progress;
    emit progressChanged();
}

/*!
    \qmlproperty Transition QtQuick.Controls::DelayButton::transition

    This property holds the transition that is applied on the \l progress
    property when the button is pressed or released.
*/
QQuickTransition *QQuickDelayButton::transition() const
{
    Q_D(const QQuickDelayButton);
    return d->transition;
}

void QQuickDelayButton::setTransition(QQuickTransition *transition)
{
    Q_D(QQuickDelayButton);
    if (d->transition == transition)
        return;

    d->transition = transition;
    emit transitionChanged();
}

void QQuickDelayButton::buttonChange(ButtonChange change)
{
    Q_D(QQuickDelayButton);
    switch (change) {
    case ButtonCheckedChange:
        d->cancelTransition();
        setProgress(d->checked ? 1.0 : 0.0);
        break;
    case ButtonPressedChanged:
        if (!d->checked)
            d->beginTransition(d->pressed ? 1.0 : 0.0);
        break;
    default:
        QQuickAbstractButton::buttonChange(change);
        break;
    }
}

void QQuickDelayButton::nextCheckState()
{
    Q_D(QQuickDelayButton);
    setChecked(!d->checked && qFuzzyCompare(d->progress, 1.0));
}

QFont QQuickDelayButton::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::PushButtonFont);
}

QT_END_NAMESPACE
