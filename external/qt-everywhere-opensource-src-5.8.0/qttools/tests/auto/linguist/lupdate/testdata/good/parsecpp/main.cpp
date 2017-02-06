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

// IMPORTANT!!!! If you want to add testdata to this file,
// always add it to the end in order to not change the linenumbers of translations!!!
int main(int argc, char **argv)
{
    Size size = QSize(1,1);
}

QString qt_detectRTLLanguage()
{
     return QApplication::tr("QT_LAYOUT_DIRECTION",
                         "Translate this string to the string 'LTR' in left-to-right"
                         " languages or to 'RTL' in right-to-left languages (such as Hebrew"
                         " and Arabic) to get proper widget layout.") == QLatin1String("RTL");
}


class Dialog2 : public QDialog
{
    Q_OBJECT
    void func();
    void func3();
    int getCount() const { return 2; }

};

void Dialog2::func()
{
    int n = getCount();
    tr("%n files", "plural form", n);
    tr("%n cars", 0, n);
    tr("&Find %n cars", 0, n);
    tr("Search in %n items?", 0, n);
    tr("%1. Search in %n items?", 0, n);
    tr("Age: %1");
    tr("There are %n house(s)", "Plurals and function call", getCount());




    QCoreApplication::translate("Plurals, QCoreApplication", "%n house(s)", "Plurals and identifier", QCoreApplication::UnicodeUTF8, n);
    QCoreApplication::translate("Plurals, QCoreApplication", "%n car(s)", "Plurals and literal number", QCoreApplication::UnicodeUTF8, 1);
    QCoreApplication::translate("Plurals, QCoreApplication", "%n horse(s)", "Plurals and function call", QCoreApplication::UnicodeUTF8, getCount());








    QTranslator trans;
    trans.translate("QTranslator", "Simple");
    trans.translate("QTranslator", "Simple", 0);
    trans.translate("QTranslator", "Simple with comment", "with comment");
    trans.translate("QTranslator", "Plural without comment", 0, 1);
    trans.translate("QTranslator", "Plural with comment", "comment 1", n);
    trans.translate("QTranslator", "Plural with comment", "comment 2", getCount());












}




/* This is actually a test of how many alternative ways a struct/class can be found in a source file.
 * Due to the simple parser in lupdate, it will actually not treat the remaining lines in the define
 * as a macro, which is a case the 'Tok_Class' parser block might not consider, and it might loop infinite
 * if it just tries to fetch the next token until it gets a '{' or a ';'. Another pitfall is that the
 * context of tr("func3") might not be parsed, it won't resume normal evaluation until the '{' after the function
 * signature.
 *
 */
typedef struct S_
{
int a;
} S, *SPtr;
class ForwardDecl;


#define FT_DEFINE_SERVICE( name )            \
  typedef struct FT_Service_ ## name ## Rec_ \
    FT_Service_ ## name ## Rec ;             \
  typedef struct FT_Service_ ## name ## Rec_ \
    const * FT_Service_ ## name ;            \
  struct FT_Service_ ## name ## Rec_




void Dialog2::func3()
{
    tr("func3");
}




namespace Gui { class BaseClass  {}; }


class TestClass : QObject {
    Q_OBJECT


    inline QString inlineFunc1() {
        return tr("inline function", "TestClass");
    }

    QString inlineFunc2() {
        return tr("inline function 2", "TestClass");
    }

    static inline QString staticInlineFunc() {
        return tr("static inline function", "TestClass");
    }

    class NoQObject : public Gui::BaseClass {
        public:
        inline QString hello() { return QString("hello"); }

    };

};


class Testing : QObject {
    Q_OBJECT

    inline QString f1() {
        //: this is an extra comment for the translator
        return tr("extra-commented string");
        return tr("not extra-commented string");
        /*: another extra-comment */
        return tr("another extra-commented string");
        /*: blah! */
        return QApplication::translate("scope", "works in translate, too", "blabb", 0);
    }

};

//: extra comment for NOOP
//: which spans multiple lines
QT_TRANSLATE_NOOP("scope", "string") /*: complain & ignore */; // 4.4 says the line of this is at the next statement
//: extra comment for NOOP3
QT_TRANSLATE_NOOP3_UTF8("scope", "string", "comment"); // 4.4 doesn't see this

QT_TRANSLATE_NOOP("scope", "string " // this is an interleaved comment
                  "continuation on next line");


class TestingTake17 : QObject {
    Q_OBJECT

    int function(void)
    {
        //: random comment
        //= this_is_an_id
        //~ loc-layout_id fooish_bar
        //~ po-ignore_me totally foo-barred  nonsense
        tr("something cool");

        tr("less cool");

        //= another_id
        tr("even more cool");
    }
};




//: again an extra comment, this time for id-based NOOP
//% "This is supposed\tto be quoted \" newline\n"
//% "backslashed \\ stuff."
QT_TRID_NOOP("this_a_id");

//~ some thing
//% "This needs to be here. Really."
QString test = qtTrId("this_another_id", n);



class YetAnotherTest : QObject {
    Q_OBJECT

    int function(void)
    {
        //
        //:
        //=
        //~
        //#
        //=============
        //~~~~~~~~~~~~~
        //:::::::::::::
        tr("nothing");
    }
};



//: This is a message without a source string
QString test = qtTrId("yet_another_id");



// QTBUG-9276: context in static initializers
class Bogus : QObject {
    Q_OBJECT

    static const char * const s_strings[];
};

const char * const Bogus::s_strings[] = {
    QT_TR_NOOP("this should be in Bogus")
};

const char * const Bogus::s_strings[SIZE] = {
    QT_TR_NOOP("this should be in Bogus")
};

void bogosity()
{
    // no spaces here. test collateral damage from ignoring equal sign
    Class::member=QObject::tr("just QObject");
}



namespace Internal {

class Message : public QObject
{
    Q_OBJECT
public:
    Message(QObject *parent = 0);
};

} // The temporary closing of the namespace triggers the problem

namespace Internal {

static inline QString message1()
{
    return Message::tr("message1"); // Had no namespace
}

static inline QString message2()
{
    return Message::tr("message2"); // Already had namespace
}

}



// QTBUG-11426: operator overloads
class LotsaFun : public QObject
{
    Q_OBJECT
public:
    int operator<<(int left, int right);
};

int LotsaFun::operator<<(int left, int right)
{
    tr("this is inside operator<<");
    return left << right;
}



// QTBUG-12683: define in re-opened namespace
namespace NameSchpace {

class YetMoreFun : public QObject
{
    Q_OBJECT
public:
    void funStuff();
};

}

namespace NameSchpace {

#define somevar 1

void YetMoreFun::funStuff()
{
    tr("funStuff!");
}

}



// QTBUG-29998: tr() macro inside square brackets
void blubb()
{
    QMap<QString, QString> d;
    d[LotsaFun::tr("bracketed")] = "plain";
}



// QTBUG-9276 part 2: QT_TR_NOOP in static member initializers
class TestClass
{
    Q_DECLARE_TR_FUNCTIONS(TestClass);

public:
    static const char TEST_STRING[];
};

const char TestClass::TEST_STRING[] = QT_TR_NOOP("Test value");



// derivation from namespaced class
class Class42 : public NameSchpase::YetMoreFun, Gui::BaseClass
{
    Q_OBJECT

    Class42() :
        NameSchpase::YetMoreFun(),
        Gui::BaseClass()
    {
        tr("does that make sense?");
    }
};

Class42::Class42() :
    NameSchpase::YetMoreFun(),
    Gui::BaseClass()
{
    tr("and does that?");
}



// QTBUG-11866: magic comment parsing is too greedy
Class42::hello(int something /*= 17 */, QString str = Class42::tr("eyo"))
{
}



// QTBUG-27974: strings from included sources are not collected
#include "included.cpp"

// test TR_EXCLUDE
#include "notincluded.cpp"



// failure to update index on insertion messes up subsequent de-duplication
int dupeFail()
{
    // First just the Id.
    qtTrId("dupe_id");

    // Then with source
    //% "This is the source"
    qtTrId("dupe_id");

    // Finally, same source, but without ID.
    QCoreApplication::translate("", "This is the source");
}



// QTBUG-42735: lupdate confused by `final` specifier (C++11)
namespace Abc {

class NamespacedFinalClass;

}

class FinalClass final : public QObject
{
    Q_OBJECT

    class SubClass final
    {
        void f()
        {
            tr("nested class context with final");
        }
    };

    void f()
    {
        tr("class context with final");
    }
};

class Abc::NamespacedFinalClass final : public QObject
{
    Q_OBJECT

    void f()
    {
        tr("namespaced class with final");
    }
};



// QTBUG-48776: lupdate fails to recognize translator comment in ternary
// operator construct
void ternary()
{
    const auto aaa =
        obj.condition ?
        //: comment, aaa, true
        QObject::tr("ternary, true, aaa") :
        QObject::tr("ternary, failure, aaa");

    const auto bbb =
        obj.condition ?
        //: comment, bbb, true
        QObject::tr("ternary, bbb, true") :
        //: comment, bbb, false
        QObject::tr("ternary, bbb, false");
}

class TernaryClass : public QObject
{
    Q_OBJECT

    void f()
    {
        const auto ccc =
            obj.condition ?
            //: comment, ccc, true
            tr("ternary, ccc, true") :
            tr("ternary, ccc, false");

        const auto ddd =
            obj.condition ?
            //: comment, ddd, true
            tr("ternary, ddd, true") :
            //: comment, ddd, false
            tr("ternary, ddd, false");
    }
};



// QTBUG-47467: lupdate confused by nullptr in case of plural forms
void nullptrInPlural()
{
    QObject::tr("%n nullptr(s)", nullptr, 3);
    QCoreApplication::translate("Plurals, nullptr", "%n car(s)", nullptr, QCoreApplication::UnicodeUTF8, 1);
}

class nullptrClass : public QObject
{
    Q_OBJECT

    void f()
    {
        tr("%n car(s)", nullptr, 2);
    }
};



// QTBUG-34265: lupdate does not detect NULL and Q_NULLPTR as 0 when being passed as context
void nullMacroInPlural()
{
    QObject::tr("%n NULL(s)", NULL, 3);
    QObject::tr("%n Q_NULLPTR(s)", Q_NULLPTR, 3);
}



// QTBUG-34128: lupdate ignores tr() calls in constructor if a member is
// initialized with C++11 initializer list
class ListInitializationClass : public NameSchpase::YetMoreFun, Gui::BaseClass
{
    Q_OBJECT

    ListInitializationClass() :
        NameSchpase::YetMoreFun(),
        Gui::BaseClass{ },
        a{ 0 },
        b(1),
        c(tr("Hello World"))
    {
        tr("ListInitializationClass in-class constructor");
    }

    ListInitializationClass(int a);

    ListInitializationClass(int a, int b, int c);

    int a;
    int b;
    QString c;
};

ListInitializationClass::ListInitializationClass(int a) :
    b{ { 2, 3 }[a] }
{
    tr("ListInitializationClass out-of-class single member initializer");
}

ListInitializationClass::ListInitializationClass(int a, int b, int c) :
    NameSchpase::YetMoreFun{ },
    Gui::BaseClass(),
    a{ 2 + (a/3) },
    b(b),
    c{ tr("%n item(s)", Q_NULLPTR, c) }
{
    tr("ListInitializationClass out-of-class multi member initializer");
}



// QTBUG-42166: lupdate is confused by C++11 lambdas in constructor initializer lists
class LambdaMemberClass : public Gui::BaseClass
{
    Q_OBJECT

    LambdaMemberClass() :
        Gui::BaseClass(),
        a{ [](){ std::cout << QObject::tr("Hello"); } },
        b([](){ std::cout << "World\n"; })
    {
        tr("LambdaMemberClass in-class constructor");
    }

    LambdaMemberClass(void *);

    std::function<void()> a;
    std::function<void()> b;
};

LambdaMemberClass::LambdaMemberClass(void *) :
    Gui::BaseClass{ },
    a([](){ std::cout << QObject::tr("Hallo "); }),
    b{ [](){ std::cout << "Welt\n"; } }
{
    tr("LambdaMemberClass out-of-class constructor");
}



// Template parameters in base class initialization
class TemplateClass : QVarLengthArray<char, sizeof(std::size_t)>, std::vector<int>
{
    Q_DECLARE_TR_FUNCTIONS(TemplateClass)
    QString member;

public:
    TemplateClass() :
        QVarLengthArray<char, sizeof(std::size_t)>(),
        std::vector<int>(3),
        member(tr("TemplateClass() in-class member initialization"))
    {
        tr("TemplateClass() in-class body");
    }

    TemplateClass(void *);
    TemplateClass(int);
};

// supported: combination of parens in base class template parameter with direct initialization (parens)
TemplateClass::TemplateClass(void *) :
    QVarLengthArray<char, sizeof(std::size_t)>(),
    std::vector<int>{ 1, 2 },
    member{ tr("TemplateClass(void *) out-of-class member initialization") }
{
    tr("TemplateClass(void *) out-of-class body");
}

// not supported: combination of parens in base class template parameter with list initialization (braces)
TemplateClass::TemplateClass(int) :
    QVarLengthArray<char, sizeof(std::size_t)>{ 3, 4, 5 },
    member(tr("[unsupported] TemplateClass(int) out-of-class member initialization"))
{
    tr("[unsupported] TemplateClass(int) out-of-class body");
}



// Related to QTBUG-53644, adapted from qglobal.h.
// Namespace Private must be parsed correctly for TranslatedAfterPrivate to work.
namespace Private {
    template <class T> struct Class1 { T t; };
    template <class T> struct Class1<T &> : Class1<T> {};
    template <class T> struct Class2 { enum { Value = sizeof(T) }; };
}  // namespace Private
class TranslatedAfterPrivate
{
    Q_OBJECT
    TranslatedAfterPrivate()
    {
        tr("Must be in context TranslatedAfterPrivate");
    }
};

#include<QObject>

QObject::tr("message after system include without space");

#include"qobject.h"

QObject::tr("message after local include without space");
