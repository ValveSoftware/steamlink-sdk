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

#include <qpa/qplatforminputcontext.h>
#include <QtGui/QInputMethod>

#include <private/qinputmethod_p.h>

class PlatformInputContext : public QPlatformInputContext
{
public:
    PlatformInputContext()
        : m_visible(false), m_action(QInputMethod::Action(-1)), m_cursorPosition(0),
          m_invokeActionCallCount(0), m_showInputPanelCallCount(0), m_hideInputPanelCallCount(0),
          m_updateCallCount(0), m_direction(Qt::LeftToRight)
    {
        QInputMethodPrivate::get(qApp->inputMethod())->testContext = this;
    }

    ~PlatformInputContext()
    {
        QInputMethodPrivate::get(qApp->inputMethod())->testContext = 0;
    }

    virtual void showInputPanel()
    {
        m_visible = true;
        m_showInputPanelCallCount++;
    }
    virtual void hideInputPanel()
    {
        m_visible = false;
        m_hideInputPanelCallCount++;
    }
    virtual bool isInputPanelVisible() const
    {
        return m_visible;
    }
    virtual void invokeAction(QInputMethod::Action action, int cursorPosition)
    {
        m_invokeActionCallCount++;
        m_action = action;
        m_cursorPosition = cursorPosition;
    }
    virtual void update(Qt::InputMethodQueries)
    {
        m_updateCallCount++;
    }

    virtual QLocale locale() const
    {
        if (m_direction == Qt::RightToLeft)
            return QLocale(QLocale::Arabic);
        else
            return QLocale(QLocale::English);
    }

    virtual Qt::LayoutDirection inputDirection() const
    {
        return m_direction;
    }

    void setInputDirection(Qt::LayoutDirection direction) {
        m_direction = direction;
        emitLocaleChanged();
        emitInputDirectionChanged(inputDirection());
    }

    void clear() {
        m_cursorPosition = 0;
        m_invokeActionCallCount = 0;
        m_visible = false;
        m_showInputPanelCallCount = 0;
        m_hideInputPanelCallCount = 0;
        m_updateCallCount = 0;
    }

    bool m_visible;
    QInputMethod::Action m_action;
    int m_cursorPosition;
    int m_invokeActionCallCount;
    int m_showInputPanelCallCount;
    int m_hideInputPanelCallCount;
    int m_updateCallCount;
    Qt::LayoutDirection m_direction;
};
