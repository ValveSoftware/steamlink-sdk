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

#ifndef VERSIONS_H
#define VERSIONS_H

#include <QQuickItem>

class Versions : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo WRITE setFoo NOTIFY fooChanged)
    Q_PROPERTY(int bar READ bar WRITE setBar NOTIFY barChanged REVISION 1)
    Q_PROPERTY(int baz READ baz WRITE setBaz NOTIFY bazChanged REVISION 2)

public:
    Versions(QQuickItem *parent = 0);
    ~Versions();
    int foo() const { return m_foo; }
    void setFoo(int value) { m_foo = value; }
    int bar() const { return m_bar; }
    void setBar(int value) { m_bar = value; }
    int baz() const { return m_baz; }
    void setBaz(int value) { m_baz = value; }
signals:
    void fooChanged();
    void barChanged();
    void bazChanged();
private:
    int m_foo;
    int m_bar;
    int m_baz;
};

#endif // VERSIONS_H

