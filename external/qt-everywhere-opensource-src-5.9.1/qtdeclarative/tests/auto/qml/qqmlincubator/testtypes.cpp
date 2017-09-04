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
#include "testtypes.h"
#include <QtQml/qqml.h>

SelfRegisteringType *SelfRegisteringType::m_me = 0;
SelfRegisteringType::SelfRegisteringType()
: m_v(0)
{
    m_me = this;
}

SelfRegisteringType *SelfRegisteringType::me()
{
    return m_me;
}

void SelfRegisteringType::clearMe()
{
    m_me = 0;
}

SelfRegisteringOuterType *SelfRegisteringOuterType::m_me = 0;
bool SelfRegisteringOuterType::beenDeleted = false;
SelfRegisteringOuterType::SelfRegisteringOuterType()
: m_v(0)
{
    m_me = this;
    beenDeleted = false;
}

SelfRegisteringOuterType::~SelfRegisteringOuterType()
{
    beenDeleted = true;
}

SelfRegisteringOuterType *SelfRegisteringOuterType::me()
{
    return m_me;
}

CompletionRegisteringType *CompletionRegisteringType::m_me = 0;
CompletionRegisteringType::CompletionRegisteringType()
{
}

void CompletionRegisteringType::classBegin()
{
}

void CompletionRegisteringType::componentComplete()
{
    m_me = this;
}

CompletionRegisteringType *CompletionRegisteringType::me()
{
    return m_me;
}

void CompletionRegisteringType::clearMe()
{
    m_me = 0;
}

CallbackRegisteringType::callback CallbackRegisteringType::m_callback = 0;
void *CallbackRegisteringType::m_data = 0;
CallbackRegisteringType::CallbackRegisteringType()
: m_v(0)
{
}

void CallbackRegisteringType::clearCallback()
{
    m_callback = 0;
    m_data = 0;
}

void CallbackRegisteringType::registerCallback(callback c, void *d)
{
    m_callback = c;
    m_data = d;
}

CompletionCallbackType::callback CompletionCallbackType::m_callback = 0;
void *CompletionCallbackType::m_data = 0;
CompletionCallbackType::CompletionCallbackType()
{
}

void CompletionCallbackType::classBegin()
{
}

void CompletionCallbackType::componentComplete()
{
    if (m_callback) m_callback(this, m_data);
}

void CompletionCallbackType::clearCallback()
{
    m_callback = 0;
    m_data = 0;
}

void CompletionCallbackType::registerCallback(callback c, void *d)
{
    m_callback = c;
    m_data = d;
}

void registerTypes()
{
    qmlRegisterType<SelfRegisteringType>("Qt.test", 1,0, "SelfRegistering");
    qmlRegisterType<SelfRegisteringOuterType>("Qt.test", 1,0, "SelfRegisteringOuter");
    qmlRegisterType<CompletionRegisteringType>("Qt.test", 1,0, "CompletionRegistering");
    qmlRegisterType<CallbackRegisteringType>("Qt.test", 1,0, "CallbackRegistering");
    qmlRegisterType<CompletionCallbackType>("Qt.test", 1,0, "CompletionCallback");
}
