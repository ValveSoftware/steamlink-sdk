/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qpa/qplatforminputcontext.h>
#include <QtGui/QInputMethod>

class PlatformInputContext : public QPlatformInputContext
{
public:
    PlatformInputContext()
        : m_visible(false), m_action(QInputMethod::Click), m_cursorPosition(0),
          m_invokeActionCallCount(0), m_showInputPanelCallCount(0), m_hideInputPanelCallCount(0),
          m_updateCallCount(0), m_direction(Qt::LeftToRight)
    {
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
