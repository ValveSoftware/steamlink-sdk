/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include "quicktestevent_p.h"
#include <QtTest/qtestkeyboard.h>
#include <QtQml/qqml.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>

QT_BEGIN_NAMESPACE

QuickTestEvent::QuickTestEvent(QObject *parent)
    : QObject(parent)
{
}

QuickTestEvent::~QuickTestEvent()
{
}

bool QuickTestEvent::keyPress(int key, int modifiers, int delay)
{
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyPress(window, Qt::Key(key), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

bool QuickTestEvent::keyRelease(int key, int modifiers, int delay)
{
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyRelease(window, Qt::Key(key), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

bool QuickTestEvent::keyClick(int key, int modifiers, int delay)
{
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyClick(window, Qt::Key(key), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

bool QuickTestEvent::keyPressChar(const QString &character, int modifiers, int delay)
{
    QTEST_ASSERT(character.length() == 1);
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyPress(window, character[0].toLatin1(), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

bool QuickTestEvent::keyReleaseChar(const QString &character, int modifiers, int delay)
{
    QTEST_ASSERT(character.length() == 1);
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyRelease(window, character[0].toLatin1(), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

bool QuickTestEvent::keyClickChar(const QString &character, int modifiers, int delay)
{
    QTEST_ASSERT(character.length() == 1);
    QWindow *window = eventWindow();
    if (!window)
        return false;
    QTest::keyClick(window, character[0].toLatin1(), Qt::KeyboardModifiers(modifiers), delay);
    return true;
}

namespace QTest {
    extern int Q_TESTLIB_EXPORT defaultMouseDelay();
};

namespace QtQuickTest
{
    enum MouseAction { MousePress, MouseRelease, MouseClick, MouseDoubleClick, MouseMove };

    static void mouseEvent(MouseAction action, QWindow *window,
                           QObject *item, Qt::MouseButton button,
                           Qt::KeyboardModifiers stateKey, QPointF _pos, int delay=-1)
    {
        QTEST_ASSERT(window);
        QTEST_ASSERT(item);

        if (delay == -1 || delay < QTest::defaultMouseDelay())
            delay = QTest::defaultMouseDelay();
        if (delay > 0)
            QTest::qWait(delay);

        if (action == MouseClick) {
            mouseEvent(MousePress, window, item, button, stateKey, _pos);
            mouseEvent(MouseRelease, window, item, button, stateKey, _pos);
            return;
        }

        QPoint pos;
        QQuickItem *sgitem = qobject_cast<QQuickItem *>(item);
        if (sgitem)
            pos = sgitem->mapToScene(_pos).toPoint();
        QTEST_ASSERT(button == Qt::NoButton || button & Qt::MouseButtonMask);
        QTEST_ASSERT(stateKey == 0 || stateKey & Qt::KeyboardModifierMask);

        stateKey &= static_cast<unsigned int>(Qt::KeyboardModifierMask);

        QMouseEvent me(QEvent::User, QPoint(), Qt::LeftButton, button, stateKey);
        switch (action)
        {
            case MousePress:
                me = QMouseEvent(QEvent::MouseButtonPress, pos, window->mapToGlobal(pos), button, button, stateKey);
                break;
            case MouseRelease:
                me = QMouseEvent(QEvent::MouseButtonRelease, pos, window->mapToGlobal(pos), button, 0, stateKey);
                break;
            case MouseDoubleClick:
                me = QMouseEvent(QEvent::MouseButtonDblClick, pos, window->mapToGlobal(pos), button, button, stateKey);
                break;
            case MouseMove:
                // with move event the button is NoButton, but 'buttons' holds the currently pressed buttons
                me = QMouseEvent(QEvent::MouseMove, pos, window->mapToGlobal(pos), Qt::NoButton, button, stateKey);
                break;
            default:
                QTEST_ASSERT(false);
        }
        QSpontaneKeyEvent::setSpontaneous(&me);
        if (!qApp->notify(window, &me)) {
            static const char *mouseActionNames[] =
                { "MousePress", "MouseRelease", "MouseClick", "MouseDoubleClick", "MouseMove" };
            QString warning = QString::fromLatin1("Mouse event \"%1\" not accepted by receiving window");
            QWARN(warning.arg(QString::fromLatin1(mouseActionNames[static_cast<int>(action)])).toLatin1().data());
        }
    }

#ifndef QT_NO_WHEELEVENT
    static void mouseWheel(QWindow* window, QObject* item, Qt::MouseButtons buttons,
                                Qt::KeyboardModifiers stateKey,
                                QPointF _pos, int xDelta, int yDelta, int delay = -1)
    {
        QTEST_ASSERT(window);
        QTEST_ASSERT(item);
        if (delay == -1 || delay < QTest::defaultMouseDelay())
            delay = QTest::defaultMouseDelay();
        if (delay > 0)
            QTest::qWait(delay);

        QPoint pos;
        QQuickItem *sgitem = qobject_cast<QQuickItem *>(item);
        if (sgitem)
            pos = sgitem->mapToScene(_pos).toPoint();

        QTEST_ASSERT(buttons == Qt::NoButton || buttons & Qt::MouseButtonMask);
        QTEST_ASSERT(stateKey == 0 || stateKey & Qt::KeyboardModifierMask);

        stateKey &= static_cast<unsigned int>(Qt::KeyboardModifierMask);
        QWheelEvent we(pos, window->mapToGlobal(pos), QPoint(0, 0), QPoint(xDelta, yDelta), 0, Qt::Vertical, buttons, stateKey);

        QSpontaneKeyEvent::setSpontaneous(&we); // hmmmm
        if (!qApp->notify(window, &we))
            QTest::qWarn("Wheel event not accepted by receiving window");
    }
#endif
};

bool QuickTestEvent::mousePress
    (QObject *item, qreal x, qreal y, int button,
     int modifiers, int delay)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseEvent(QtQuickTest::MousePress, view, item,
                            Qt::MouseButton(button),
                            Qt::KeyboardModifiers(modifiers),
                            QPointF(x, y), delay);
    return true;
}

#ifndef QT_NO_WHEELEVENT
bool QuickTestEvent::mouseWheel(
    QObject *item, qreal x, qreal y, int buttons,
    int modifiers, int xDelta, int yDelta, int delay)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseWheel(view, item, Qt::MouseButtons(buttons),
                            Qt::KeyboardModifiers(modifiers),
                            QPointF(x, y), xDelta, yDelta, delay);
    return true;
}
#endif

bool QuickTestEvent::mouseRelease
    (QObject *item, qreal x, qreal y, int button,
     int modifiers, int delay)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseEvent(QtQuickTest::MouseRelease, view, item,
                            Qt::MouseButton(button),
                            Qt::KeyboardModifiers(modifiers),
                            QPointF(x, y), delay);
    return true;
}

bool QuickTestEvent::mouseClick
    (QObject *item, qreal x, qreal y, int button,
     int modifiers, int delay)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseEvent(QtQuickTest::MouseClick, view, item,
                            Qt::MouseButton(button),
                            Qt::KeyboardModifiers(modifiers),
                            QPointF(x, y), delay);
    return true;
}

bool QuickTestEvent::mouseDoubleClick
    (QObject *item, qreal x, qreal y, int button,
     int modifiers, int delay)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseEvent(QtQuickTest::MouseDoubleClick, view, item,
                            Qt::MouseButton(button),
                            Qt::KeyboardModifiers(modifiers),
                            QPointF(x, y), delay);
    return true;
}

bool QuickTestEvent::mouseMove
    (QObject *item, qreal x, qreal y, int delay, int buttons)
{
    QWindow *view = eventWindow();
    if (!view)
        return false;
    QtQuickTest::mouseEvent(QtQuickTest::MouseMove, view, item,
                            Qt::MouseButton(buttons), Qt::NoModifier,
                            QPointF(x, y), delay);
    return true;
}

QWindow *QuickTestEvent::eventWindow()
{
    QQuickItem *sgitem = qobject_cast<QQuickItem *>(parent());
    if (sgitem)
        return sgitem->window();
    return 0;
}

QT_END_NAMESPACE
