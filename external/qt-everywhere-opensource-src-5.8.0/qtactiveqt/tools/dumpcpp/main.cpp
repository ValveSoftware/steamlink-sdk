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

#include <QAxObject>
#include <QFile>
#include <QMetaObject>
#include <QMetaEnum>
#include <QTextStream>
#include <QSettings>
#include <QStringList>
#include <QTemporaryFile>
#include <QUuid>
#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QWidget>
#include <QFileInfo>
#include <qt_windows.h>
#include <ocidl.h>
#include <private/qmetaobject_p.h>
QT_BEGIN_NAMESPACE

static ITypeInfo *currentTypeInfo = 0;

enum ProgramMode {
    GenerateMode,
    TypeLibID
};

enum ObjectCategory
{
    DefaultObject    = 0x00,
    SubObject        = 0x001,
    ActiveX          = 0x002,
    NoMetaObject     = 0x004,
    NoImplementation = 0x008,
    NoDeclaration    = 0x010,
    NoInlines        = 0x020,
    OnlyInlines      = 0x040,
    Licensed         = 0x100,
};

Q_DECLARE_FLAGS(ObjectCategories, ObjectCategory)
Q_DECLARE_OPERATORS_FOR_FLAGS(ObjectCategories)

extern QMetaObject *qax_readEnumInfo(ITypeLib *typeLib, const QMetaObject *parentObject);
extern QMetaObject *qax_readClassInfo(ITypeLib *typeLib, ITypeInfo *typeInfo, const QMetaObject *parentObject);
extern QMetaObject *qax_readInterfaceInfo(ITypeLib *typeLib, ITypeInfo *typeInfo, const QMetaObject *parentObject);
extern QList<QByteArray> qax_qualified_usertypes;
extern QString qax_docuFromName(ITypeInfo *typeInfo, const QString &name);
extern bool qax_dispatchEqualsIDispatch;
extern void qax_deleteMetaObject(QMetaObject *mo);

static QMap<QByteArray, QByteArray> namespaceForType;
static QVector<QByteArray> strings;
static QHash<QByteArray, int> stringIndex; // Optimization, speeds up generation
static QByteArrayList vTableOnlyStubs;

void writeEnums(QTextStream &out, const QMetaObject *mo)
{
    // enums
    for (int ienum = mo->enumeratorOffset(); ienum < mo->enumeratorCount(); ++ienum) {
        QMetaEnum metaEnum = mo->enumerator(ienum);
        out << "    enum " << metaEnum.name() << " {" << endl;
        for (int k = 0; k < metaEnum.keyCount(); ++k) {
            QByteArray key(metaEnum.key(k));
            out << "        " << key.leftJustified(24) << "= " << metaEnum.value(k);
            if (k < metaEnum.keyCount() - 1)
                out << ',';
            out << endl;
        }
        out << "    };" << endl;
        out << endl;
    }
}

void writeHeader(QTextStream &out, const QString &nameSpace, const QString &outFileName)
{
    out << "#ifndef QAX_DUMPCPP_" << outFileName.toUpper() << "_H" << endl;
    out << "#define QAX_DUMPCPP_" << outFileName.toUpper() << "_H" << endl;
    out << endl;
    out << "// Define this symbol to __declspec(dllexport) or __declspec(dllimport)" << endl;
    out << "#ifndef " << nameSpace.toUpper() << "_EXPORT" << endl;
    out << "#define " << nameSpace.toUpper() << "_EXPORT" << endl;
    out << "#endif" << endl;
    out << endl;
    out << "#include <qaxobject.h>" << endl;
    out << "#include <qaxwidget.h>" << endl;
    out << "#include <qdatetime.h>" << endl;
    out << "#include <qpixmap.h>" << endl;
    out << endl;
    out << "struct IDispatch;" << endl;
    out << endl;
}

void generateNameSpace(QTextStream &out, const QMetaObject *mo, const QByteArray &nameSpace)
{
    out << "namespace " << nameSpace << " {" << endl;
    out << endl;
    writeEnums(out, mo);

    // don't close on purpose
}

static QByteArray joinParameterNames(const QList<QByteArray> &parameterNames)
{
    QByteArray slotParameters;
    for (int p = 0; p < parameterNames.count(); ++p) {
        slotParameters += parameterNames.at(p);
        if (p < parameterNames.count() - 1)
            slotParameters += ',';
    }

    return slotParameters;
}

QByteArray constRefify(const QByteArray &type)
{
    QByteArray ctype(type);
    if (type == "QString" || type == "QPixmap"
        || type == "QVariant" || type == "QDateTime"
        || type == "QColor" || type == "QFont"
        || type == "QByteArray" || type == "QValueList<QVariant>"
        || type == "QStringList")
        ctype = "const " + ctype + '&';

    return ctype;
}

void generateClassDecl(QTextStream &out, const QString &controlID, const QMetaObject *mo,
                       const QByteArray &className, const QByteArray &nameSpace,
                       ObjectCategories category)
{
    QList<QByteArray> functions;

    QByteArray indent;
    if (!(category & OnlyInlines))
        indent = "    ";

    if (!(category & OnlyInlines)) {
        // constructors
        out << "class " << nameSpace.toUpper() << "_EXPORT " << className << " : public ";
        if (category & ActiveX)
            out << "QAxWidget";
        else
            out << "QAxObject";
        out << endl;

        out << '{' << endl;
        out << "public:" << endl;
        out << "    " << className << '(';
        if (category & Licensed)
            out << "const QString &licenseKey = QString(), ";
        if (category & ActiveX)
            out << "QWidget *parent = 0, Qt::WindowFlags f";
        else if (category & SubObject)
            out << "IDispatch *subobject = 0, QAxObject *parent";
        else
            out << "QObject *parent";
        out << " = 0)" << endl;
        out << "    : ";
        if (category & ActiveX)
            out << "QAxWidget(parent, f";
        else if (category & SubObject)
            out << "QAxObject((IUnknown*)subobject, parent";
        else
            out << "QAxObject(parent";
        out << ')' << endl;
        out << "    {" << endl;
        if (category & SubObject)
            out << "        internalRelease();" << endl;
        else if (category & Licensed) {
            out << "        if (licenseKey.isEmpty())" << endl;
            out << "            setControl(\"" << controlID << "\");" << endl;
            out << "        else" << endl;
            out << "            setControl(\"" << controlID << ":\" + licenseKey);" << endl;
        } else {
            out << "        setControl(\"" << controlID << "\");" << endl;
        }
        out << "    }" << endl;
        out << endl;

        for (int ci = mo->classInfoOffset(); ci < mo->classInfoCount(); ++ci) {
            QMetaClassInfo info = mo->classInfo(ci);
            QByteArray iface_name = info.name();
            if (iface_name.startsWith("Event "))
                continue;

            QByteArray iface_class = info.value();

            out << "    " << className << '(' << iface_class << " *iface)" << endl;

            if (category & ActiveX)
                out << "    : QAxWidget()" << endl;
            else
                out << "    : QAxObject()" << endl;
            out << "    {" << endl;
            out << "        initializeFrom(iface);" << endl;
            out << "        delete iface;" << endl;
            out << "    }" << endl;
            out << endl;
        }
    }

    functions << className;

    // enums
    if (nameSpace.isEmpty() && !(category & OnlyInlines)) {
        for (int ienum = mo->enumeratorOffset(); ienum < mo->enumeratorCount(); ++ienum) {
            QMetaEnum metaEnum = mo->enumerator(ienum);
            out << "    enum " << metaEnum.name() << " {" << endl;
            for (int k = 0; k < metaEnum.keyCount(); ++k) {
                QByteArray key(metaEnum.key(k));
                out << "        " << key.leftJustified(24) << "= " << metaEnum.value(k);
                if (k < metaEnum.keyCount() - 1)
                    out << ',';
                out << endl;
            }
            out << "    };" << endl;
            out << endl;
        }
    }
    // QAxBase public virtual functions.
    QList<QByteArray> axBase_vfuncs;
    axBase_vfuncs.append("metaObject");
    axBase_vfuncs.append("qObject");
    axBase_vfuncs.append("className");
    axBase_vfuncs.append("propertyWritable");
    axBase_vfuncs.append("setPropertyWritable");

    // properties
    for (int iprop = mo->propertyOffset(); iprop < mo->propertyCount(); ++iprop) {
        QMetaProperty property = mo->property(iprop);
        if (!property.isReadable())
            continue;

        QByteArray propertyName(property.name());
        if (propertyName == "control" || propertyName == className)
            continue;

        if (!(category & OnlyInlines)) {
            out << indent << "/*" << endl << indent << "Property " << propertyName << endl;
            QString documentation = qax_docuFromName(currentTypeInfo, QString::fromLatin1(propertyName.constData()));
            if (!documentation.isEmpty()) {
                out << endl;
                out << indent << documentation << endl;
            }
            out << indent << "*/" << endl;
        }

        // Check whether the new function conflicts with any of QAxBase public virtual functions.
        // If so, prepend the function name with '<classname>_'. Since all internal metaobject magic
        // remains the same, we have to use the original name when used with QObject::connect or QMetaObject
        QByteArray propertyFunctionName(propertyName);
        if (axBase_vfuncs.contains(propertyFunctionName)) {
            propertyFunctionName = className + '_' + propertyName;
            qWarning("property conflits with QAXBase: %s changed to %s", propertyName.constData(), propertyFunctionName.constData());
        }

        QByteArray propertyType(property.typeName());

        QByteArray simplePropType = propertyType;
        simplePropType.replace('*', "");

        out << indent << "inline ";
        bool foreignNamespace = true;
        if (!propertyType.contains("::") &&
            (qax_qualified_usertypes.contains(simplePropType) || qax_qualified_usertypes.contains("enum "+ simplePropType))
           ) {
            propertyType.prepend(nameSpace + "::");
            foreignNamespace = false;
        }

        out << propertyType << ' ';

        if (category & OnlyInlines)
            out << className << "::";
        out << propertyFunctionName << "() const";

        if (!(category & NoInlines)) {
            out << endl << indent << '{' << endl;
            if (qax_qualified_usertypes.contains(simplePropType)) {
                if (foreignNamespace)
                    out << "#ifdef QAX_DUMPCPP_" << propertyType.left(propertyType.indexOf("::")).toUpper() << "_H" << endl;
                out << indent << "    " << propertyType << " qax_pointer = 0;" << endl;
                QByteArray simplePropTypeWithNamespace = propertyType;
                simplePropTypeWithNamespace.replace('*', "");
                out << indent << "    qRegisterMetaType<" << propertyType << ">(\"" << property.typeName() << "\", &qax_pointer);" << endl;
                out << indent << "    qRegisterMetaType<" << simplePropTypeWithNamespace << ">(\"" << simplePropType << "\", qax_pointer);" << endl;
            }
            out << indent << "    QVariant qax_result = property(\"" << propertyName << "\");" << endl;
            if (propertyType.length() && propertyType.at(propertyType.length()-1) == '*')
                out << indent << "    if (!qax_result.constData()) return 0;" << endl;
            out << indent << "    Q_ASSERT(qax_result.isValid());" << endl;
            if (qax_qualified_usertypes.contains(simplePropType)) {
                simplePropType = propertyType;
                simplePropType.replace('*', "");
                out << indent << "    return *(" << propertyType << "*)qax_result.constData();" << endl;
                if (foreignNamespace) {
                    out << "#else" << endl;
                    out << indent << "    return 0; // foreign namespace not included" << endl;
                    out << "#endif" << endl;
                }

            } else {
                out << indent << "    return *(" << propertyType << "*)qax_result.constData();" << endl;
            }
            out << indent << '}' << endl;
        } else {
            out << "; //Returns the value of " << propertyName << endl;
        }

        functions << propertyName;

        if (property.isWritable()) {
            QByteArray setter(propertyName);
            if (isupper(setter.at(0))) {
                setter = "Set" + setter;
            } else {
                setter[0] = char(toupper(setter[0]));
                setter = "set" + setter;
            }

            out << indent << "inline " << "void ";
            if (category & OnlyInlines)
                out << className << "::";
            out << setter << '(' << constRefify(propertyType) << " value)";

            if (!(category & NoInlines)) {
                if (propertyType.endsWith('*')) {
                    out << '{' << endl;
                    out << "    int typeId = qRegisterMetaType<" << propertyType << ">(\"" << propertyType << "\", &value);" << endl;
                    out << "    setProperty(\"" << propertyName << "\", QVariant(typeId, &value));" << endl;
                    out << '}' << endl;
                } else {
                    out << "{ setProperty(\"" << propertyName << "\", QVariant(value)); }" << endl;
                }
            } else {
                out << "; //Sets the value of the " << propertyName << " property" << endl;
            }

            functions << setter;
        }

        out << endl;
    }

    // slots - but not property setters
    int defaultArguments = 0;
    for (int islot = mo->methodOffset(); islot < mo->methodCount(); ++islot) {
        const QMetaMethod slot(mo->method(islot));
        if (slot.methodType() != QMetaMethod::Slot)
            continue;

#if 0
        // makes not sense really to respect default arguments...
        if (slot.attributes() & Cloned) {
            ++defaultArguments;
            continue;
        }
#endif

        QByteArray slotSignature(slot.methodSignature());
        QByteArray slotName = slotSignature.left(slotSignature.indexOf('('));
        if (functions.contains(slotName))
            continue;

        if (!(category & OnlyInlines)) {
            out << indent << "/*" << endl << indent << "Method " << slotName << endl;
            QString documentation = qax_docuFromName(currentTypeInfo, QString::fromLatin1(slotName.constData()));
            if (!documentation.isEmpty()) {
                out << endl;
                out << indent << documentation << endl;
            }
            out << indent << "*/" << endl;
        }

        QByteArray slotParameters(joinParameterNames(slot.parameterNames()));
        QByteArray slotTag(slot.tag());
        QByteArray slotType(slot.typeName());

        QByteArray simpleSlotType = slotType;
        simpleSlotType.replace('*', "");
        if (!slotType.contains("::") && qax_qualified_usertypes.contains(simpleSlotType))
            slotType.prepend(nameSpace + "::");


        QByteArray slotNamedSignature;
        if (slotSignature.endsWith("()")) { // no parameters - no names
            slotNamedSignature = slotSignature;
        } else {
            slotNamedSignature = slotSignature.left(slotSignature.indexOf('(') + 1);
            QByteArray slotSignatureTruncated(slotSignature.mid(slotNamedSignature.length()));
            slotSignatureTruncated.truncate(slotSignatureTruncated.length() - 1);

            QList<QByteArray> signatureSplit = slotSignatureTruncated.split(',');
            QList<QByteArray> parameterSplit;
            if (slotParameters.isEmpty()) { // generate parameter names
                for (int i = 0; i < signatureSplit.count(); ++i)
                    parameterSplit << QByteArray("p") + QByteArray::number(i);
            } else {
                parameterSplit = slotParameters.split(',');
            }

            for (int i = 0; i < signatureSplit.count(); ++i) {
                QByteArray parameterType = signatureSplit.at(i);
                if (!parameterType.contains("::") && namespaceForType.contains(parameterType))
                    parameterType.prepend(namespaceForType.value(parameterType) + "::");

                QByteArray arraySpec; // transform array method signature "foo(int[4])" ->"foo(int p[4])"
                const int arrayPos = parameterType.lastIndexOf('[');
                if (arrayPos != -1) {
                    arraySpec = parameterType.right(parameterType.size() - arrayPos);
                    parameterType.truncate(arrayPos);
                }
                slotNamedSignature += constRefify(parameterType);
                slotNamedSignature += ' ';
                slotNamedSignature += parameterSplit.at(i);
                slotNamedSignature += arraySpec;
                if (defaultArguments >= signatureSplit.count() - i) {
                    slotNamedSignature += " = ";
                    slotNamedSignature += parameterType + "()";
                }
                if (i + 1 < signatureSplit.count())
                    slotNamedSignature += ", ";
            }
            slotNamedSignature += ')';
        }

        out << indent << "inline ";

        if (!slotTag.isEmpty())
            out << slotTag << ' ';
        else
            out << slotType << ' ';
        if (category & OnlyInlines)
            out << className << "::";

        // Update function name in case of conflicts with QAxBase public virtual functions.
        int parnIdx = slotNamedSignature.indexOf('(');
        QByteArray slotOriginalName =  slotNamedSignature.left(parnIdx);
        if (axBase_vfuncs.contains(slotOriginalName)) {
            QByteArray newSignature = className + '_' + slotOriginalName;
            newSignature += slotNamedSignature.mid(parnIdx);
            qWarning("function name conflits with QAXBase %s changed to %s", slotNamedSignature.constData(), newSignature.constData());
            slotNamedSignature = newSignature;
        }

        out << slotNamedSignature;

        if (category & NoInlines) {
            out << ';' << endl;
        } else {
            out << endl;
            out << indent << '{' << endl;

            if (slotType != QByteArrayLiteral("void")) {
                out << indent << "    " << slotType << " qax_result";
                if (slotType.endsWith('*'))
                    out << " = 0";
                out << ';' << endl;
                if (qax_qualified_usertypes.contains(simpleSlotType)) {
                    bool foreignNamespace = simpleSlotType.contains("::");
                    if (foreignNamespace)
                        out << "#ifdef QAX_DUMPCPP_" << simpleSlotType.left(simpleSlotType.indexOf(':')).toUpper() << "_H" << endl;
                    QByteArray simpleSlotTypeWithNamespace = slotType;
                    simpleSlotTypeWithNamespace.replace('*', "");
                    out << indent << "    qRegisterMetaType<" << simpleSlotTypeWithNamespace << "*>(\"" << simpleSlotType << "*\", &qax_result);" << endl;
                    if (!vTableOnlyStubs.contains(simpleSlotTypeWithNamespace))
                        out << indent << "    qRegisterMetaType<" << simpleSlotTypeWithNamespace << ">(\"" << simpleSlotType << "\", qax_result);" << endl;
                    if (foreignNamespace)
                        out << "#endif" << endl;
                }
            }
            out << indent << "    void *_a[] = {";
            if (slotType != QByteArrayLiteral("void"))
                out << "(void*)&qax_result";
            else
                out << '0';
            if (!slotParameters.isEmpty()) {
                out << ", (void*)&";
                out << slotParameters.replace(",", ", (void*)&");
            }
            out << "};" << endl;

            out << indent << "    qt_metacall(QMetaObject::InvokeMetaMethod, " << islot << ", _a);" << endl;
            if (slotType != QByteArrayLiteral("void"))
                out << indent << "    return qax_result;" << endl;
            out << indent << '}' << endl;
        }

        out << endl;
        defaultArguments = 0;
    }

    if (!(category & OnlyInlines)) {
        if (!(category & NoMetaObject)) {
            out << "// meta object functions" << endl;
            out << "    static const QMetaObject staticMetaObject;" << endl;
            out << "    virtual const QMetaObject *metaObject() const { return &staticMetaObject; }" << endl;
            out << "    virtual void *qt_metacast(const char *);" << endl;
        }

        out << "};" << endl;
    }
}

#define addStringIdx(string) \
    out << stridx(string) << ", ";

// The following functions were copied from moc generator with only some minor changes
void strreg(const QByteArray &s)
{
    if (!stringIndex.contains(s)) {
        stringIndex.insert(s, strings.size());
        strings.append(s);
    }
}

void strDetachAndRegister(QByteArray s)
{
    s.detach();
    strreg(s);
}

int stridx(const QByteArray &s)
{
    int i = stringIndex.value(s);
    Q_ASSERT_X(i != -1, Q_FUNC_INFO, "We forgot to register some strings");
    return i;
}

const char *metaTypeEnumValueString(int type)
{
#define RETURN_METATYPENAME_STRING(MetaTypeName, MetaTypeId, RealType) \
    case QMetaType::MetaTypeName: return #MetaTypeName;

    switch (type) {
QT_FOR_EACH_STATIC_TYPE(RETURN_METATYPENAME_STRING)
    }
#undef RETURN_METATYPENAME_STRING
    return 0;
}

int nameToBuiltinType(const QByteArray &name)
{
    if (name.isEmpty())
        return 0;

    const int tp = QMetaType::type(name.constData());
    return tp < QMetaType::User ? tp : QMetaType::UnknownType;
}

void copyFileToStream(QFile *file, QTextStream *stream)
{
    file->seek(0);
    QByteArray buffer;
    const int bufferSize = 4096 * 1024;
    buffer.resize(bufferSize);
    while (!file->atEnd()) {
        const int bytesRead = static_cast<int>(file->read(buffer.data(), bufferSize));
        if (bytesRead < bufferSize) {
            buffer.resize(bytesRead);
            *stream << buffer;
            buffer.resize(bufferSize);
        } else {
            *stream << buffer;
        }
    }
}

void generateTypeInfo(QTextStream &out, const QByteArray &typeName)
{
    if (QtPrivate::isBuiltinType(typeName)) {
        int type;
        QByteArray valueString;
        if (typeName == "qreal") {
            type = QMetaType::UnknownType;
            valueString = "QReal";
        } else {
            type = nameToBuiltinType(typeName);
            valueString = metaTypeEnumValueString(type);
        }
        if (!valueString.isEmpty()) {
            out << "QMetaType::" << valueString;
        } else {
            Q_ASSERT(type != QMetaType::UnknownType);
            out << type;
        }
    } else {
        Q_ASSERT(!typeName.isEmpty());
        out << "0x80000000 | " << stridx(typeName);
    }
}
// End functions copied from moc generator

void generateMethods(QTextStream &out, const QMetaObject *mo, const QMetaMethod::MethodType funcType, int &paramsIndex)
{
    out << "// ";
    MethodFlags funcTypeFlag;
    if (funcType == QMetaMethod::Signal) {
        out << "signal";
        funcTypeFlag = MethodSignal;
    } else {
        out << "slot";
        funcTypeFlag = MethodSlot;
    }
    out  << ": name, argc, parameters, tag, flags" << endl;

    int methodCount = mo->methodCount();
    for (int i = mo->methodOffset(); i < methodCount; ++i) {
        const QMetaMethod method(mo->method(i));
        if (method.methodType() != funcType)
            continue;
        out << "    ";
        addStringIdx(method.name());
        out << method.parameterCount() << ", ";
        out << paramsIndex << ", ";
        addStringIdx(method.tag());
        out << (AccessProtected | method.attributes() | funcTypeFlag) << ',' << endl;
        paramsIndex += 1 + method.parameterCount() * 2;
    }
    out << endl;
}

void generateMethodParameters(QTextStream &out, const QMetaObject *mo, const QMetaMethod::MethodType funcType)
{
    out << "// ";
    if (funcType == QMetaMethod::Signal)
        out << "signal";
    else if (funcType == QMetaMethod::Slot)
        out << "slot";
    out  << ": parameters" << endl;

    int methodCount = mo->methodCount();
    for (int i = mo->methodOffset(); i < methodCount; ++i) {
        const QMetaMethod method(mo->method(i));
        if (method.methodType() != funcType)
            continue;

        out << "    ";

        int argsCount = method.parameterCount();

        // Return type
        generateTypeInfo(out, method.typeName());
        out << ',';

        // Parameter types
        const QList<QByteArray> parameterTypes = method.parameterTypes();
        for (int j = 0; j < argsCount; ++j) {
            out << ' ';
            generateTypeInfo(out, parameterTypes.at(j));
            out << ',';
        }

        // Parameter names
        const QList<QByteArray> parameterNames = method.parameterNames();
        for (int j = 0; j < argsCount; ++j)
            out << ' ' << stridx(parameterNames.at(j)) << ',';

        out << endl;
    }
    out << endl;
}

void generateClassImpl(QTextStream &out, const QMetaObject *mo, const QByteArray &className,
                       const QByteArray &nameSpace, ObjectCategories category)
{
    Q_STATIC_ASSERT_X(QMetaObjectPrivate::OutputRevision == 7, "dumpcpp should generate the same version as moc");

    QByteArray qualifiedClassName;
    if (!nameSpace.isEmpty())
        qualifiedClassName = nameSpace + "::";
    qualifiedClassName += className;
    QByteArray qualifiedClassNameIdentifier = qualifiedClassName;
    qualifiedClassNameIdentifier.replace(':', '_');

    int allClassInfoCount = mo->classInfoCount();
    int allMethodCount = mo->methodCount();
    int allPropertyCount = mo->propertyCount();
    int allEnumCount = mo->enumeratorCount();

    int thisClassInfoCount = allClassInfoCount - mo->classInfoOffset();
    int thisEnumCount = allEnumCount - mo->enumeratorOffset();
    int thisMethodCount = allMethodCount - mo->methodOffset();
    int thisPropertyCount = allPropertyCount - mo->propertyOffset();

    int signalCount = 0;
    int slotCount = 0;
    int combinedParameterCount = 0;
    int enumStart = MetaObjectPrivateFieldCount;

    // Register strings
    strreg(qualifiedClassName);
    for (int i = mo->classInfoOffset(); i < allClassInfoCount; ++i) {
        const QMetaClassInfo classInfo = mo->classInfo(i);
        strreg(classInfo.name());
        strreg(classInfo.value());
    }
    for (int i = mo->methodOffset(); i < allMethodCount; ++i) {
        const QMetaMethod method(mo->method(i));
        if (method.methodType() == QMetaMethod::Signal)
            signalCount++;
        if (method.methodType() == QMetaMethod::Slot)
            slotCount++;
        int argsCount = method.parameterCount();
        combinedParameterCount += argsCount;

        strDetachAndRegister(method.name());
        QByteArray typeName = method.typeName();
        if (!QtPrivate::isBuiltinType(typeName))
            strreg(typeName);
        strreg(method.tag());

        const QList<QByteArray> parameterNames = method.parameterNames();
        const QList<QByteArray> parameterTypes = method.parameterTypes();
        for (int j = 0; j < argsCount; ++j) {
            if (!QtPrivate::isBuiltinType(parameterTypes.at(j)))
                strDetachAndRegister(parameterTypes.at(j));
            strDetachAndRegister(parameterNames.at(j));
        }
    }
    for (int i = mo->propertyOffset(); i < allPropertyCount; ++i) {
        const QMetaProperty property = mo->property(i);
        strreg(property.name());
        if (!QtPrivate::isBuiltinType(property.typeName()))
            strreg(property.typeName());
    }
    for (int i = mo->enumeratorOffset(); i < allEnumCount; ++i) {
        const QMetaEnum enumerator = mo->enumerator(i);
        strreg(enumerator.name());
        for (int j = 0; j < enumerator.keyCount(); ++j)
            strreg(enumerator.key(j));
    }

    // Build data array
    out << "static const uint qt_meta_data_" << qualifiedClassNameIdentifier << "[] = {" << endl;
    out << endl;
    out << " // content:" << endl;
    out << "    7, // revision" << endl;
    out << "    ";
    addStringIdx(qualifiedClassName);
    out << " // classname" << endl;
    out << "    " << thisClassInfoCount << ", " << (thisClassInfoCount ? enumStart : 0) << ", // classinfo" << endl;
    enumStart += thisClassInfoCount * 2;
    out << "    " << thisMethodCount << ", " << (thisMethodCount ? enumStart : 0) << ", // methods" << endl;
    enumStart += thisMethodCount * 5;
    int paramsIndex = enumStart;
    enumStart += (combinedParameterCount * 2); // parameter types + names
    enumStart += thisMethodCount; // return types
    out << "    " << thisPropertyCount << ", " << (thisPropertyCount ? enumStart : 0) << ", // properties" << endl;
    enumStart += thisPropertyCount * 3;
    out << "    " << thisEnumCount << ", " << (thisEnumCount ? enumStart : 0) << ", // enums/sets" << endl;
    out << "    0, 0, // constructors" << endl;
    out << "    0, // flags" << endl;
    out << "    " << signalCount << ", // signal count" << endl;
    out << endl;

    if (thisClassInfoCount) {
        out << " // classinfo: key, value" << endl;
        for (int i = mo->classInfoOffset(); i < allClassInfoCount; ++i) {
            QMetaClassInfo classInfo = mo->classInfo(i);
            out << "    ";
            addStringIdx(classInfo.name());
            addStringIdx(classInfo.value());
            out << endl;
        }
        out << endl;
    }

    // Signal/Slot arrays
    if (signalCount)
        generateMethods(out, mo, QMetaMethod::Signal, paramsIndex);
    if (slotCount)
        generateMethods(out, mo, QMetaMethod::Slot, paramsIndex);

    // Method parameter arrays
    if (signalCount)
        generateMethodParameters(out, mo, QMetaMethod::Signal);
    if (slotCount)
        generateMethodParameters(out, mo, QMetaMethod::Slot);

    if (thisPropertyCount) {
        out << " // properties: name, type, flags" << endl;
        for (int i = mo->propertyOffset(); i < allPropertyCount; ++i) {
            QMetaProperty property = mo->property(i);
            out << "    ";
            addStringIdx(property.name());
            generateTypeInfo(out, property.typeName());
            out << ", ";

            uint flags = 0;
            uint vartype = property.type();
            if (vartype != QVariant::Invalid && vartype != QVariant::UserType)
                flags = vartype << 24;

            if (property.isReadable())
                flags |= Readable;
            if (property.isWritable())
                flags |= Writable;
            if (property.isEnumType())
                flags |= EnumOrFlag;
            if (property.isDesignable())
                flags |= Designable;
            if (property.isScriptable())
                flags |= Scriptable;
            if (property.isStored())
                flags |= Stored;
            if (property.isEditable())
                flags |= Editable;

            out << "0x" << QString::number(flags, 16).rightJustified(8, QLatin1Char('0'))
                << ", \t\t // " << property.typeName() << ' ' << property.name()
                << endl;
        }
        out << endl;
    }

    if (thisEnumCount) {
        out << " // enums: name, flags, count, data" << endl;
        enumStart += thisEnumCount * 4;
        for (int i = mo->enumeratorOffset(); i < allEnumCount; ++i) {
            QMetaEnum enumerator = mo->enumerator(i);
            out << "    ";
            addStringIdx(enumerator.name());
            out << (enumerator.isFlag() ? "0x1" : "0x0") << ", " << enumerator.keyCount() << ", " << enumStart << ", " << endl;
            enumStart += enumerator.keyCount() * 2;
        }
        out << endl;

        out << " // enum data: key, value" << endl;
        for (int i = mo->enumeratorOffset(); i < allEnumCount; ++i) {
            QMetaEnum enumerator = mo->enumerator(i);
            for (int j = 0; j < enumerator.keyCount(); ++j) {
                out << "    ";
                addStringIdx(enumerator.key(j));
                out << "uint(";
                if (nameSpace.isEmpty())
                    out << className << "::";
                else
                    out << nameSpace << "::";
                out << enumerator.key(j) << ")," << endl;
            }
        }
    }
    out << "    0 // eod" << endl;
    out << "};" << endl;
    out << endl;


    out << "const QMetaObject " << className << "::staticMetaObject = {" << endl;
    if (category & ActiveX)
        out << "{ &QWidget::staticMetaObject," << endl;
    else
        out << "{ &QObject::staticMetaObject," << endl;
    out << "qt_meta_stringdata_all.data," << endl;
    out << "qt_meta_data_" << qualifiedClassNameIdentifier << ", 0, 0, 0 }" << endl;
    out << "};" << endl;
    out << endl;

    out << "void *" << className << "::qt_metacast(const char *_clname)" << endl;
    out << '{' << endl;
    out << "    if (!_clname) return 0;" << endl;
    out << "    if (!strcmp(_clname, \"" << qualifiedClassName << "\"))" << endl;
    out << "        return static_cast<void*>(const_cast<" << className << "*>(this));" << endl;
    if (category & ActiveX)
        out << "    return QAxWidget::qt_metacast(_clname);" << endl;
    else
        out << "    return QAxObject::qt_metacast(_clname);" << endl;
    out << '}' << endl;
}

static void formatCommentBlockFooter(const QString &typeLibFile, QTextStream &str)
{
    str << " generated by dumpcpp v" << QT_VERSION_STR << " using\n**";
    foreach (const QString &arg, QCoreApplication::arguments())
        str << ' ' << arg;
    str << "\n** from the type library " << typeLibFile << "\n**\n"
        << "****************************************************************************/\n\n";
}

static QByteArray classNameFromTypeInfo(ITypeInfo *typeinfo)
{
    BSTR bstr;
    QByteArray result;
    if (SUCCEEDED(typeinfo->GetDocumentation(-1, &bstr, 0, 0, 0))) {
        result = QString::fromWCharArray(bstr).toLatin1();
        SysFreeString(bstr);
    }
    return result;
}

// Extract a list of VTable only stubs from a ITypeLib.
static QByteArrayList vTableOnlyStubsFromTypeLib(ITypeLib *typelib, const QString &nameSpace)
{
    QByteArrayList result;
    const QByteArray nameSpacePrefix = nameSpace.toLatin1() + "::";
    for (UINT i = 0, typeCount = typelib->GetTypeInfoCount(); i < typeCount; ++i) {
        TYPEKIND typekind;
        if (SUCCEEDED(typelib->GetTypeInfoType(i, &typekind)) && typekind == TKIND_INTERFACE) {
            ITypeInfo *typeinfo = Q_NULLPTR;
            if (SUCCEEDED(typelib->GetTypeInfo(i, &typeinfo) && typeinfo)) {
                result.append(nameSpacePrefix + classNameFromTypeInfo(typeinfo));
                typeinfo->Release();
            }
        }
    }
    return result;
}

bool generateTypeLibrary(QString typeLibFile, QString outname,
                         const QString &nameSpace, ObjectCategories category)
{
    typeLibFile.replace(QLatin1Char('/'), QLatin1Char('\\'));

    ITypeLib *typelib;
    LoadTypeLibEx(reinterpret_cast<const wchar_t *>(typeLibFile.utf16()), REGKIND_NONE, &typelib);
    if (!typelib) {
        qWarning("dumpcpp: loading '%s' as a type library failed", qPrintable(typeLibFile));
        return false;
    }

    QString libName = nameSpace;
    if (libName.isEmpty()) {
        BSTR nameString = Q_NULLPTR;
        if (SUCCEEDED(typelib->GetDocumentation(-1, &nameString, 0, 0, 0))) {
            libName = QString::fromWCharArray(nameString);
            SysFreeString(nameString);
        }
    }
    vTableOnlyStubs = vTableOnlyStubsFromTypeLib(typelib, libName);

    QString libVersion(QLatin1String("1.0"));

    TLIBATTR *tlibattr = 0;
    typelib->GetLibAttr(&tlibattr);
    if (tlibattr) {
        libVersion = QString::fromLatin1("%1.%2").arg(tlibattr->wMajorVerNum).arg(tlibattr->wMinorVerNum);
        typelib->ReleaseTLibAttr(tlibattr);
    }

    if (outname.isEmpty())
        outname = libName.toLower();

    if (outname.isEmpty()) {
        qWarning("dumpcpp: no output filename provided, and cannot deduce output filename");
        return false;
    }

    QMetaObject *namespaceObject = qax_readEnumInfo(typelib, 0);

    QTemporaryFile classImplFile;
    if (!classImplFile.open()) {
        qWarning("dumpcpp: Cannot open temporary file.");
        return false;
    }
    QTextStream classImplOut(&classImplFile);
    QFile implFile(outname + QLatin1String(".cpp"));
    QTextStream implOut(&implFile);
    if (!(category & (NoMetaObject|NoImplementation))) {
        if (!implFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("dumpcpp: Could not open output file '%s'", qPrintable(implFile.fileName()));
            return false;
        }

        implOut << "/****************************************************************************\n"
                   "**\n** Metadata for " << libName;
        formatCommentBlockFooter(typeLibFile, implOut);

        implOut << "#define QAX_DUMPCPP_" << libName.toUpper() << "_NOINLINES" << endl;

        implOut << "#include \"" << outname << ".h\"" << endl;
        implOut << endl;
        implOut << "using namespace " << libName << ';' << endl;
        implOut << endl;
    }

    QFile declFile(outname + QLatin1String(".h"));
    QTextStream declOut(&declFile);
    QByteArray classes;
    QTextStream classesOut(&classes, QIODevice::WriteOnly);
    QByteArray inlines;
    QTextStream inlinesOut(&inlines, QIODevice::WriteOnly);

    QMap<QByteArray, QList<QByteArray> > namespaces;

    if(!(category & NoDeclaration)) {
        if (!declFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("dumpcpp: Could not open output file '%s'", qPrintable(declFile.fileName()));
            return false;
        }

        declOut << "/****************************************************************************\n"
                   "**\n** Namespace " << libName;
        formatCommentBlockFooter(typeLibFile, declOut);

        QFileInfo cppFileInfo(outname);
        writeHeader(declOut, libName, cppFileInfo.fileName());

        UINT typeCount = typelib->GetTypeInfoCount();
        if (declFile.isOpen()) {
            QByteArrayList opaquePointerTypes;
            declOut << endl;
            declOut << "// Referenced namespace" << endl;
            for (UINT index = 0; index < typeCount; ++index) {
                ITypeInfo *typeinfo = 0;
                typelib->GetTypeInfo(index, &typeinfo);
                if (!typeinfo)
                    continue;

                TYPEATTR *typeattr;
                typeinfo->GetTypeAttr(&typeattr);
                if (!typeattr) {
                    typeinfo->Release();
                    continue;
                }

                TYPEKIND typekind;
                typelib->GetTypeInfoType(index, &typekind);

                QMetaObject *metaObject = 0;

                // trigger meta object to collect references to other type libraries
                switch (typekind) {
                case TKIND_COCLASS:
                    if (category & ActiveX)
                        metaObject = qax_readClassInfo(typelib, typeinfo, &QWidget::staticMetaObject);
                    else
                        metaObject = qax_readClassInfo(typelib, typeinfo, &QObject::staticMetaObject);
                    break;
                case TKIND_DISPATCH:
                    if (category & ActiveX)
                        metaObject = qax_readInterfaceInfo(typelib, typeinfo, &QWidget::staticMetaObject);
                    else
                        metaObject = qax_readInterfaceInfo(typelib, typeinfo, &QObject::staticMetaObject);
                    break;
                case TKIND_RECORD:
                case TKIND_ENUM:
                case TKIND_INTERFACE: // only for forward declarations
                    {
                        QByteArray className = classNameFromTypeInfo(typeinfo);
                        switch (typekind) {
                        case TKIND_RECORD:
                            className.prepend("struct ");
                            break;
                        case TKIND_ENUM:
                            className.prepend("enum ");
                            break;
                        default:
                            break;
                        }
                        namespaces[libName.toLatin1()].append(className);
                        if (!qax_qualified_usertypes.contains(className))
                            qax_qualified_usertypes << className;
                    }
                    break;
                default:
                    break;
                }

                qax_deleteMetaObject(metaObject);
                typeinfo->ReleaseTypeAttr(typeattr);
                typeinfo->Release();
            }

            for (int i = 0; i < qax_qualified_usertypes.count(); ++i) {
                QByteArray refType = qax_qualified_usertypes.at(i);
                QByteArray refTypeLib;
                if (refType.contains("::")) {
                    refTypeLib = refType;
                    refType.remove(0, refType.lastIndexOf("::") + 2);
                    if (refTypeLib.contains(' ')) {
                        refType = refTypeLib.left(refTypeLib.indexOf(' ')) + ' ' + refType;
                    }
                    refTypeLib.truncate(refTypeLib.indexOf("::"));
                    refTypeLib.remove(0, refTypeLib.lastIndexOf(' ') + 1);
                    namespaces[refTypeLib].append(refType);
                } else {
                    namespaces[libName.toLatin1()].append(refType);
                }
            }

            QList<QByteArray> keys = namespaces.keys();
            for (int n = 0; n < keys.count(); ++n) {
                QByteArray nspace = keys.at(n);
                if (QString::fromLatin1(nspace.constData()) != libName) {
                    declOut << "namespace " << nspace << " {" << endl;
                    QList<QByteArray> classList = namespaces.value(nspace);
                    for (int c = 0; c < classList.count(); ++c) {
                        QByteArray className = classList.at(c);
                        if (className.contains(' ')) {
                            declOut << "    " << className << ';' << endl;
                            namespaceForType.insert(className.mid(className.indexOf(' ') + 1), nspace);
                        } else {
                            declOut << "    class " << className << ';' << endl;
                            opaquePointerTypes.append(nspace + "::" + className);
                            namespaceForType.insert(className, nspace);
                            namespaceForType.insert(className + '*', nspace);
                            namespaceForType.insert(className + "**", nspace);
                        }
                    }
                    declOut << '}' << endl << endl;
                }
            }
            foreach (const QByteArray &opaquePointerType, opaquePointerTypes)
                declOut << "Q_DECLARE_OPAQUE_POINTER(" << opaquePointerType << "*)" << endl;
            declOut << endl;
        }
        generateNameSpace(declOut, namespaceObject, libName.toLatin1());

        QList<QByteArray> classList = namespaces.value(libName.toLatin1());
        if (classList.count())
            declOut << "// forward declarations" << endl;
        for (int c = 0; c < classList.count(); ++c) {
            QByteArray className = classList.at(c);
            if (className.contains(' ')) {
                declOut << "    " << className << ';' << endl;
                namespaceForType.insert(className.mid(className.indexOf(' ') + 1), libName.toLatin1());
            } else {
                declOut << "    class " << className << ';' << endl;
                namespaceForType.insert(className, libName.toLatin1());
                namespaceForType.insert(className + '*', libName.toLatin1());
                namespaceForType.insert(className + "**", libName.toLatin1());
            }
        }

        declOut << endl;
    }

    QList<QByteArray> subtypes;

    UINT typeCount = typelib->GetTypeInfoCount();
    for (UINT index = 0; index < typeCount; ++index) {
        ITypeInfo *typeinfo = 0;
        typelib->GetTypeInfo(index, &typeinfo);
        if (!typeinfo)
            continue;

        TYPEATTR *typeattr;
        typeinfo->GetTypeAttr(&typeattr);
        if (!typeattr) {
            typeinfo->Release();
            continue;
        }

        TYPEKIND typekind;
        typelib->GetTypeInfoType(index, &typekind);

        ObjectCategories object_category = category;
        if (!(typeattr->wTypeFlags & TYPEFLAG_FCANCREATE))
            object_category |= SubObject;
        else if (typeattr->wTypeFlags & TYPEFLAG_FCONTROL)
            object_category |= ActiveX;

        QMetaObject *metaObject = 0;
        QUuid guid(typeattr->guid);

        if (!(object_category & ActiveX)) {
            QSettings settings(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Classes\\CLSID\\") + guid.toString(), QSettings::NativeFormat);
            if (settings.childGroups().contains(QLatin1String("Control"))) {
                object_category |= ActiveX;
                object_category &= ~SubObject;
            }
        }

        switch (typekind) {
        case TKIND_COCLASS:
            if (object_category & ActiveX)
                metaObject = qax_readClassInfo(typelib, typeinfo, &QWidget::staticMetaObject);
            else
                metaObject = qax_readClassInfo(typelib, typeinfo, &QObject::staticMetaObject);
            break;
        case TKIND_DISPATCH:
            if (object_category & ActiveX)
                metaObject = qax_readInterfaceInfo(typelib, typeinfo, &QWidget::staticMetaObject);
            else
                metaObject = qax_readInterfaceInfo(typelib, typeinfo, &QObject::staticMetaObject);
            break;
        case TKIND_INTERFACE: { // only stub: QTBUG-27792, explicitly disable copy in inherited
                                // class to make related error messages clearer
                const QByteArray className = classNameFromTypeInfo(typeinfo);
                declOut << "// stub for vtable-only interface\n"
                    << "class " << className << " : public QAxObject { Q_DISABLE_COPY("
                    << className << ") };\n\n";
            }
            break;
        default:
            break;
        }

        if (metaObject) {
            currentTypeInfo = typeinfo;
            QByteArray className(metaObject->className());
            if (!(typeattr->wTypeFlags & TYPEFLAG_FDUAL)
                && (metaObject->propertyCount() - metaObject->propertyOffset()) == 1
                && className.contains("Events")) {
                declOut << "// skipping event interface " << className << endl << endl;
            } else {
                if (declFile.isOpen()) {
                    if (typeattr->wTypeFlags & TYPEFLAG_FLICENSED)
                        object_category |= Licensed;
                    if (typekind == TKIND_COCLASS) { // write those later...
                        generateClassDecl(classesOut, guid.toString(), metaObject, className, libName.toLatin1(),
                                          object_category | NoInlines);
                        classesOut << endl;
                    } else {
                        generateClassDecl(declOut, guid.toString(), metaObject, className, libName.toLatin1(),
                                          object_category | NoInlines);
                        declOut << endl;
                    }
                    subtypes << className;
                    generateClassDecl(inlinesOut, guid.toString(), metaObject, className, libName.toLatin1(),
                                      object_category | OnlyInlines);
                    inlinesOut << endl;
                }
                if (implFile.isOpen())
                    generateClassImpl(classImplOut, metaObject, className, libName.toLatin1(),
                                      object_category);
            }
            currentTypeInfo = 0;
        }

        qax_deleteMetaObject(metaObject);

        typeinfo->ReleaseTypeAttr(typeattr);
        typeinfo->Release();
    }

    // String table generation logic was ported from moc generator, with some modifications
    // required to split large stringdata arrays.
    if (!strings.isEmpty() && implFile.isOpen()) {
        //
        // Build stringdata struct
        //
        implOut << "struct qt_meta_stringdata_all_t {" << endl;
        implOut << "    QByteArrayData data[" << strings.size() << "];" << endl;

        QVector<QList<QByteArray> > listVector;
        QList<QByteArray> currentList;

        int currentTableLen = 0;
        for (int i = 0; i < strings.size(); ++i) {
            currentTableLen += strings.at(i).length() + 1;
            currentList.append(strings.at(i));
            // Split strings into chunks less than 64k to work around compiler limits.
            if (currentTableLen > 60000) {
                implOut << "    char stringdata" << listVector.size() << '[' << currentTableLen + 1 << "];" << endl;
                listVector.append(currentList);
                currentList.clear();
                currentTableLen = 0;
            }
        }
        implOut << "    char stringdata" << listVector.size() << '[' << currentTableLen + 1 << "];" << endl;
        implOut << "};" << endl;
        listVector.append(currentList);

        // Macro that expands into a QByteArrayData. The offset member is
        // calculated from 1) the offset of the actual characters in the
        // stringdata.stringdata member, and 2) the stringdata.data index of the
        // QByteArrayData being defined. This calculation relies on the
        // QByteArrayData::data() implementation returning simply "this + offset".
        implOut << "#define QT_MOC_LITERAL(idx, ofs, len, table) \\" << endl
            << "    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \\" << endl
            << "    offsetof(qt_meta_stringdata_all_t, stringdata##table) + ofs \\" << endl
            << "        - idx * sizeof(QByteArrayData) \\" << endl
            << "    )" << endl;

        implOut << "static const qt_meta_stringdata_all_t qt_meta_stringdata_all = {" << endl;
        implOut << "    {" << endl;

        int totalStringCount = 0;
        for (int i = 0; i < listVector.size(); ++i) {
            int idx = 0;
            for (int j = 0; j < listVector[i].size(); j++) {
                if (totalStringCount)
                    implOut << ',' << endl;
                const QByteArray &str = listVector[i].at(j);
                implOut << "QT_MOC_LITERAL(" << totalStringCount++ << ", " << idx << ", " << str.length() << ", " << i << ')';
                idx += str.length() + 1;
            }
        }
        implOut << endl << "    }";

        //
        // Build stringdata arrays
        //
        for (int i = 0; i < listVector.size(); ++i) {
            int col = 0;
            int len = 0;
            implOut << ',' << endl;
            implOut << "    \"";
            for (int j = 0; j < listVector[i].size(); ++j) {
                QByteArray s = listVector[i].at(j);
                len = s.length();
                if (col && col + len >= 150) {
                    implOut << '"' << endl << "    \"";
                    col = 0;
                } else if (len && s.at(0) >= '0' && s.at(0) <= '9') {
                    implOut << "\"\"";
                    len += 2;
                }
                int idx = 0;
                while (idx < s.length()) {
                    if (idx > 0) {
                        col = 0;
                        implOut << '"' << endl << "    \"";
                    }
                    int spanLen = qMin(150, s.length() - idx);
                    implOut << s.mid(idx, spanLen);
                    idx += spanLen;
                    col += spanLen;
                }

                implOut << "\\0";
                col += len + 2;
            }
            implOut << '"';
        }
        // Terminate stringdata struct
        implOut << endl << "};" << endl;

        implOut << "#undef QT_MOC_LITERAL" << endl << endl;

        classImplOut.flush();
        copyFileToStream(&classImplFile, &implOut);
        implOut << endl;
    }

    qax_deleteMetaObject(namespaceObject);

    classesOut.flush();
    inlinesOut.flush();

    if (declFile.isOpen()) {
        if (classes.size()) {
            declOut << "// Actual coclasses" << endl;
            declOut << classes;
        }
        if (inlines.size()) {
            declOut << "// member function implementation" << endl;
            declOut << "#ifndef QAX_DUMPCPP_" << libName.toUpper() << "_NOINLINES" << endl;
            declOut << inlines << endl;
            declOut << "#endif" << endl << endl;
        }
        // close namespace
        declOut << '}' << endl;
        declOut << endl;

        // partial template specialization for qMetaTypeCreateHelper and qMetaTypeConstructHelper
        declOut << "QT_BEGIN_NAMESPACE" << endl << endl;
        declOut << "namespace QtMetaTypePrivate {" << endl;
        for (int t = 0; t < subtypes.count(); ++t) {
            QByteArray subType(subtypes.at(t));

            declOut << "template<>" << endl;
            declOut << "struct QMetaTypeFunctionHelper<" << libName << "::" << subType << ", /* Accepted */ true> {" << endl;

            declOut << "    static void Destruct(void *t)" << endl;
            declOut << "    {" << endl;
            declOut << "        Q_UNUSED(t)" << endl; // Silence MSVC that warns for POD types.
            declOut << "        static_cast<" << libName << "::" << subType << "*>(t)->" << libName << "::" << subType << "::~" << subType << "();" << endl;
            declOut << "    }" << endl;

            declOut << "    static void *Construct(void *where, const void *t)" << endl;
            declOut << "    {" << endl;
            declOut << "        Q_ASSERT(!t);" << endl;
            declOut << "        Q_UNUSED(t)" << endl; // Silence warnings for release builds
            declOut << "        return new (where) " << libName << "::" << subType << ';' << endl;
            declOut << "    }" << endl;

            declOut << "#ifndef QT_NO_DATASTREAM" << endl;

            declOut << "    static void Save(QDataStream &stream, const void *t) { stream << *static_cast<const " << libName << "::" << subType << "*>(t); }" << endl;
            declOut << "    static void Load(QDataStream &stream, void *t) { stream >> *static_cast<" << libName << "::" << subType << "*>(t); }" << endl;

            declOut << "#endif // QT_NO_DATASTREAM" << endl;

            declOut << "};" << endl << endl;
        }
        declOut << "} // namespace QtMetaTypePrivate" << endl;
        declOut << "QT_END_NAMESPACE" << endl << endl;
        declOut << "#endif" << endl;
        declOut << endl;
    }

    typelib->Release();
    return true;
}

QT_END_NAMESPACE

QT_USE_NAMESPACE

struct Options
{
    Options() : mode(GenerateMode), category(DefaultObject), dispatchEqualsIDispatch(false) {}

    ProgramMode mode;
    ObjectCategories category;
    bool dispatchEqualsIDispatch;

    QString outname;
    QString typeLib;
    QString nameSpace;
};

static void parseOptions(Options *options)
{
    const char helpText[] =
        "\nGenerate a C++ namespace from a type library.\n\n"
        "Examples:\n"
        "   dumpcpp -o ieframe %WINDIR%\\system32\\ieframe.dll\n"
        "   dumpcpp -o outlook Outlook.Application\n"
        "   dumpcpp {3B756301-0075-4E40-8BE8-5A81DE2426B7}\n"
        "   dumpcpp -getfile {21D6D480-A88B-11D0-83DD-00AA003CCABD}\n";

    const char outputOptionC[] = "-o";
    const char nameSpaceOptionC[] = "-n";
    const char getfileOptionC[] = "-getfile";

    QStringList args = QCoreApplication::arguments();
    // Convert Windows-style '/option' into '-option'.
    for (int i = 1; i < args.size(); ) {
        QString &arg = args[i];
        if (arg.startsWith(QLatin1Char('/')))
            arg[0] = QLatin1Char('-');
        const bool takesOptionValue = arg == QLatin1String(outputOptionC)
            || arg == QLatin1String(nameSpaceOptionC)
            || arg == QLatin1String(getfileOptionC);
        i += takesOptionValue ? 2 : 1;
    }

    QCommandLineParser parser;
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    parser.setApplicationDescription(QLatin1String(helpText));


    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption outputOption(QLatin1String(outputOptionC + 1),
                                    QStringLiteral("Write output to file."),
                                    QStringLiteral("file"));
    parser.addOption(outputOption);
    QCommandLineOption nameSpaceOption(QLatin1String(nameSpaceOptionC + 1),
                                       QStringLiteral("The name of the generated C++ namespace."),
                                       QStringLiteral("namespace"));
    parser.addOption(nameSpaceOption);
    QCommandLineOption noMetaObjectOption(QStringLiteral("nometaobject"),
                                          QStringLiteral("Don't generate meta object information (no .cpp file). The meta object is then generated in runtime."));
    parser.addOption(noMetaObjectOption);
    QCommandLineOption noDeclarationOption(QStringLiteral("impl"),
                                           QStringLiteral("Only generate the .cpp file."));
    parser.addOption(noDeclarationOption);
    QCommandLineOption noImplementationOption(QStringLiteral("decl"),
                                              QStringLiteral("Only generate the .h file."));
    parser.addOption(noImplementationOption);
    QCommandLineOption compatOption(QStringLiteral("compat"),
                                    QStringLiteral("Treat all coclass parameters as IDispatch."));
    parser.addOption(compatOption);
    QCommandLineOption getFileOption(QLatin1String(getfileOptionC + 1),
                                     QStringLiteral("Print the filename for the type library it to standard output."),
                                     QStringLiteral("id"));
    parser.addOption(getFileOption);
    parser.addPositionalArgument(QStringLiteral("input"),
                                 QStringLiteral("A type library file, type library ID, ProgID or CLSID."));
    parser.process(args);

    if (parser.isSet(outputOption))
        options->outname = parser.value(outputOption);
    if (parser.isSet(nameSpaceOption))
        options->nameSpace = parser.value(nameSpaceOption);
    if (parser.isSet(noMetaObjectOption))
         options->category |= NoMetaObject;
    if (parser.isSet(noDeclarationOption))
        options->category |= NoDeclaration;
    if (parser.isSet(noImplementationOption))
        options->category |= NoImplementation;
    options->dispatchEqualsIDispatch = parser.isSet(compatOption);
    if (parser.isSet(getFileOption)) {
        options->typeLib = parser.value(getFileOption);
        options->mode = TypeLibID;
    }
    if (!parser.positionalArguments().isEmpty())
        options->typeLib = parser.positionalArguments().first();

    if (options->mode == GenerateMode && options->typeLib.isEmpty()) {
        qWarning("dumpcpp: No object class or type library name provided.\n");
        parser.showHelp(1);
    }
}

int main(int argc, char **argv)
{
    if (FAILED(CoInitialize(0))) {
        qErrnoWarning("CoInitialize() failed.");
        return -1;
    }
    QCoreApplication app(argc, argv);

    Options options;
    parseOptions(&options);
    qax_dispatchEqualsIDispatch = options.dispatchEqualsIDispatch;
    QString typeLib = options.typeLib;

    if (options.mode == TypeLibID) {
        QSettings settings(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Classes\\TypeLib\\") +
                           typeLib, QSettings::NativeFormat);
        typeLib.clear();
        QStringList codes = settings.childGroups();
        for (int c = 0; c < codes.count(); ++c) {
            typeLib = settings.value(QLatin1Char('/') + codes.at(c) + QLatin1String("/0/win32/.")).toString();
            if (QFile::exists(typeLib))
                break;
        }

        if (!typeLib.isEmpty())
            fprintf(stdout, "\"%s\"\n", qPrintable(typeLib));
        return 0;
    }

    if (typeLib.isEmpty()) {
        qWarning("dumpcpp: No object class or type library name provided.\n"
            "         Use -h for help.");
        return -1;
    }

    // not a file - search registry
    if (!QFile::exists(typeLib)) {
        bool isObject = false;
        QSettings settings(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Classes"), QSettings::NativeFormat);

        // regular string and not a file - must be ProgID
        if (typeLib.at(0) != QLatin1Char('{')) {
            CLSID clsid;
            if (CLSIDFromProgID(reinterpret_cast<const wchar_t *>(typeLib.utf16()), &clsid) != S_OK) {
                qWarning("dumpcpp: '%s' is not a type library and not a registered ProgID",
                         qPrintable(typeLib));
                return -2;
            }
            typeLib = QUuid(clsid).toString();
            isObject = true;
        }

        // check if CLSID
        if (!isObject) {
            QVariant test = settings.value(QLatin1String("/CLSID/") + typeLib + QLatin1String("/."));
            isObject = test.isValid();
        }

        // search typelib ID for CLSID
        if (isObject)
            typeLib = settings.value(QLatin1String("/CLSID/") + typeLib
                                     + QLatin1String("/Typelib/.")).toString();

        // interpret input as type library ID
        QString key = QLatin1String("/TypeLib/") + typeLib;
        settings.beginGroup(key);
        QStringList versions = settings.childGroups();
        QStringList codes;
        if (versions.count()) {
            settings.beginGroup(QLatin1Char('/') + versions.last());
            codes = settings.childGroups();
            key += QLatin1Char('/') + versions.last();
            settings.endGroup();
        }
        settings.endGroup();

        for (int c = 0; c < codes.count(); ++c) {
            typeLib = settings.value(key + QLatin1Char('/') + codes.at(c)
                                     + QLatin1String("/win32/.")).toString();
            if (QFile::exists(typeLib))
                break;
        }
    }

    if (!QFile::exists(typeLib)) {
        qWarning("dumpcpp: type library '%s' not found", qPrintable(typeLib));
        return -2;
    }

    if (!generateTypeLibrary(typeLib, options.outname, options.nameSpace, options.category)) {
        qWarning("dumpcpp: error processing type library '%s'", qPrintable(typeLib));
        return -1;
    }

    return 0;
}
