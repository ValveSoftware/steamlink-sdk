/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#include <QDebug>
#include <QFile>
#include <QDir>

#include "codegenerator.h"
using namespace CodeGenerator;

const Item argument = "arg" + Counter();
const Item argumentRef = "&arg" + Counter();
const Item argumentType  = "Arg" + Counter();
const Item constArgumentType  = "const Arg" + Counter();
const Item parameterType  = "Param" + Counter();

Group argumentTypes(argumentType);          // expands to ",Arg1, Arg2, ..."
Group argumentTypesNoPrefix(argumentType);  // expands to "Arg1, Arg2, ..."
Group arguments(argument);                  // expands to ",arg1, arg2, ..."
Group argumentsNoPrefix(argument);          // expands to "arg1, arg2, ..."
Group parameterTypes(parameterType);        // expands to ",Param1, Param2, ..."
Group parameterTypesNoPrefix(parameterType); // expands to "Param1, Param2, ..."
Group typenameTypes("typename " + parameterType + ", typename " + argumentType); // expands to " ,typename Param1, typename Arg1, ..."
Group types(parameterType + ", " + argumentType); // expands to ", Param1, Arg1, ..."
Group functionParameters(constArgumentType + " " + argumentRef);
Group typenameArgumentTypes("typename " + argumentType);

Group initializers(argument + "(" + argument + ")");
Group classData(argumentType +" "  + argument + ";");
Group arglist(argument);
Group typeList(argumentTypes);

void init()
{
    argumentTypes.setPrefix(", ");
    arguments.setPrefix(", ");
    parameterTypes.setPrefix(", ");
    typenameTypes.setPrefix(", ");
    types.setPrefix(", ");
    functionParameters.setPrefix(", ");
    typenameArgumentTypes.setPrefix(", ");

    initializers.setPrefix(", ");
    classData.setSeparator(" ");
    classData.setPrefix("    ");
    arglist.setPrefix(", ");
    typeList.setPrefix(", ");
}


Item Line(Item item)
{
    return item + "\n";
}

Item generateRunFunctions(int repeats, bool withExplicitPoolArg)
{
    Item functionPointerType = "T (*)(" + parameterTypesNoPrefix + ")";

    Item functionPointerParameter = "T (*functionPointer)(" + parameterTypesNoPrefix + ")";

    const Item pool = withExplicitPoolArg ? "QThreadPool *pool, " : "";
    const char * const startArg = withExplicitPoolArg ? "pool" : "";

    // plain functions
    Repeater functions = Line ("template <typename T" + typenameTypes + ">") +
                         Line ("QFuture<T> run(" + pool + functionPointerParameter + functionParameters + ")")  +
                         Line("{") +
                         Line("    return (new StoredFunctorCall" + Counter() + "<T, " +
                                   functionPointerType + argumentTypes + ">(functionPointer" + arguments + "))->start(" + startArg + ");") +
                         Line("}");
    functions.setRepeatCount(repeats);

    // function objects by value
    Repeater functionObjects =  Line ("template <typename FunctionObject" + typenameArgumentTypes + ">") +
                                Line ("QFuture<typename FunctionObject::result_type> run(" + pool + "FunctionObject functionObject" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new StoredFunctorCall" + Counter() +
                                     "<typename FunctionObject::result_type, FunctionObject" +
                                     argumentTypes + ">(functionObject" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    functionObjects.setRepeatCount(repeats);

    // function objects by pointer
    Repeater functionObjectsPointer =  Line ("template <typename FunctionObject" + typenameArgumentTypes + ">") +
                                Line ("QFuture<typename FunctionObject::result_type> run(" + pool + "FunctionObject *functionObject" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new typename SelectStoredFunctorPointerCall" + Counter() +
                                     "<typename FunctionObject::result_type, FunctionObject" +
                                     argumentTypes + ">::type(functionObject" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    functionObjectsPointer.setRepeatCount(repeats);

    // member functions by value
    Repeater memberFunction =  Line ("template <typename T, typename Class" + typenameTypes + ">") +
                                Line ("QFuture<T> run(" + pool + "const Class &object, T (Class::*fn)(" + parameterTypesNoPrefix  + ")" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new typename SelectStoredMemberFunctionCall" + Counter() +
                                     "<T, Class" +
                                     types + ">::type(fn, object" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    memberFunction.setRepeatCount(repeats);

    // const member functions by value
    Repeater constMemberFunction =  Line ("template <typename T, typename Class" + typenameTypes + ">") +
                                Line ("QFuture<T> run(" + pool + "const Class &object, T (Class::*fn)(" + parameterTypesNoPrefix  + ") const" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new typename SelectStoredConstMemberFunctionCall" + Counter() +
                                     "<T, Class" +
                                     types + ">::type(fn, object" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    constMemberFunction.setRepeatCount(repeats);

    // member functions by class pointer
    Repeater memberFunctionPointer =  Line ("template <typename T, typename Class" + typenameTypes + ">") +
                                Line ("QFuture<T> run(" + pool + "Class *object, T (Class::*fn)(" + parameterTypesNoPrefix  + ")" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new typename SelectStoredMemberFunctionPointerCall" + Counter() +
                                     "<T, Class" +
                                     types + ">::type(fn, object" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    memberFunctionPointer.setRepeatCount(repeats);

    // const member functions by class pointer
    Repeater constMemberFunctionPointer =  Line ("template <typename T, typename Class" + typenameTypes + ">") +
                                Line ("QFuture<T> run(" + pool + "const Class *object, T (Class::*fn)(" + parameterTypesNoPrefix  + ") const" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new typename SelectStoredConstMemberFunctionPointerCall" + Counter() +
                                     "<T, Class" +
                                     types + ">::type(fn, object" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    constMemberFunctionPointer.setRepeatCount(repeats);


    Item interfaceFunctionPointerType = "void (*)(QFutureInterface<T> &" + argumentTypes + ")";
    Item interfaceFunctionPointerParameter = "void (*functionPointer)(QFutureInterface<T> &" + argumentTypes + ")";
/*
    // QFutureInterface functions
    Repeater interfaceFunctions = Line ("template <typename T" + typenameTypes + ">") +
                         Line ("QFuture<T> run(" + pool + interfaceFunctionPointerParameter + functionParameters + ")")  +
                         Line("{") +
                         Line("    return (new StoredInterfaceFunctionCall" + Counter() + "<T, " +
                                   interfaceFunctionPointerType + typenameArgumentTypes + ">(functionPointer" + arguments + "))->start(" + startArg + ");") +
                         Line("}");
    functions.setRepeatCount(repeats);
    interfaceFunctions.setRepeatCount(repeats);

    // member functions by class pointer
    Repeater interfaceMemberFunction =  Line ("template <typename Class, typename T" + typenameTypes + ">") +
                                Line ("QFuture<T> run(" + pool + "void (Class::*fn)(QFutureInterface<T> &), Class *object" + functionParameters + ")") +
                                Line("{") +
                                Line("    return (new StoredInterfaceMemberFunctionCall" + Counter() +
                                     "<T, void (Class::*)(QFutureInterface<T> &), Class" +
                                     typenameArgumentTypes + ">(fn, object" + arguments + "))->start(" + startArg + ");") +
                                Line("}");
    memberFunctionPointer.setRepeatCount(repeats);
*/
    return functions + Line("") + functionObjects + Line("") + functionObjectsPointer + Line("")
          + memberFunction + Line("") + constMemberFunction + Line("")
          + memberFunctionPointer + Line("") + constMemberFunctionPointer + Line("")
  /*        + interfaceFunctions + Line("") + interfaceMemberFunction + Line("")*/
    ;
}


Item functions(Item className, Item functorType, Item callLine)
{
    return
    Line("template <typename T, typename FunctionPointer" +  typenameArgumentTypes + ">") +
    Line("struct " + className +  Counter() +": public RunFunctionTask<T>") +
    Line("{") +
    Line("    inline " + className + Counter() + "(" + functorType + " function" + functionParameters +")") +
    Line("      : function(function)" + initializers + " {}") +
    Line("    void runFunctor() {" + callLine + argumentsNoPrefix + "); }") +
    Line("    " + functorType + " function;") +
    Line(       classData) +
    Line("};") +
    Line("");
}

Item functionSelector(Item classNameBase)
{
    return
    Line("template <typename T, typename FunctionPointer" + typenameArgumentTypes + ">") +
    Line("struct Select" + classNameBase + Counter()) +
    Line("{") +
    Line("    typedef typename SelectSpecialization<T>::template") +
    Line("        Type<" + classNameBase + Counter() + "    <T, FunctionPointer" + argumentTypes + ">,") +
    Line("             Void" + classNameBase + Counter() + "<T, FunctionPointer" + argumentTypes + "> >::type type;") +
    Line("};");
}

Item memberFunctions(Item className, Item constFunction, Item objectArgument, Item objectMember, Item callLine)
{
    return
    Line("template <typename T, typename Class"  + typenameTypes + ">") +
    Line("class " + className + Counter() + " : public RunFunctionTask<T>") +
    Line("{") +
    Line("public:")+
    Line("    " + className + Counter() + "(T (Class::*fn)(" + parameterTypesNoPrefix  + ") " + constFunction + ", " + objectArgument + functionParameters + ")") +
    Line("    : fn(fn), object(object)" + initializers + "{ }" ) +
    Line("")+
    Line("    void runFunctor()")+
    Line("    {")+
    Line("        " + callLine + argumentsNoPrefix +  ");")+
    Line("    }")+
    Line("private:")+
    Line("    T (Class::*fn)(" + parameterTypesNoPrefix  + ")" + constFunction + ";")+
    Line("    " + objectMember + ";") +
    Line(        classData) +
    Line("};");
}

Item memberFunctionSelector(Item classNameBase)
{
    return
    Line("template <typename T, typename Class" + typenameTypes + ">") +
    Line("struct Select" + classNameBase + Counter()) +
    Line("{") +
    Line("    typedef typename SelectSpecialization<T>::template") +
    Line("        Type<" + classNameBase + Counter() + "    <T, Class" + types + ">,") +
    Line("             Void" + classNameBase + Counter() + "<T, Class" + types + "> >::type type;") +
    Line("};");
}

Item generateSFCs(int repeats)
{

    Item functionPointerTypedef = "typedef void (*FunctionPointer)(" + argumentTypesNoPrefix + ");";

    Repeater dataStructures =

    // Function objects by value
    functions(Item("StoredFunctorCall"), Item("FunctionPointer"), Item(" this->result = function(")) +
    functions(Item("VoidStoredFunctorCall"), Item("FunctionPointer"), Item(" function(")) +
    functionSelector(Item("StoredFunctorCall")) +

    // Function objects by pointer
    functions(Item("StoredFunctorPointerCall"), Item("FunctionPointer *"), Item(" this->result =(*function)(")) +
    functions(Item("VoidStoredFunctorPointerCall"), Item("FunctionPointer *"), Item("(*function)(")) +
    functionSelector(Item("StoredFunctorPointerCall")) +

    // Member functions by value
    memberFunctions(Item("StoredMemberFunctionCall"), Item(""), Item("const Class &object"), Item("Class object"), Item("this->result = (object.*fn)(")) +
    memberFunctions(Item("VoidStoredMemberFunctionCall"), Item(""), Item("const Class &object"), Item("Class object"), Item("(object.*fn)(")) +
    memberFunctionSelector(Item("StoredMemberFunctionCall")) +

    // Const Member functions by value
    memberFunctions(Item("StoredConstMemberFunctionCall"), Item("const"), Item("const Class &object"), Item("const Class object"), Item("this->result = (object.*fn)(")) +
    memberFunctions(Item("VoidStoredConstMemberFunctionCall"), Item("const"), Item("const Class &object"), Item("const Class object"), Item("(object.*fn)(")) +
    memberFunctionSelector(Item("StoredConstMemberFunctionCall")) +

    // Member functions by pointer
    memberFunctions(Item("StoredMemberFunctionPointerCall"), Item(""), Item("Class *object"), Item("Class *object"), Item("this->result = (object->*fn)(")) +
    memberFunctions(Item("VoidStoredMemberFunctionPointerCall"), Item(""), Item("Class *object"), Item("Class *object"), Item("(object->*fn)(")) +
    memberFunctionSelector(Item("StoredMemberFunctionPointerCall")) +

    // const member functions by pointer
    memberFunctions(Item("StoredConstMemberFunctionPointerCall"), Item("const"), Item("Class const *object"), Item("Class const *object"), Item("this->result = (object->*fn)(")) +
    memberFunctions(Item("VoidStoredConstMemberFunctionPointerCall"), Item("const"), Item("Class const *object"), Item("Class const *object"), Item("(object->*fn)(")) +
    memberFunctionSelector(Item("StoredConstMemberFunctionPointerCall"));

    dataStructures.setRepeatCount(repeats);
    return dataStructures;
}

void writeFile(QString fileName, QByteArray contents)
{
    QFile runFile(fileName);
    if (runFile.open(QIODevice::WriteOnly) == false) {
        qDebug() << "Write to" << fileName << "failed";
        return;
    }

    runFile.write(contents);
    runFile.close();
    qDebug() << "Write to" << fileName << "Ok";
}

Item dollarQuote(Item item)
{
    return Item("$") + item + Item("$");
}

static int usage(const char *executable)
{
    qDebug("Usage: %s path/to/qtconcurrent", executable);
    return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    if (argc !=2)
        return usage(argv[0]);

    const QDir outdir(QFile::decodeName(argv[1]));

    const int repeats = 6;
    init();
    Item run =  (
                       Line("/****************************************************************************") +
                       Line("**") +
                       Line("** Copyright (C) 2015 The Qt Company Ltd.") +
                       Line("** Contact: http://www.qt.io/licensing/") +
                       Line("**") +
                       Line("** This file is part of the Qt Toolkit.") +
                       Line("**") +
                       Line("****************************************************************************/") +
                       Line("") +
                       Line("// Generated code, do not edit! Use generator at tools/qtconcurrent/generaterun/") +
                       Line("#ifndef QTCONCURRENT_RUN_H") +
                       Line("#define QTCONCURRENT_RUN_H") +
                       Line("") +
                       Line("#include <QtConcurrent/qtconcurrentcompilertest.h>") +
                       Line("") +
                       Line("#ifndef QT_NO_CONCURRENT") +
                       Line("") +
                       Line("#include <QtConcurrent/qtconcurrentrunbase.h>") +
                       Line("#include <QtConcurrent/qtconcurrentstoredfunctioncall.h>") +
                       Line("") +
                       Line("QT_BEGIN_NAMESPACE") +
                       Line("") +
                       Line("") +
                       Line("#ifdef Q_QDOC") +
                       Line("") +
                       Line("namespace QtConcurrent {") +
                       Line("") +
                       Line("    template <typename T>") +
                       Line("    QFuture<T> run(Function function, ...);") +
                       Line("") +
                       Line("    template <typename T>") +
                       Line("    QFuture<T> run(QThreadPool *pool, Function function, ...);") +
                       Line("") +
                       Line("} // namespace QtConcurrent") +
                       Line("") +
                       Line("#else") +
                       Line("") +
                       Line("namespace QtConcurrent {") +
                       Line("") +
                       generateRunFunctions(repeats, false) +
                       Line("") +
                       generateRunFunctions(repeats, true) +
                       Line("} //namespace QtConcurrent") +
                       Line("") +
                       Line("#endif // Q_QDOC") +
                       Line("") +
                       Line("QT_END_NAMESPACE") +
                       Line("") +
                       Line("#endif // QT_NO_CONCURRENT") +
                       Line("") +
                       Line("#endif")
                      );

    writeFile(outdir.filePath("qtconcurrentrun.h"), run.generate());

    Item storedFunctionCall = (
                                     Line("/****************************************************************************") +
                                     Line("**") +
                                     Line("** Copyright (C) 2015 The Qt Company Ltd.") +
                                     Line("** Contact: http://www.qt.io/licensing/") +
                                     Line("**") +
                                     Line("** This file is part of the Qt Toolkit.") +
                                     Line("**") +
                                     Line("****************************************************************************/") +
                                     Line("") +
                                     Line("// Generated code, do not edit! Use generator at tools/qtconcurrent/generaterun/") +
                                     Line("#ifndef QTCONCURRENT_STOREDFUNCTIONCALL_H") +
                                     Line("#define QTCONCURRENT_STOREDFUNCTIONCALL_H") +
                                     Line("") +
                                     Line("#include <QtCore/qglobal.h>") +
                                     Line("") +
                                     Line("#ifndef QT_NO_CONCURRENT") +
                                     Line("#include <QtConcurrent/qtconcurrentrunbase.h>") +
                                     Line("") +
                                     Line("QT_BEGIN_NAMESPACE") +
                                     Line("") +
                                     Line("#ifndef Q_QDOC") +
                                     Line("") +
                                     Line("namespace QtConcurrent {") +
                                     generateSFCs(repeats) +
                                     Line("} //namespace QtConcurrent") +
                                     Line("") +
                                     Line("#endif // Q_QDOC") +
                                     Line("") +
                                     Line("QT_END_NAMESPACE") +
                                     Line("") +
                                     Line("#endif // QT_NO_CONCURRENT") +
                                     Line("") +
                                     Line("#endif")
                                    );

    writeFile(outdir.filePath("qtconcurrentstoredfunctioncall.h"), storedFunctionCall.generate());
}


