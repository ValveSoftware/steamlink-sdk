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

#include <QEventLoop>

#include "ResultThreader.h"

using namespace QPatternistSDK;

ResultThreader::ResultThreader(QEventLoop &ev,
                               QFile *file,
                               const Type t,
                               QObject *p) : QThread(p)
                                           , m_file(file)
                                           , m_type(t)
                                           , m_eventLoop(ev)
{
    Q_ASSERT_X(p,       Q_FUNC_INFO, "Should have a parent");
    Q_ASSERT_X(file,    Q_FUNC_INFO, "Should have a valid file");
    Q_ASSERT(m_type == Baseline || m_type == Result);
}

void ResultThreader::run()
{
    QXmlSimpleReader reader;
    reader.setContentHandler(this);

    QXmlInputSource source(m_file);
    reader.parse(source);
    m_file->close();
}

ResultThreader::Type ResultThreader::type() const
{
    return m_type;
}

// vim: et:ts=4:sw=4:sts=4
