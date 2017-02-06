/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickshortcut_p.h"

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qtquickglobal_p.h>
#include <QtGui/private/qguiapplication_p.h>

/*!
    \qmltype Shortcut
    \instantiates QQuickShortcut
    \inqmlmodule QtQuick
    \since 5.5
    \ingroup qtquick-input
    \brief Provides keyboard shortcuts

    The Shortcut type provides a way of handling keyboard shortcuts. The shortcut can
    be set to one of the \l{QKeySequence::StandardKey}{standard keyboard shortcuts},
    or it can be described with a string containing a sequence of up to four key
    presses that are needed to \l{Shortcut::activated}{activate} the shortcut.

    \qml
    Item {
        id: view

        property int currentIndex

        Shortcut {
            sequence: StandardKey.NextChild
            onActivated: view.currentIndex++
        }
    }
    \endqml

    \sa Keys
*/

/*! \qmlsignal QtQuick::Shortcut::activated()

    This signal is emitted when the shortcut is activated.

    The corresponding handler is \c onActivated.
*/

/*! \qmlsignal QtQuick::Shortcut::activatedAmbiguously()

    This signal is emitted when the shortcut is activated ambigously,
    meaning that it matches the start of more than one shortcut.

    The corresponding handler is \c onActivatedAmbiguously.
*/

static bool qQuickShortcutContextMatcher(QObject *obj, Qt::ShortcutContext context)
{
    switch (context) {
    case Qt::ApplicationShortcut:
        return true;
    case Qt::WindowShortcut:
        while (obj && !obj->isWindowType()) {
            obj = obj->parent();
            if (QQuickItem *item = qobject_cast<QQuickItem *>(obj))
                obj = item->window();
        }
        return obj && obj == QGuiApplication::focusWindow();
    default:
        return false;
    }
}

typedef bool (*ContextMatcher)(QObject *, Qt::ShortcutContext);

Q_GLOBAL_STATIC_WITH_ARGS(ContextMatcher, ctxMatcher, (qQuickShortcutContextMatcher))

Q_QUICK_PRIVATE_EXPORT ContextMatcher qt_quick_shortcut_context_matcher()
{
    return *ctxMatcher();
}

Q_QUICK_PRIVATE_EXPORT void qt_quick_set_shortcut_context_matcher(ContextMatcher matcher)
{
    *ctxMatcher() = matcher;
}

QT_BEGIN_NAMESPACE

QQuickShortcut::QQuickShortcut(QObject *parent) : QObject(parent), m_id(0),
    m_enabled(true), m_completed(false), m_autorepeat(true), m_context(Qt::WindowShortcut)
{
}

QQuickShortcut::~QQuickShortcut()
{
    ungrabShortcut();
}

/*!
    \qmlproperty keysequence QtQuick::Shortcut::sequence

    This property holds the shortcut's key sequence. The key sequence can be set
    to one of the \l{QKeySequence::StandardKey}{standard keyboard shortcuts}, or
    it can be described with a string containing a sequence of up to four key
    presses that are needed to \l{Shortcut::activated}{activate} the shortcut.

    The default value is an empty key sequence.

    \qml
    Shortcut {
        sequence: "Ctrl+E,Ctrl+W"
        onActivated: edit.wrapMode = TextEdit.Wrap
    }
    \endqml
*/
QVariant QQuickShortcut::sequence() const
{
    return m_sequence;
}

void QQuickShortcut::setSequence(const QVariant &sequence)
{
    if (sequence == m_sequence)
        return;

    QKeySequence shortcut;
    if (sequence.type() == QVariant::Int)
        shortcut = QKeySequence(static_cast<QKeySequence::StandardKey>(sequence.toInt()));
    else
        shortcut = QKeySequence::fromString(sequence.toString());

    grabShortcut(shortcut, m_context);

    m_sequence = sequence;
    m_shortcut = shortcut;
    emit sequenceChanged();
}

/*!
    \qmlproperty string QtQuick::Shortcut::nativeText
    \since 5.6

    This property provides the shortcut's key sequence as a platform specific
    string. This means that it will be shown translated, and on \macos it will
    resemble a key sequence from the menu bar. It is best to display this text
    to the user (for example, on a tooltip).

    \sa sequence, portableText
*/
QString QQuickShortcut::nativeText() const
{
    return m_shortcut.toString(QKeySequence::NativeText);
}

/*!
    \qmlproperty string QtQuick::Shortcut::portableText
    \since 5.6

    This property provides the shortcut's key sequence as a string in a
    "portable" format, suitable for reading and writing to a file. In many
    cases, it will look similar to the native text on Windows and X11.

    \sa sequence, nativeText
*/
QString QQuickShortcut::portableText() const
{
    return m_shortcut.toString(QKeySequence::PortableText);
}

/*!
    \qmlproperty bool QtQuick::Shortcut::enabled

    This property holds whether the shortcut is enabled.

    The default value is \c true.
*/
bool QQuickShortcut::isEnabled() const
{
    return m_enabled;
}

void QQuickShortcut::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    if (m_id)
        QGuiApplicationPrivate::instance()->shortcutMap.setShortcutEnabled(enabled, m_id, this);

    m_enabled = enabled;
    emit enabledChanged();
}

/*!
    \qmlproperty bool QtQuick::Shortcut::autoRepeat

    This property holds whether the shortcut can auto repeat.

    The default value is \c true.
*/
bool QQuickShortcut::autoRepeat() const
{
    return m_autorepeat;
}

void QQuickShortcut::setAutoRepeat(bool repeat)
{
    if (repeat == m_autorepeat)
        return;

    if (m_id)
        QGuiApplicationPrivate::instance()->shortcutMap.setShortcutAutoRepeat(repeat, m_id, this);

    m_autorepeat = repeat;
    emit autoRepeatChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Shortcut::context

    This property holds the \l{Qt::ShortcutContext}{shortcut context}.

    Supported values are:
    \list
    \li \c Qt.WindowShortcut (default) - The shortcut is active when its parent item is in an active top-level window.
    \li \c Qt.ApplicationShortcut - The shortcut is active when one of the application's windows are active.
    \endlist

    \qml
    Shortcut {
        sequence: StandardKey.Quit
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
    \endqml
*/
Qt::ShortcutContext QQuickShortcut::context() const
{
    return m_context;
}

void QQuickShortcut::setContext(Qt::ShortcutContext context)
{
    if (context == m_context)
        return;

    grabShortcut(m_shortcut, context);

    m_context = context;
    emit contextChanged();
}

void QQuickShortcut::classBegin()
{
}

void QQuickShortcut::componentComplete()
{
    m_completed = true;
    grabShortcut(m_shortcut, m_context);
}

bool QQuickShortcut::event(QEvent *event)
{
    if (m_enabled && event->type() == QEvent::Shortcut) {
        QShortcutEvent *se = static_cast<QShortcutEvent *>(event);
        if (se->shortcutId() == m_id && se->key() == m_shortcut){
            if (se->isAmbiguous())
                emit activatedAmbiguously();
            else
                emit activated();
            return true;
        }
    }
    return false;
}

void QQuickShortcut::grabShortcut(const QKeySequence &sequence, Qt::ShortcutContext context)
{
    ungrabShortcut();

    if (m_completed && !sequence.isEmpty()) {
        QGuiApplicationPrivate *pApp = QGuiApplicationPrivate::instance();
        m_id = pApp->shortcutMap.addShortcut(this, sequence, context, *ctxMatcher());
        if (!m_enabled)
            pApp->shortcutMap.setShortcutEnabled(false, m_id, this);
        if (!m_autorepeat)
            pApp->shortcutMap.setShortcutAutoRepeat(false, m_id, this);
    }
}

void QQuickShortcut::ungrabShortcut()
{
    if (m_id) {
        QGuiApplicationPrivate::instance()->shortcutMap.removeShortcut(m_id, this);
        m_id = 0;
    }
}

QT_END_NAMESPACE
