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

#include <QtCore>

class Class : public QObject
{
    Q_OBJECT

    class SubClass
    {
        void f()
        {
            tr("nested class context");
        }
    };

    void f()
    {
        tr("just class context");
    }
};

namespace Outer {

class Class : public QObject { Q_OBJECT };

namespace Middle1 {

class Class : public QObject { Q_OBJECT };

namespace Inner1 {

class Class : public QObject { Q_OBJECT };

}

namespace I = Inner1;

class Something;
class Different;

}

namespace Middle2 {

class Class : public QObject { Q_OBJECT };

namespace Inner2 {

class Class : public QObject { Q_OBJECT };

namespace IO = Middle2;

}

namespace I = Inner2;

}

namespace MI = Middle1::Inner1;

namespace O = ::Outer;

class Middle1::Different : QObject {
Q_OBJECT
    void f() {
        tr("different namespaced class def");
    }
};

}

namespace O = Outer;
namespace OM = Outer::Middle1;
namespace OMI = Outer::Middle1::I;

int main()
{
    Class::tr("outestmost class");
    Outer::Class::tr("outer class");
    Outer::MI::Class::tr("innermost one");
    OMI::Class::tr("innermost two");
    O::Middle1::I::Class::tr("innermost three");
    O::Middle2::I::Class::tr("innermost three b");
    OM::I::Class::tr("innermost four");
    return 0;
}

class OM::Something : QObject {
Q_OBJECT
    void f() {
        tr("namespaced class def");
    }
    void g() {
        tr("namespaced class def 2");
    }
};

// QTBUG-8360
namespace A {

void foo()
{
    using namespace A;
}

void goo()
{
    return QObject::tr("Bla");
}

}


namespace AA {
namespace B {

using namespace AA;

namespace C {

class Test : public QObject {
    Q_OBJECT
};

}

}

using namespace B;
using namespace C;

void goo()
{
    AA::Test::tr("howdy?");
}

}


namespace A1 {
namespace B {

class Test : public QObject {
    Q_OBJECT
};

using namespace A1;

void foo()
{
    B::B::B::Test::tr("yeeee-ha!");
}

}
}


#include "main.moc"
