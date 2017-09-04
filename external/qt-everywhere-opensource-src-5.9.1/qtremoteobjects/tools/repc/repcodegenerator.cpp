/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "repcodegenerator.h"

#include "repparser.h"

#include <QFileInfo>
#include <QMetaType>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRegExp>

QT_BEGIN_NAMESPACE

template <typename C>
static int accumulatedSizeOfNames(const C &c)
{
    int result = 0;
    Q_FOREACH (const typename C::value_type &e, c)
        result += e.name.size();
    return result;
}

template <typename C>
static int accumulatedSizeOfTypes(const C &c)
{
    int result = 0;
    Q_FOREACH (const typename C::value_type &e, c)
        result += e.type.size();
    return result;
}

static QString cap(QString name)
{
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

static bool isClassEnum(const ASTClass &classContext, const QString &typeName)
{
    Q_FOREACH (const ASTEnum &astEnum, classContext.enums) {
        if (astEnum.name == typeName) {
            return true;
        }
    }

    return false;
}

static QString fullyQualifiedTypeName(const ASTClass& classContext, const QString &className, const QString &typeName)
{
    if (isClassEnum(classContext, typeName)) {
        // type was defined in this class' context, prefix typeName with class name
        return className + QStringLiteral("::") + typeName;
    }
    return typeName;
}

/*
  Returns \c true if the type is a built-in type.
*/
static bool isBuiltinType(const QString &type)
 {
    int id = QMetaType::type(type.toLatin1().constData());
    if (id == QMetaType::UnknownType)
        return false;
    return (id < QMetaType::User);
}

RepCodeGenerator::RepCodeGenerator(QIODevice *outputDevice)
    : m_outputDevice(outputDevice)
{
    Q_ASSERT(m_outputDevice);
}

static QByteArray enumSignature(const ASTEnum &e)
{
    QByteArray ret;
    ret += e.name.toLatin1();
    Q_FOREACH (const ASTEnumParam &param, e.params)
        ret += param.name.toLatin1() + QByteArray::number(param.value);
    return ret;
}

static QByteArray typeData(const QString &type, const QHash<QString, QByteArray> &specialTypes)
{
    QHash<QString, QByteArray>::const_iterator it = specialTypes.find(type);
    if (it != specialTypes.end())
        return it.value();
    int pos = type.lastIndexOf(QLatin1String("::"));
    if (pos > 0)
            return typeData(type.mid(pos + 2), specialTypes);
    return type.toLatin1();
}

static QByteArray functionsData(const QVector<ASTFunction> &functions, const QHash<QString, QByteArray> &specialTypes)
{
    QByteArray ret;
    Q_FOREACH (const ASTFunction &func, functions) {
        ret += func.name.toLatin1();
        Q_FOREACH (const ASTDeclaration &param, func.params) {
            ret += param.name.toLatin1();
            ret += typeData(param.type, specialTypes);
            ret += QByteArray(reinterpret_cast<const char *>(&param.variableType), sizeof(param.variableType));
        }
        ret += typeData(func.returnType, specialTypes);
    }
    return ret;
}

QByteArray RepCodeGenerator::classSignature(const ASTClass &ac)
{
    QCryptographicHash checksum(QCryptographicHash::Sha1);
    QHash<QString, QByteArray> localTypes = m_globalEnumsPODs;
    Q_FOREACH (const ASTEnum &e, ac.enums) // add local enums
        localTypes[e.name] = enumSignature(e);

    checksum.addData(ac.name.toLatin1());

    // Checksum properties
    Q_FOREACH (const ASTProperty &p, ac.properties) {
        checksum.addData(p.name.toLatin1());
        checksum.addData(typeData(p.type, localTypes));
        checksum.addData(reinterpret_cast<const char *>(&p.modifier), sizeof(p.modifier));
    }

    // Checksum signals
    checksum.addData(functionsData(ac.signalsList, localTypes));

    // Checksum slots
    checksum.addData(functionsData(ac.slotsList, localTypes));

    return checksum.result().toHex();
}

void RepCodeGenerator::generate(const AST &ast, Mode mode, QString fileName)
{
    QTextStream stream(m_outputDevice);
    if (fileName.isEmpty())
        stream << "#pragma once" << endl << endl;
    else {
        fileName = QFileInfo(fileName).fileName();
        fileName = fileName.toUpper();
        fileName.replace(QLatin1Char('.'), QLatin1Char('_'));
        stream << "#ifndef " << fileName << endl;
        stream << "#define " << fileName << endl << endl;
    }

    generateHeader(mode, stream, ast);
    Q_FOREACH (const ASTEnum &en, ast.enums)
        generateENUM(stream, en);
    Q_FOREACH (const POD &pod, ast.pods)
        generatePOD(stream, pod);

    QSet<QString> metaTypes;
    Q_FOREACH (const POD &pod, ast.pods)
        metaTypes << pod.name;
    Q_FOREACH (const ASTClass &astClass, ast.classes) {
        Q_FOREACH (const ASTProperty &property, astClass.properties)
            metaTypes << property.type;
        Q_FOREACH (const ASTFunction &function, astClass.signalsList + astClass.slotsList) {
            metaTypes << function.returnType;
            Q_FOREACH (const ASTDeclaration &decl, function.params) {
                metaTypes << decl.type;
            }
        }
    }

    const QString metaTypeRegistrationCode = generateMetaTypeRegistration(metaTypes)
                                           + generateMetaTypeRegistrationForEnums(ast.enumUses);

    Q_FOREACH (const ASTClass &astClass, ast.classes) {
        if (mode == MERGED) {
            generateClass(REPLICA, stream, astClass, metaTypeRegistrationCode);
            generateClass(SOURCE, stream, astClass, metaTypeRegistrationCode);
            generateClass(SIMPLE_SOURCE, stream, astClass, metaTypeRegistrationCode);
            generateSourceAPI(stream, astClass);
        } else {
            generateClass(mode, stream, astClass, metaTypeRegistrationCode);
            if (mode == SOURCE) {
                generateClass(SIMPLE_SOURCE, stream, astClass, metaTypeRegistrationCode);
                generateSourceAPI(stream, astClass);
            }
        }
    }

    generateStreamOperatorsForEnums(stream, ast.enumUses);

    stream << endl;
    if (!fileName.isEmpty())
        stream << "#endif // " << fileName << endl;
}

void RepCodeGenerator::generateHeader(Mode mode, QTextStream &out, const AST &ast)
{
    out << "// This is an autogenerated file.\n"
           "// Do not edit this file, any changes made will be lost the next time it is generated.\n"
           "\n"
           "#include <QtCore/qobject.h>\n"
           "#include <QtCore/qdatastream.h>\n"
           "#include <QtCore/qvariant.h>\n"
           "#include <QtCore/qmetatype.h>\n";
    bool hasModel = false;
    for (auto c : ast.classes)
    {
        if (c.models.count() > 0)
        {
            hasModel = true;
            break;
        }
    }
    if (hasModel)
        out << "#include <QtCore/qabstractitemmodel.h>\n";
    out << "\n"
           "#include <QtRemoteObjects/qremoteobjectnode.h>\n";

    if (mode == MERGED) {
        out << "#include <QtRemoteObjects/qremoteobjectpendingcall.h>\n";
        out << "#include <QtRemoteObjects/qremoteobjectreplica.h>\n";
        out << "#include <QtRemoteObjects/qremoteobjectsource.h>\n";
        if (hasModel)
            out << "#include <QtRemoteObjects/qremoteobjectabstractitemmodelreplica.h>\n";
    } else if (mode == REPLICA) {
        out << "#include <QtRemoteObjects/qremoteobjectpendingcall.h>\n";
        out << "#include <QtRemoteObjects/qremoteobjectreplica.h>\n";
        if (hasModel)
            out << "#include <QtRemoteObjects/qremoteobjectabstractitemmodelreplica.h>\n";
    } else
        out << "#include <QtRemoteObjects/qremoteobjectsource.h>\n";
    out << "\n";

    out << ast.preprocessorDirectives.join(QLatin1Char('\n'));
    out << "\n";
}

static QString formatTemplateStringArgTypeNameCapitalizedName(int numberOfTypeOccurrences, int numberOfNameOccurrences,
                                                              QString templateString, const POD &pod)
{
    QString out;
    const int LengthOfPlaceholderText = 2;
    Q_ASSERT(templateString.count(QRegExp(QStringLiteral("%\\d"))) == numberOfNameOccurrences + numberOfTypeOccurrences);
    const int expectedOutSize
            = numberOfNameOccurrences * accumulatedSizeOfNames(pod.attributes)
            + numberOfTypeOccurrences * accumulatedSizeOfTypes(pod.attributes)
            + pod.attributes.size() * (templateString.size() - (numberOfNameOccurrences + numberOfTypeOccurrences) * LengthOfPlaceholderText);
    out.reserve(expectedOutSize);
    Q_FOREACH (const PODAttribute &a, pod.attributes)
        out += templateString.arg(a.type, a.name, cap(a.name));
    return out;
}

QString RepCodeGenerator::formatQPropertyDeclarations(const POD &pod)
{
    return formatTemplateStringArgTypeNameCapitalizedName(1, 3, QStringLiteral("    Q_PROPERTY(%1 %2 READ %2 WRITE set%3)\n"), pod);
}

QString RepCodeGenerator::formatConstructors(const POD &pod)
{
    QString initializerString = QStringLiteral(": ");
    QString defaultInitializerString = initializerString;
    QString argString;
    Q_FOREACH (const PODAttribute &a, pod.attributes) {
        initializerString += QString::fromLatin1("_%1(%1), ").arg(a.name);
        defaultInitializerString += QString::fromLatin1("_%1(), ").arg(a.name);
        argString += QString::fromLatin1("%1 %2, ").arg(a.type, a.name);
    }
    argString.chop(2);
    initializerString.chop(2);
    defaultInitializerString.chop(2);

    return QString::fromLatin1("    %1() %2 {}\n"
                   "    explicit %1(%3) %4 {}\n")
            .arg(pod.name, defaultInitializerString, argString, initializerString);
}

QString RepCodeGenerator::formatPropertyGettersAndSetters(const POD &pod)
{
    // MSVC doesn't like adjacent string literal concatenation within QStringLiteral, so keep it in one line:
    QString templateString
            = QStringLiteral("    %1 %2() const { return _%2; }\n    void set%3(%1 %2) { if (%2 != _%2) { _%2 = %2; } }\n");
    return formatTemplateStringArgTypeNameCapitalizedName(2, 8, qMove(templateString), pod);
}

QString RepCodeGenerator::formatDataMembers(const POD &pod)
{
    QString out;
    const QString prefix = QStringLiteral("    ");
    const QString infix  = QStringLiteral(" _");
    const QString suffix = QStringLiteral(";\n");
    const int expectedOutSize
            = accumulatedSizeOfNames(pod.attributes)
            + accumulatedSizeOfTypes(pod.attributes)
            + pod.attributes.size() * (prefix.size() + infix.size() + suffix.size());
    out.reserve(expectedOutSize);
    Q_FOREACH (const PODAttribute &a, pod.attributes) {
        out += prefix;
        out += a.type;
        out += infix;
        out += a.name;
        out += suffix;
    }
    Q_ASSERT(out.size() == expectedOutSize);
    return out;
}

QString RepCodeGenerator::formatMarshallingOperators(const POD &pod)
{
    return QLatin1String("inline QDataStream &operator<<(QDataStream &ds, const ") + pod.name + QLatin1String(" &obj) {\n"
           "    QtRemoteObjects::copyStoredProperties(&obj, ds);\n"
           "    return ds;\n"
           "}\n"
           "\n"
           "inline QDataStream &operator>>(QDataStream &ds, ") + pod.name + QLatin1String(" &obj) {\n"
           "    QtRemoteObjects::copyStoredProperties(ds, &obj);\n"
           "    return ds;\n"
           "}\n")
           ;
}

void RepCodeGenerator::generatePOD(QTextStream &out, const POD &pod)
{
    QByteArray podData = pod.name.toLatin1();
    QStringList equalityCheck;
    Q_FOREACH (const PODAttribute &attr, pod.attributes) {
        equalityCheck << QStringLiteral("left.%1() == right.%1()").arg(attr.name);
        podData += attr.name.toLatin1() + typeData(attr.type, m_globalEnumsPODs);
    }
    m_globalEnumsPODs[pod.name] = podData;
    out << "class " << pod.name << "\n"
           "{\n"
           "    Q_GADGET\n"
        << "\n"
        <<      formatQPropertyDeclarations(pod)
        << "public:\n"
        <<      formatConstructors(pod)
        <<      formatPropertyGettersAndSetters(pod)
        << "private:\n"
        <<      formatDataMembers(pod)
        << "};\n"
        << "\n"
        << "inline bool operator==(const " << pod.name << " &left, const " << pod.name << " &right) Q_DECL_NOTHROW {\n"
        << "    return " << equalityCheck.join(QStringLiteral(" && ")) << ";\n"
        << "}\n"
        << "inline bool operator!=(const " << pod.name << " &left, const " << pod.name << " &right) Q_DECL_NOTHROW {\n"
        << "    return !(left == right);\n"
        << "}\n"
        << "\n"
        << formatMarshallingOperators(pod)
        << "\n"
           "\n"
           ;
}

QString getEnumType(const ASTEnum &en)
{
    if (en.isSigned) {
        if (en.max < 0x7F)
            return QStringLiteral("qint8");
        if (en.max < 0x7FFF)
            return QStringLiteral("qint16");
        return QStringLiteral("qint32");
    } else {
        if (en.max < 0xFF)
            return QStringLiteral("quint8");
        if (en.max < 0xFFFF)
            return QStringLiteral("quint16");
        return QStringLiteral("quint32");
    }
}

void RepCodeGenerator::generateDeclarationsForEnums(QTextStream &out, const QVector<ASTEnum> &enums, bool generateQENUM)
{
    if (!generateQENUM) {
        out << "    // You need to add this enum as well as Q_ENUM to your" << endl;
        out << "    // QObject class in order to use .rep enums over QtRO for" << endl;
        out << "    // non-repc generated QObjects." << endl;
    }
    Q_FOREACH (const ASTEnum &en, enums) {
        m_globalEnumsPODs[en.name] = enumSignature(en);
        out << "    enum " << en.name << " {" << endl;
        Q_FOREACH (const ASTEnumParam &p, en.params)
            out << "        " << p.name << " = " << p.value << "," << endl;

        out << "    };" << endl;

        if (generateQENUM) {
            out << "#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))" << endl;
            out << "    Q_ENUM(" << en.name << ")" << endl;
            out << "#else" << endl;
            out << "    Q_ENUMS(" << en.name << ")" << endl;
            out << "#endif" << endl;
        }
    }
}

void RepCodeGenerator::generateENUMs(QTextStream &out, const QVector<ASTEnum> &enums, const QString &className)
{
    out << "class " << className << "\n"
           "{\n"
           "    Q_GADGET\n"
           "    " << className << "();\n"
           "\n"
           "public:\n";

    generateDeclarationsForEnums(out, enums);
    generateConversionFunctionsForEnums(out, enums);

    out << "};\n\n";

    if (!enums.isEmpty()) {
        out << "#if (QT_VERSION < QT_VERSION_CHECK(5, 5, 0))\n";
        Q_FOREACH (const ASTEnum &en, enums)
            out << "    Q_DECLARE_METATYPE(" << className <<"::" << en.name << ")\n";
        out <<  "#endif\n\n";
    }

    generateStreamOperatorsForEnums(out, enums, className);
}

void RepCodeGenerator::generateConversionFunctionsForEnums(QTextStream &out, const QVector<ASTEnum> &enums)
{
    Q_FOREACH (const ASTEnum &en, enums)
    {
        const QString type = getEnumType(en);
        out << "    static inline " << en.name << " to" << en.name << "(" << type << " i, bool *ok = 0)\n"
               "    {\n"
               "        if (ok)\n"
               "            *ok = true;\n"
               "        switch (i) {\n";
            Q_FOREACH (const ASTEnumParam &p, en.params)
                out << "        case " << p.value << ": return " << p.name << ";\n";
        out << "        default:\n"
               "            if (ok)\n"
               "                *ok = false;\n"
               "            return " << en.params.at(0).name << ";\n"
               "        }\n"
               "    }\n";
    }
}

void RepCodeGenerator::generateStreamOperatorsForEnums(QTextStream &out, const QVector<ASTEnum> &enums, const QString &className)
{
    Q_FOREACH (const ASTEnum &en, enums)
    {
        const QString type = getEnumType(en);
        out <<  "inline QDataStream &operator<<(QDataStream &ds, const " << className << "::" << en.name << " &obj)\n"
                "{\n"
                "    " << type << " val = obj;\n"
                "    ds << val;\n"
                "    return ds;\n"
                "}\n\n"

                "inline QDataStream &operator>>(QDataStream &ds, " << className << "::" << en.name << " &obj) {\n"
                "    bool ok;\n"
                "    " << type << " val;\n"
                "    ds >> val;\n"
                "    obj = " << className << "::to" << en.name << "(val, &ok);\n"
                "    if (!ok)\n        qWarning() << \"QtRO received an invalid enum value for type" << en.name << ", value =\" << val;\n"
                "    return ds;\n"
                "}\n\n";
    }
}

void RepCodeGenerator::generateENUM(QTextStream &out, const ASTEnum &en)
{
    generateENUMs(out, (QVector<ASTEnum>() << en), QStringLiteral("%1Enum").arg(en.name));
}

QString RepCodeGenerator::generateMetaTypeRegistration(const QSet<QString> &metaTypes)
{
    QString out;
    const QString qRegisterMetaType = QStringLiteral("        qRegisterMetaType<");
    const QString qRegisterMetaTypeStreamOperators = QStringLiteral("        qRegisterMetaTypeStreamOperators<");
    const QString lineEnding = QStringLiteral(">();\n");
    Q_FOREACH (const QString &metaType, metaTypes) {
        if (isBuiltinType(metaType))
            continue;

        out += qRegisterMetaType;
        out += metaType;
        out += lineEnding;

        out += qRegisterMetaTypeStreamOperators;
        out += metaType;
        out += lineEnding;
    }
    return out;
}

QString RepCodeGenerator::generateMetaTypeRegistrationForEnums(const QVector<QString> &enumUses)
{
    QString out;

    Q_FOREACH (const QString &enumName, enumUses) {
        out += QLatin1String("        qRegisterMetaTypeStreamOperators<") + enumName + QLatin1String(">(\"") + enumName + QLatin1String("\");\n");
    }

    return out;
}

void RepCodeGenerator::generateStreamOperatorsForEnums(QTextStream &out, const QVector<QString> &enumUses)
{
    out << "QT_BEGIN_NAMESPACE" << endl;
    Q_FOREACH (const QString &enumName, enumUses) {
        out << "inline QDataStream &operator<<(QDataStream &out, " << enumName << " value)" << endl;
        out << "{" << endl;
        out << "    out << static_cast<qint32>(value);" << endl;
        out << "    return out;" << endl;
        out << "}" << endl;
        out << endl;
        out << "inline QDataStream &operator>>(QDataStream &in, " << enumName << " &value)" << endl;
        out << "{" << endl;
        out << "    qint32 intValue = 0;" << endl;
        out << "    in >> intValue;" << endl;
        out << "    value = static_cast<" << enumName << ">(intValue);" << endl;
        out << "    return in;" << endl;
        out << "}" << endl;
        out << endl;
    }
    out << "QT_END_NAMESPACE" << endl << endl;
}

void RepCodeGenerator::generateClass(Mode mode, QTextStream &out, const ASTClass &astClass, const QString &metaTypeRegistrationCode)
{
    const QString className = (astClass.name + (mode == REPLICA ? QStringLiteral("Replica") : mode == SOURCE ? QStringLiteral("Source") : QStringLiteral("SimpleSource")));
    if (mode == REPLICA)
        out << "class " << className << " : public QRemoteObjectReplica" << endl;
    else
        out << "class " << className << " : public QObject" << endl;

    out << "{" << endl;
    out << "    Q_OBJECT" << endl;
    out << "    Q_CLASSINFO(QCLASSINFO_REMOTEOBJECT_TYPE, \"" << astClass.name << "\")" << endl;
    out << "    Q_CLASSINFO(QCLASSINFO_REMOTEOBJECT_SIGNATURE, \"" << QLatin1String(classSignature(astClass)) << "\")" << endl;

    //First output properties
    Q_FOREACH (const ASTProperty &property, astClass.properties) {
        out << "    Q_PROPERTY(" << property.type << " " << property.name << " READ " << property.name;
        if (property.modifier == ASTProperty::Constant)
            out << " CONSTANT";
        else if (property.modifier == ASTProperty::ReadOnly)
            out << " NOTIFY " << property.name << "Changed";
        else if (property.modifier == ASTProperty::ReadWrite)
            out << " WRITE set" << cap(property.name) << " NOTIFY " << property.name << "Changed";
        else if (property.modifier == ASTProperty::ReadPush) {
            if (mode == REPLICA) // The setter slot isn't known to the PROP
                out << " NOTIFY " << property.name << "Changed";
            else // The Source can use the setter, since non-asynchronous
                out << " WRITE set" << cap(property.name) << " NOTIFY " << property.name << "Changed";
        }
        out << ")" << endl;
    }
    for (auto model : astClass.models) {
        if (mode == REPLICA)
            out << QString::fromLatin1("    Q_PROPERTY(QAbstractItemModelReplica *%1 READ %1 CONSTANT)").arg(model.name) << endl;
        else
            out << QString::fromLatin1("    Q_PROPERTY(QAbstractItemModel *%1 READ %1 CONSTANT)").arg(model.name) << endl;
    }

    if (!astClass.enums.isEmpty()) {
        out << "" << endl;
        out << "public:" << endl;
        generateDeclarationsForEnums(out, astClass.enums);
    }

    out << "" << endl;
    out << "public:" << endl;

    if (mode == REPLICA) {
        out << "    " << className << "() : QRemoteObjectReplica() { initialize(); }" << endl;
        out << "    static void registerMetatypes()" << endl;
        out << "    {" << endl;
        out << "        static bool initialized = false;" << endl;
        out << "        if (initialized)" << endl;
        out << "            return;" << endl;
        out << "        initialized = true;" << endl;

        if (!metaTypeRegistrationCode.isEmpty())
            out << metaTypeRegistrationCode << endl;

        out << "    }" << endl;

        out << "" << endl;
        out << "private:" << endl;
        out << "    " << className << "(QRemoteObjectNode *node, const QString &name = QString())" << endl;
        out << "        : QRemoteObjectReplica(ConstructWithNode)" << endl;
        for (auto model : astClass.models)
            out << QString::fromLatin1("        , m_%1(node->acquireModel(\"%2::%1\"))")
                                       .arg(model.name, astClass.name) << endl;
        out << "        { initializeNode(node, name); }" << endl;

        out << "" << endl;

        out << "    void initialize()" << endl;
        out << "    {" << endl;
        out << "        " << className << "::registerMetatypes();" << endl;
        out << "        QVariantList properties;" << endl;
        out << "        properties.reserve(" << astClass.properties.size() << ");" << endl;
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            out << "        properties << QVariant::fromValue(" << property.type << "(" << property.defaultValue << "));" << endl;
        }
        int nPersisted = 0;
        if (astClass.hasPersisted) {
            out << "        QVariantList stored = retrieveProperties(\"" << astClass.name << "\", \"" << classSignature(astClass) << "\");" << endl;
            out << "        if (!stored.isEmpty()) {" << endl;
            for (int i = 0; i < astClass.properties.size(); i++) {
                if (astClass.properties.at(i).persisted) {
                    out << "            properties[" << i << "] = stored.at(" << nPersisted << ");" << endl;
                    nPersisted++;
                }
            }
            out << "        }" << endl;
        }
        out << "        setProperties(properties);" << endl;
        out << "    }" << endl;
    } else {
        if (astClass.models.isEmpty() || mode == SOURCE)
            out << "    explicit " << className << "(QObject *parent = nullptr) : QObject(parent)" << endl;
        else {
            const QString modelParameterString = QStringLiteral("model%1");
            QStringList parametersForModels;
            for (int i = 0; i < astClass.models.count(); i++)
                parametersForModels << modelParameterString.arg(i+1);
            out << "    explicit " << className << "(QAbstractItemModel *"
                << parametersForModels.join(QString::fromLatin1(", QAbstractItemModel *"))
                << ", QObject *parent = nullptr) : QObject(parent)" << endl;
            for (int i = 0; i < astClass.models.count(); i++)
            {
                out << "        , m_" << astClass.models[i].name
                    << "(" << parametersForModels.at(i) << ")" << endl;
            }
        }

        if (mode == SIMPLE_SOURCE) {
            Q_FOREACH (const ASTProperty &property, astClass.properties) {
                out << "        , _" << property.name << "(" << property.defaultValue << ")" << endl;
            }
        }

        out << "    {" << endl;
        if (!metaTypeRegistrationCode.isEmpty())
            out << metaTypeRegistrationCode << endl;
        out << "    }" << endl;
    }

    out << "" << endl;
    out << "public:" << endl;

    if (mode == REPLICA && astClass.hasPersisted) {
        out << "    virtual ~" << className << "() {" << endl;
        out << "        QVariantList persisted;" << endl;
        for (int i = 0; i < astClass.properties.size(); i++) {
            if (astClass.properties.at(i).persisted) {
                out << "        persisted << propAsVariant(" << i << ");" << endl;
            }
        }
        out << "        persistProperties(\"" << astClass.name << "\", \"" << classSignature(astClass) << "\", persisted);" << endl;
        out << "    }" << endl;
    } else {
        out << "    virtual ~" << className << "() {}" << endl;
    }
    out << "" << endl;

    generateConversionFunctionsForEnums(out, astClass.enums);

    //Next output getter/setter
    if (mode == REPLICA) {
        int i = 0;
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            out << "    " << property.type << " " << property.name << "() const" << endl;
            out << "    {" << endl;
            out << "        const QVariant variant = propAsVariant(" << i << ");" << endl;
            out << "        if (!variant.canConvert<" << property.type << ">()) {" << endl;
            out << "            qWarning() << \"QtRO cannot convert the property " << property.name << " to type " << property.type << "\";" << endl;
            out << "        }" << endl;
            out << "        return variant.value<" << property.type << " >();" << endl;
            out << "    }" << endl;
            i++;
            if (property.modifier == ASTProperty::ReadWrite) {
                out << "" << endl;
                out << "    void set" << cap(property.name) << "(" << property.type << " " << property.name << ")" << endl;
                out << "    {" << endl;
                out << "        static int __repc_index = " << className << "::staticMetaObject.indexOfProperty(\"" << property.name << "\");" << endl;
                out << "        QVariantList __repc_args;" << endl;
                out << "        __repc_args << QVariant::fromValue(" << property.name << ");" << endl;
                out << "        send(QMetaObject::WriteProperty, __repc_index, __repc_args);" << endl;
                out << "    }" << endl;
            }
            out << "" << endl;
        }
    } else if (mode == SOURCE) {
        Q_FOREACH (const ASTProperty &property, astClass.properties)
            out << "    virtual " << property.type << " " << property.name << "() const = 0;" << endl;
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            if (property.modifier == ASTProperty::ReadWrite ||
                    property.modifier == ASTProperty::ReadPush)
                out << "    virtual void set" << cap(property.name) << "(" << property.type << " " << property.name << ") = 0;" << endl;
        }
    } else {
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            out << "    virtual " << property.type << " " << property.name << "() const { return _" << property.name << "; }" << endl;
        }
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            if (property.modifier == ASTProperty::ReadWrite ||
                    property.modifier == ASTProperty::ReadPush) {
                out << "    virtual void set" << cap(property.name) << "(" << property.type << " " << property.name << ")" << endl;
                out << "    {" << endl;
                out << "        if (" << property.name << " != _" << property.name << ") { " << endl;
                out << "            _" << property.name << " = " << property.name << ";" << endl;
                out << "            Q_EMIT " << property.name << "Changed(_" << property.name << ");" << endl;
                out << "        }" << endl;
                out << "    }" << endl;
            }
        }
    }

    if (!astClass.models.isEmpty()) {
        Q_FOREACH (const ASTModel &model, astClass.models) {
            if (mode != SOURCE) {
                if (mode == REPLICA)
                    out << "    QAbstractItemModelReplica *" << model.name << "()" << endl;
                else
                    out << "    QAbstractItemModel *" << model.name << "()" << endl;
                out << "    {" << endl;
                out << "        return m_" << model.name << ".data();" << endl;
                out << "    }" << endl;
            } else {
                out << "    virtual QAbstractItemModel *" << model.name << "() = 0;" << endl;
            }
        }
    }

    //Next output property signals
    if (!astClass.properties.isEmpty() || !astClass.signalsList.isEmpty()) {
        out << "" << endl;
        out << "Q_SIGNALS:" << endl;
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            if (property.modifier != ASTProperty::Constant)
                out << "    void " << property.name << "Changed(" << fullyQualifiedTypeName(astClass, className, property.type) << ");" << endl;
        }

        Q_FOREACH (const ASTFunction &signal, astClass.signalsList)
            out << "    void " << signal.name << "(" << signal.paramsAsString() << ");" << endl;
    }
    bool hasWriteSlots = false;
    Q_FOREACH (const ASTProperty &property, astClass.properties) {
        if (property.modifier == ASTProperty::ReadPush)
            hasWriteSlots = true;
    }
    if (hasWriteSlots || !astClass.slotsList.isEmpty()) {
        out << "" << endl;
        out << "public Q_SLOTS:" << endl;
        Q_FOREACH (const ASTProperty &property, astClass.properties) {
            if (property.modifier == ASTProperty::ReadPush) {
                if (mode != REPLICA) {
                    out << "    virtual void push" << cap(property.name) << "(" << property.type << " " << property.name << ")" << endl;
                    out << "    {" << endl;
                    out << "        set" << cap(property.name) << "(" << property.name << ");" << endl;
                    out << "    }" << endl;
                } else {
                    out << "    void push" << cap(property.name) << "(" << property.type << " " << property.name << ")" << endl;
                    out << "    {" << endl;
                    out << "        static int __repc_index = " << className << "::staticMetaObject.indexOfSlot(\"push" << cap(property.name) << "(" << property.type << ")\");" << endl;
                    out << "        QVariantList __repc_args;" << endl;
                    out << "        __repc_args << QVariant::fromValue(" << property.name << ");" << endl;
                    out << "        send(QMetaObject::InvokeMetaMethod, __repc_index, __repc_args);" << endl;
                    out << "    }" << endl;
                }
            }
        }
        Q_FOREACH (const ASTFunction &slot, astClass.slotsList) {
            if (mode != REPLICA) {
                out << "    virtual " << slot.returnType << " " << slot.name << "(" << slot.paramsAsString() << ") = 0;" << endl;
            } else {
                // TODO: Discuss whether it is a good idea to special-case for void here,
                const bool isVoid = slot.returnType == QStringLiteral("void");

                if (isVoid)
                    out << "    void " << slot.name << "(" << slot.paramsAsString() << ")" << endl;
                else
                    out << "    QRemoteObjectPendingReply<" << slot.returnType << "> " << slot.name << "(" << slot.paramsAsString()<< ")" << endl;
                out << "    {" << endl;
                out << "        static int __repc_index = " << className << "::staticMetaObject.indexOfSlot(\"" << slot.name << "(" << slot.paramsAsString(ASTFunction::Normalized) << ")\");" << endl;
                out << "        QVariantList __repc_args;" << endl;
                if (!slot.paramNames().isEmpty()) {
                    out << "        __repc_args" << endl;
                    Q_FOREACH (const QString &name, slot.paramNames())
                        out << "            << " << "QVariant::fromValue(" << name << ")" << endl;
                    out << "        ;" << endl;
                }
                if (isVoid)
                    out << "        send(QMetaObject::InvokeMetaMethod, __repc_index, __repc_args);" << endl;
                else
                    out << "        return QRemoteObjectPendingReply<" << slot.returnType << ">(sendWithReply(QMetaObject::InvokeMetaMethod, __repc_index, __repc_args));" << endl;
                out << "    }" << endl;
            }
        }
    }

    if (mode == SIMPLE_SOURCE)
    {
        //Next output data members
        if (!astClass.properties.isEmpty()) {
            out << "" << endl;
            out << "private:" << endl;
            Q_FOREACH (const ASTProperty &property, astClass.properties) {
                out << "    " << property.type << " " << "_" << property.name << ";" << endl;
            }
        }
        Q_FOREACH (const ASTModel &model, astClass.models)
            out << "    QScopedPointer<QAbstractItemModel> m_" << model.name << ";" << endl;
    }

    out << "" << endl;
    out << "private:" << endl;
    if (mode == REPLICA) {
        Q_FOREACH (const ASTModel &model, astClass.models)
            out << "    QScopedPointer<QAbstractItemModelReplica> m_" << model.name << ";" << endl;
    }
    out << "    friend class QT_PREPEND_NAMESPACE(QRemoteObjectNode);" << endl;

    out << "};" << endl;
    out << "" << endl;

    out << "#if (QT_VERSION < QT_VERSION_CHECK(5, 5, 0))" << endl;
    Q_FOREACH (const ASTEnum &en, astClass.enums)
        out << "    Q_DECLARE_METATYPE(" << className << "::" << en.name << ")" << endl;
    out <<  "#endif" << endl;
    out << "" << endl;

    generateStreamOperatorsForEnums(out, astClass.enums, className);

    out << "" << endl;
}

void RepCodeGenerator::generateSourceAPI(QTextStream &out, const ASTClass &astClass)
{
    const QString className = astClass.name + QStringLiteral("SourceAPI");
    out << QStringLiteral("template <class ObjectType>") << endl;
    out << QString::fromLatin1("struct %1 : public SourceApiMap").arg(className) << endl;
    out << QStringLiteral("{") << endl;
    if (!astClass.enums.isEmpty()) {
        // Include enum definition in SourceAPI
        generateDeclarationsForEnums(out, astClass.enums, false);
    }
    out << QString::fromLatin1("    %1(ObjectType *object)").arg(className) << endl;
    out << QStringLiteral("        : SourceApiMap()") << endl;
    out << QStringLiteral("    {") << endl;
    if (astClass.models.isEmpty())
        out << QStringLiteral("        Q_UNUSED(object);") << endl;
    const int propCount = astClass.properties.count();
    out << QString::fromLatin1("        _properties[0] = %1;").arg(propCount) << endl;
    QStringList changeSignals;
    QList<int> propertyChangeIndex;
    for (int i = 0; i < propCount; ++i) {
        const ASTProperty &prop = astClass.properties.at(i);
        const QString propTypeName = fullyQualifiedTypeName(astClass, QStringLiteral("typename ObjectType"), prop.type);
        out << QString::fromLatin1("        _properties[%1] = qtro_prop_index<ObjectType>(&ObjectType::%2, "
                              "static_cast<%3 (QObject::*)()>(0),\"%2\");")
                             .arg(QString::number(i+1), prop.name, propTypeName) << endl;
        if (prop.modifier == prop.ReadWrite) //Make sure we have a setter function
            out << QStringLiteral("        qtro_method_test<ObjectType>(&ObjectType::set%1, static_cast<void (QObject::*)(%2)>(0));")
                                 .arg(cap(prop.name), propTypeName) << endl;
        if (prop.modifier != prop.Constant) { //Make sure we have an onChange signal
            out << QStringLiteral("        qtro_method_test<ObjectType>(&ObjectType::%1Changed, static_cast<void (QObject::*)()>(0));")
                                 .arg(prop.name) << endl;
            changeSignals << QString::fromLatin1("%1Changed").arg(prop.name);
            propertyChangeIndex << i + 1; //_properties[0] is the count, so index is one higher
        }
    }
    const int signalCount = astClass.signalsList.count();
    const int changedCount = changeSignals.size();
    out << QString::fromLatin1("        _signals[0] = %1;").arg(signalCount+changeSignals.size()) << endl;
    for (int i = 0; i < changedCount; ++i)
        out << QString::fromLatin1("        _signals[%1] = qtro_signal_index<ObjectType>(&ObjectType::%2, "
                              "static_cast<void (QObject::*)()>(0),signalArgCount+%4,&signalArgTypes[%4]);")
                             .arg(i+1).arg(changeSignals.at(i)).arg(i) << endl;
    for (int i = 0; i < signalCount; ++i) {
        const ASTFunction &sig = astClass.signalsList.at(i);
        out << QString::fromLatin1("        _signals[%1] = qtro_signal_index<ObjectType>(&ObjectType::%2, "
                              "static_cast<void (QObject::*)(%3)>(0),signalArgCount+%4,&signalArgTypes[%4]);")
                             .arg(QString::number(changedCount+i+1), sig.name, sig.paramsAsString(ASTFunction::Normalized), QString::number(i)) << endl;
    }
    const int slotCount = astClass.slotsList.count();
    QVector<ASTProperty> pushProps;
    Q_FOREACH (const ASTProperty &property, astClass.properties) {
        if (property.modifier == ASTProperty::ReadPush)
            pushProps << property;
    }
    const int pushCount = pushProps.count();
    const int methodCount = slotCount + pushCount;
    out << QString::fromLatin1("        _methods[0] = %1;").arg(methodCount) << endl;
    for (int i = 0; i < pushCount; ++i) {
        const ASTProperty &prop = pushProps.at(i);
        const QString propTypeName = fullyQualifiedTypeName(astClass, QStringLiteral("typename ObjectType"), prop.type);
        out << QString::fromLatin1("        _methods[%1] = qtro_method_index<ObjectType>(&ObjectType::push%2, "
                              "static_cast<void (QObject::*)(%3)>(0),\"push%2(%3)\",methodArgCount+%4,&methodArgTypes[%4]);")
                             .arg(QString::number(i+1), cap(prop.name), propTypeName, QString::number(i)) << endl;
    }
    for (int i = 0; i < slotCount; ++i) {
        const ASTFunction &slot = astClass.slotsList.at(i);
        out << QString::fromLatin1("        _methods[%1] = qtro_method_index<ObjectType>(&ObjectType::%2, "
                              "static_cast<void (QObject::*)(%3)>(0),\"%2(%3)\",methodArgCount+%4,&methodArgTypes[%4]);")
                             .arg(QString::number(i+pushCount+1), slot.name, slot.paramsAsString(ASTFunction::Normalized), QString::number(i+pushCount)) << endl;
    }
    const int modelCount = astClass.models.count();
    out << QString::fromLatin1("        _modelCount = %1;").arg(modelCount) << endl;
    for (int i = 0; i < modelCount; ++i) {
        const ASTModel &model = astClass.models.at(i);
        out << QString::fromLatin1("        _models[%1] = object->%2();").arg(QString::number(i), model.name) << endl;
    }

    out << QStringLiteral("    }") << endl;
    out << QStringLiteral("") << endl;
    out << QString::fromLatin1("    QString name() const override { return QStringLiteral(\"%1\"); }").arg(astClass.name) << endl;
    out << QString::fromLatin1("    QString typeName() const override { return QStringLiteral(\"%1\"); }").arg(astClass.name) << endl;
    out << QStringLiteral("    int propertyCount() const override { return _properties[0]; }") << endl;
    out << QStringLiteral("    int signalCount() const override { return _signals[0]; }") << endl;
    out << QStringLiteral("    int methodCount() const override { return _methods[0]; }") << endl;
    out << QStringLiteral("    int modelCount() const override { return _modelCount; }") << endl;
    out << QStringLiteral("    int sourcePropertyIndex(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    out << QStringLiteral("        if (index < 0 || index >= _properties[0])") << endl;
    out << QStringLiteral("            return -1;") << endl;
    out << QStringLiteral("        return _properties[index+1];") << endl;
    out << QStringLiteral("    }") << endl;
    out << QStringLiteral("    int sourceSignalIndex(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    out << QStringLiteral("        if (index < 0 || index >= _signals[0])") << endl;
    out << QStringLiteral("            return -1;") << endl;
    out << QStringLiteral("        return _signals[index+1];") << endl;
    out << QStringLiteral("    }") << endl;
    out << QStringLiteral("    int sourceMethodIndex(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    out << QStringLiteral("        if (index < 0 || index >= _methods[0])") << endl;
    out << QStringLiteral("            return -1;") << endl;
    out << QStringLiteral("        return _methods[index+1];") << endl;
    out << QStringLiteral("    }") << endl;
    if (signalCount+changedCount > 0) {
        out << QStringLiteral("    int signalParameterCount(int index) const override") << endl;
        out << QStringLiteral("    {") << endl;
        out << QStringLiteral("        if (index < 0 || index >= _signals[0])") << endl;
        out << QStringLiteral("            return -1;") << endl;
        out << QStringLiteral("        return signalArgCount[index];") << endl;
        out << QStringLiteral("    }") << endl;
        out << QStringLiteral("    int signalParameterType(int sigIndex, int paramIndex) const override") << endl;
        out << QStringLiteral("    {") << endl;
        out << QStringLiteral("        if (sigIndex < 0 || sigIndex >= _signals[0] || paramIndex < 0 || paramIndex >= signalArgCount[sigIndex])") << endl;
        out << QStringLiteral("            return -1;") << endl;
        out << QStringLiteral("        return signalArgTypes[sigIndex][paramIndex];") << endl;
        out << QStringLiteral("    }") << endl;
    } else {
        out << QStringLiteral("    int signalParameterCount(int index) const override { Q_UNUSED(index); return -1; }") << endl;
        out << QStringLiteral("    int signalParameterType(int sigIndex, int paramIndex) const override") << endl;
        out << QStringLiteral("    { Q_UNUSED(sigIndex); Q_UNUSED(paramIndex); return -1; }") << endl;
    }
    if (methodCount > 0) {
        out << QStringLiteral("    int methodParameterCount(int index) const override") << endl;
        out << QStringLiteral("    {") << endl;
        out << QStringLiteral("        if (index < 0 || index >= _methods[0])") << endl;
        out << QStringLiteral("            return -1;") << endl;
        out << QStringLiteral("        return methodArgCount[index];") << endl;
        out << QStringLiteral("    }") << endl;
        out << QStringLiteral("    int methodParameterType(int methodIndex, int paramIndex) const override") << endl;
        out << QStringLiteral("    {") << endl;
        out << QStringLiteral("        if (methodIndex < 0 || methodIndex >= _methods[0] || paramIndex < 0 || paramIndex >= methodArgCount[methodIndex])") << endl;
        out << QStringLiteral("            return -1;") << endl;
        out << QStringLiteral("        return methodArgTypes[methodIndex][paramIndex];") << endl;
        out << QStringLiteral("    }") << endl;
    } else {
        out << QStringLiteral("    int methodParameterCount(int index) const override { Q_UNUSED(index); return -1; }") << endl;
        out << QStringLiteral("    int methodParameterType(int methodIndex, int paramIndex) const override") << endl;
        out << QStringLiteral("    { Q_UNUSED(methodIndex); Q_UNUSED(paramIndex); return -1; }") << endl;
    }
    //propertyIndexFromSignal method
    out << QStringLiteral("    int propertyIndexFromSignal(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    if (!propertyChangeIndex.isEmpty()) {
        out << QStringLiteral("        switch (index) {") << endl;
        for (int i = 0; i < propertyChangeIndex.size(); ++i)
            out << QString::fromLatin1("        case %1: return _properties[%2];").arg(i).arg(propertyChangeIndex.at(i)) << endl;
        out << QStringLiteral("        }") << endl;
    } else
        out << QStringLiteral("        Q_UNUSED(index);") << endl;
    out << QStringLiteral("        return -1;") << endl;
    out << QStringLiteral("    }") << endl;
    //propertyRawIndexFromSignal method
    out << QStringLiteral("    int propertyRawIndexFromSignal(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    if (!propertyChangeIndex.isEmpty()) {
        out << QStringLiteral("        switch (index) {") << endl;
        for (int i = 0; i < propertyChangeIndex.size(); ++i)
            out << QString::fromLatin1("        case %1: return %2;").arg(i).arg(propertyChangeIndex.at(i)) << endl;
        out << QStringLiteral("        }") << endl;
    } else
        out << QStringLiteral("        Q_UNUSED(index);") << endl;
    out << QStringLiteral("        return -1;") << endl;
    out << QStringLiteral("    }") << endl;

    //signalSignature method
    out << QStringLiteral("    const QByteArray signalSignature(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    if (signalCount+changedCount > 0) {
        out << QStringLiteral("        switch (index) {") << endl;
        for (int i = 0; i < changedCount; ++i)
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"%2()\");").arg(i).arg(changeSignals.at(i)) << endl;
        for (int i = 0; i < signalCount; ++i)
        {
            const ASTFunction &sig = astClass.signalsList.at(i);
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"%2(%3)\");")
                                      .arg(QString::number(i+changedCount), sig.name, sig.paramsAsString(ASTFunction::Normalized)) << endl;
        }
        out << QStringLiteral("        }") << endl;
    } else
        out << QStringLiteral("        Q_UNUSED(index);") << endl;
    out << QStringLiteral("        return QByteArrayLiteral(\"\");") << endl;
    out << QStringLiteral("    }") << endl;

    //methodSignature method
    out << QStringLiteral("    const QByteArray methodSignature(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    if (methodCount > 0) {
        out << QStringLiteral("        switch (index) {") << endl;
        for (int i = 0; i < pushCount; ++i)
        {
            const ASTProperty &prop = pushProps.at(i);
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"push%2(%3)\");")
                                      .arg(QString::number(i), cap(prop.name), prop.type) << endl;
        }
        for (int i = 0; i < slotCount; ++i)
        {
            const ASTFunction &slot = astClass.slotsList.at(i);
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"%2(%3)\");")
                                      .arg(QString::number(i+pushCount), slot.name, slot.paramsAsString(ASTFunction::Normalized)) << endl;
        }
        out << QStringLiteral("        }") << endl;
    } else
        out << QStringLiteral("        Q_UNUSED(index);") << endl;
    out << QStringLiteral("        return QByteArrayLiteral(\"\");") << endl;
    out << QStringLiteral("    }") << endl;

    //methodType method
    out << QStringLiteral("    QMetaMethod::MethodType methodType(int) const override") << endl;
    out << QStringLiteral("    {") << endl;
    out << QStringLiteral("        return QMetaMethod::Slot;") << endl;
    out << QStringLiteral("    }") << endl;

    //typeName method
    out << QStringLiteral("    const QByteArray typeName(int index) const override") << endl;
    out << QStringLiteral("    {") << endl;
    if (methodCount > 0) {
        out << QStringLiteral("        switch (index) {") << endl;
        for (int i = 0; i < pushCount; ++i)
        {
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"void\");")
                                      .arg(QString::number(i)) << endl;
        }
        for (int i = 0; i < slotCount; ++i)
        {
            const ASTFunction &slot = astClass.slotsList.at(i);
            out << QString::fromLatin1("        case %1: return QByteArrayLiteral(\"%2\");")
                                      .arg(QString::number(i+pushCount), slot.returnType) << endl;
        }
        out << QStringLiteral("        }") << endl;
    } else
        out << QStringLiteral("        Q_UNUSED(index);") << endl;
    out << QStringLiteral("        return QByteArrayLiteral(\"\");") << endl;
    out << QStringLiteral("    }") << endl;

    //objectSignature method
    out << QStringLiteral("    QByteArray objectSignature() const override { return QByteArray{\"")
        << QLatin1String(classSignature(astClass))
        << QStringLiteral("\"}; }") << endl;

    out << QStringLiteral("") << endl;
    out << QString::fromLatin1("    int _properties[%1];").arg(propCount+1) << endl;
    out << QString::fromLatin1("    int _signals[%1];").arg(signalCount+changedCount+1) << endl;
    out << QString::fromLatin1("    int _methods[%1];").arg(methodCount+1) << endl;
    if (signalCount+changedCount > 0) {
        out << QString::fromLatin1("    int signalArgCount[%1];").arg(signalCount+changedCount) << endl;
        out << QString::fromLatin1("    const int* signalArgTypes[%1];").arg(signalCount+changedCount) << endl;
    }
    if (methodCount > 0) {
        out << QString::fromLatin1("    int methodArgCount[%1];").arg(methodCount) << endl;
        out << QString::fromLatin1("    const int* methodArgTypes[%1];").arg(methodCount) << endl;
    }
    out << QString::fromLatin1("    int _modelCount;") << endl;
    if (modelCount > 0)
    {
        out << QString::fromLatin1("    QAbstractItemModel *_models[%1];").arg(modelCount) << endl;
        out << "    void modelSetup(QRemoteObjectHostBase *node) const override" << endl;
        out << "    {" << endl;
        out << "        QVector<int> roles;" << endl;
        out << "        int roleIndex;" << endl;
        out << "        QHash<int, QByteArray> knownRoles;" << endl;
        for (int i = 0; i < astClass.models.count(); i++)
        {
            out << endl;
            const ASTModel model = astClass.models.at(i);
            out << "        // Handle model #" << i+1 << " " << model.name << endl;
            out << "        roles.clear();" << endl;
            out << "        knownRoles = _models[" << i << "]->roleNames();" << endl;
            for (auto role : model.roles)
            {
                out << "        roleIndex = knownRoles.key(\"" << role.name << "\", -1);" << endl;
                out << "        if (roleIndex == -1)" << endl;
                out << "            qWarning() << " << QString::fromLatin1("\"Invalid role %1 for model %2\";").arg(role.name, model.name) << endl;
                out << "        else" << endl;
                out << "            roles << roleIndex;" << endl;
            }
            out << "        node->enableRemoting(" << QString::fromLatin1("_models[%1], \"%2::%3\", roles, nullptr);").arg(QString::number(i), astClass.name, model.name) << endl;
        }
        out << "    }" << endl;
    }
    out << QStringLiteral("};") << endl;
}

QT_END_NAMESPACE
