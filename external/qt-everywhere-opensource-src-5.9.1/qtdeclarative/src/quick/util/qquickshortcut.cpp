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

    It is also possible to set multiple shortcut \l sequences, so that the shortcut
    can be \l activated via several different sequences of key presses.

    \sa Keys, {Keys::}{shortcutOverride()}
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

static QKeySequence valueToKeySequence(const QVariant &value)
{
    if (value.type() == QVariant::Int)
        return QKeySequence(static_cast<QKeySequence::StandardKey>(value.toInt()));
    return QKeySequence::fromString(value.toString());
}

QQuickShortcut::QQuickShortcut(QObject *parent) : QObject(parent),
    m_enabled(true), m_completed(false), m_autorepeat(true), m_context(Qt::WindowShortcut)
{
}

QQuickShortcut::~QQuickShortcut()
{
    ungrabShortcut(m_shortcut);
    for (Shortcut &shortcut : m_shortcuts)
        ungrabShortcut(shortcut);
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

    \sa sequences
*/
QVariant QQuickShortcut::sequence() const
{
    return m_shortcut.userValue;
}

void QQuickShortcut::setSequence(const QVariant &value)
{
    if (value == m_shortcut.userValue)
        return;

    QKeySequence keySequence = valueToKeySequence(value);

    ungrabShortcut(m_shortcut);
    m_shortcut.userValue = value;
    m_shortcut.keySequence = keySequence;
    grabShortcut(m_shortcut, m_context);
    emit sequenceChanged();
}

/*!
    \qmlproperty list<keysequence> QtQuick::Shortcut::sequences
    \since 5.9

    This property holds multiple key sequences for the shortcut. The key sequences
    can be set to one of the \l{QKeySequence::StandardKey}{standard keyboard shortcuts},
    or they can be described with strings containing sequences of up to four key
    presses that are needed to \l{Shortcut::activated}{activate} the shortcut.

    \qml
    Shortcut {
        sequences: [StandardKey.Cut, "Ctrl+X", "Shift+Del"]
        onActivated: edit.cut()
    }
    \endqml
*/
QVariantList QQuickShortcut::sequences() const
{
    QVariantList values;
    for (const Shortcut &shortcut : m_shortcuts)
        values += shortcut.userValue;
    return values;
}

void QQuickShortcut::setSequences(const QVariantList &values)
{
    QVector<Shortcut> remainder = m_shortcuts.mid(values.count());
    m_shortcuts.resize(values.count());

    bool changed = !remainder.isEmpty();
    for (int i = 0; i < values.count(); ++i) {
        QVariant value = values.at(i);
        Shortcut& shortcut = m_shortcuts[i];
        if (value == shortcut.userValue)
            continue;

        QKeySequence keySequence = valueToKeySequence(value);

        ungrabShortcut(shortcut);
        shortcut.userValue = value;
        shortcut.keySequence = keySequence;
        grabShortcut(shortcut, m_context);

        changed = true;
    }

    if (changed)
        emit sequencesChanged();
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
    return m_shortcut.keySequence.toString(QKeySequence::NativeText);
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
    return m_shortcut.keySequence.toString(QKeySequence::PortableText);
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

    setEnabled(m_shortcut, enabled);
    for (Shortcut &shortcut : m_shortcuts)
        setEnabled(shortcut, enabled);

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

    setAutoRepeat(m_shortcut, repeat);
    for (Shortcut &shortcut : m_shortcuts)
        setAutoRepeat(shortcut, repeat);

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

    ungrabShortcut(m_shortcut);
    m_context = context;
    grabShortcut(m_shortcut, context);
    emit contextChanged();
}

void QQuickShortcut::classBegin()
{
}

void QQuickShortcut::componentComplete()
{
    m_completed = true;
    grabShortcut(m_shortcut, m_context);
    for (Shortcut &shortcut : m_shortcuts)
        grabShortcut(shortcut, m_context);
}

bool QQuickShortcut::event(QEvent *event)
{
    if (m_enabled && event->type() == QEvent::Shortcut) {
        QShortcutEvent *se = static_cast<QShortcutEvent *>(event);
        bool match = m_shortcut.matches(se);
        int i = 0;
        while (!match && i < m_shortcuts.count())
            match |= m_shortcuts.at(i++).matches(se);
        if (match) {
            if (se->isAmbiguous())
                emit activatedAmbiguously();
            else
                emit activated();
            return true;
        }
    }
    return false;
}

bool QQuickShortcut::Shortcut::matches(QShortcutEvent *event) const
{
    return event->shortcutId() == id && event->key() == keySequence;
}

void QQuickShortcut::setEnabled(QQuickShortcut::Shortcut &shortcut, bool enabled)
{
    if (shortcut.id)
        QGuiApplicationPrivate::instance()->shortcutMap.setShortcutEnabled(enabled, shortcut.id, this);
}

void QQuickShortcut::setAutoRepeat(QQuickShortcut::Shortcut &shortcut, bool repeat)
{
    if (shortcut.id)
        QGuiApplicationPrivate::instance()->shortcutMap.setShortcutAutoRepeat(repeat, shortcut.id, this);
}

void QQuickShortcut::grabShortcut(Shortcut &shortcut, Qt::ShortcutContext context)
{
    if (m_completed && !shortcut.keySequence.isEmpty()) {
        QGuiApplicationPrivate *pApp = QGuiApplicationPrivate::instance();
        shortcut.id = pApp->shortcutMap.addShortcut(this, shortcut.keySequence, context, *ctxMatcher());
        if (!m_enabled)
            pApp->shortcutMap.setShortcutEnabled(false, shortcut.id, this);
        if (!m_autorepeat)
            pApp->shortcutMap.setShortcutAutoRepeat(false, shortcut.id, this);
    }
}

void QQuickShortcut::ungrabShortcut(Shortcut &shortcut)
{
    if (shortcut.id) {
        QGuiApplicationPrivate::instance()->shortcutMap.removeShortcut(shortcut.id, this);
        shortcut.id = 0;
    }
}

QT_END_NAMESPACE

#include "moc_qquickshortcut_p.cpp"
