/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qv4jsonobject_p.h>
#include <qv4objectproto_p.h>
#include <qv4numberobject_p.h>
#include <qv4stringobject_p.h>
#include <qv4booleanobject_p.h>
#include <qv4objectiterator_p.h>
#include <qv4scopedvalue_p.h>
#include <qv4runtime_p.h>
#include <qv4variantobject_p.h>
#include "qv4string_p.h"

#include <qstack.h>
#include <qstringlist.h>

#include <wtf/MathExtras.h>

using namespace QV4;

//#define PARSER_DEBUG
#ifdef PARSER_DEBUG
static int indent = 0;
#define BEGIN qDebug() << QByteArray(4*indent++, ' ').constData()
#define END --indent
#define DEBUG qDebug() << QByteArray(4*indent, ' ').constData()
#else
#define BEGIN if (1) ; else qDebug()
#define END do {} while (0)
#define DEBUG if (1) ; else qDebug()
#endif


DEFINE_OBJECT_VTABLE(JsonObject);

static const int nestingLimit = 1024;


JsonParser::JsonParser(ExecutionEngine *engine, const QChar *json, int length)
    : engine(engine), head(json), json(json), nestingLevel(0), lastError(QJsonParseError::NoError)
{
    end = json + length;
}



/*

begin-array     = ws %x5B ws  ; [ left square bracket

begin-object    = ws %x7B ws  ; { left curly bracket

end-array       = ws %x5D ws  ; ] right square bracket

end-object      = ws %x7D ws  ; } right curly bracket

name-separator  = ws %x3A ws  ; : colon

value-separator = ws %x2C ws  ; , comma

Insignificant whitespace is allowed before or after any of the six
structural characters.

ws = *(
          %x20 /              ; Space
          %x09 /              ; Horizontal tab
          %x0A /              ; Line feed or New line
          %x0D                ; Carriage return
      )

*/

enum {
    Space = 0x20,
    Tab = 0x09,
    LineFeed = 0x0a,
    Return = 0x0d,
    BeginArray = 0x5b,
    BeginObject = 0x7b,
    EndArray = 0x5d,
    EndObject = 0x7d,
    NameSeparator = 0x3a,
    ValueSeparator = 0x2c,
    Quote = 0x22
};

bool JsonParser::eatSpace()
{
    while (json < end) {
        if (*json > Space)
            break;
        if (*json != Space &&
            *json != Tab &&
            *json != LineFeed &&
            *json != Return)
            break;
        ++json;
    }
    return (json < end);
}

QChar JsonParser::nextToken()
{
    if (!eatSpace())
        return 0;
    QChar token = *json++;
    switch (token.unicode()) {
    case BeginArray:
    case BeginObject:
    case NameSeparator:
    case ValueSeparator:
    case EndArray:
    case EndObject:
        eatSpace();
    case Quote:
        break;
    default:
        token = 0;
        break;
    }
    return token;
}

/*
    JSON-text = object / array
*/
ReturnedValue JsonParser::parse(QJsonParseError *error)
{
#ifdef PARSER_DEBUG
    indent = 0;
    qDebug() << ">>>>> parser begin";
#endif

    eatSpace();

    Scope scope(engine);
    ScopedValue v(scope);
    if (!parseValue(v)) {
#ifdef PARSER_DEBUG
        qDebug() << ">>>>> parser error";
#endif
        if (lastError == QJsonParseError::NoError)
            lastError = QJsonParseError::IllegalValue;
        error->offset = json - head;
        error->error  = lastError;
        return Encode::undefined();
    }

    // some input left...
    if (eatSpace()) {
        lastError = QJsonParseError::IllegalValue;
        error->offset = json - head;
        error->error  = lastError;
        return Encode::undefined();
    }

    END;
    error->offset = 0;
    error->error = QJsonParseError::NoError;
    return v->asReturnedValue();
}

/*
    object = begin-object [ member *( value-separator member ) ]
    end-object
*/

ReturnedValue JsonParser::parseObject()
{
    if (++nestingLevel > nestingLimit) {
        lastError = QJsonParseError::DeepNesting;
        return Encode::undefined();
    }

    BEGIN << "parseObject pos=" << json;
    Scope scope(engine);

    ScopedObject o(scope, engine->newObject());

    QChar token = nextToken();
    while (token == Quote) {
        if (!parseMember(o))
            return Encode::undefined();
        token = nextToken();
        if (token != ValueSeparator)
            break;
        token = nextToken();
        if (token == EndObject) {
            lastError = QJsonParseError::MissingObject;
            return Encode::undefined();
        }
    }

    DEBUG << "end token=" << token;
    if (token != EndObject) {
        lastError = QJsonParseError::UnterminatedObject;
        return Encode::undefined();
    }

    END;

    --nestingLevel;
    return o.asReturnedValue();
}

/*
    member = string name-separator value
*/
bool JsonParser::parseMember(Object *o)
{
    BEGIN << "parseMember";
    Scope scope(engine);

    QString key;
    if (!parseString(&key))
        return false;
    QChar token = nextToken();
    if (token != NameSeparator) {
        lastError = QJsonParseError::MissingNameSeparator;
        return false;
    }
    ScopedValue val(scope);
    if (!parseValue(val))
        return false;

    ScopedString s(scope, engine->newIdentifier(key));
    uint idx = s->asArrayIndex();
    if (idx < UINT_MAX) {
        o->putIndexed(idx, val);
    } else {
        o->insertMember(s, val);
    }

    END;
    return true;
}

/*
    array = begin-array [ value *( value-separator value ) ] end-array
*/
ReturnedValue JsonParser::parseArray()
{
    Scope scope(engine);
    BEGIN << "parseArray";
    ScopedArrayObject array(scope, engine->newArrayObject());

    if (++nestingLevel > nestingLimit) {
        lastError = QJsonParseError::DeepNesting;
        return Encode::undefined();
    }

    if (!eatSpace()) {
        lastError = QJsonParseError::UnterminatedArray;
        return Encode::undefined();
    }
    if (*json == EndArray) {
        nextToken();
    } else {
        uint index = 0;
        while (1) {
            ScopedValue val(scope);
            if (!parseValue(val))
                return Encode::undefined();
            array->arraySet(index, val);
            QChar token = nextToken();
            if (token == EndArray)
                break;
            else if (token != ValueSeparator) {
                if (!eatSpace())
                    lastError = QJsonParseError::UnterminatedArray;
                else
                    lastError = QJsonParseError::MissingValueSeparator;
                return Encode::undefined();
            }
            ++index;
        }
    }

    DEBUG << "size =" << array->getLength();
    END;

    --nestingLevel;
    return array.asReturnedValue();
}

/*
value = false / null / true / object / array / number / string

*/

bool JsonParser::parseValue(Value *val)
{
    BEGIN << "parse Value" << *json;

    switch ((json++)->unicode()) {
    case 'n':
        if (end - json < 3) {
            lastError = QJsonParseError::IllegalValue;
            return false;
        }
        if (*json++ == 'u' &&
            *json++ == 'l' &&
            *json++ == 'l') {
            *val = Primitive::nullValue();
            DEBUG << "value: null";
            END;
            return true;
        }
        lastError = QJsonParseError::IllegalValue;
        return false;
    case 't':
        if (end - json < 3) {
            lastError = QJsonParseError::IllegalValue;
            return false;
        }
        if (*json++ == 'r' &&
            *json++ == 'u' &&
            *json++ == 'e') {
            *val = Primitive::fromBoolean(true);
            DEBUG << "value: true";
            END;
            return true;
        }
        lastError = QJsonParseError::IllegalValue;
        return false;
    case 'f':
        if (end - json < 4) {
            lastError = QJsonParseError::IllegalValue;
            return false;
        }
        if (*json++ == 'a' &&
            *json++ == 'l' &&
            *json++ == 's' &&
            *json++ == 'e') {
            *val = Primitive::fromBoolean(false);
            DEBUG << "value: false";
            END;
            return true;
        }
        lastError = QJsonParseError::IllegalValue;
        return false;
    case Quote: {
        QString value;
        if (!parseString(&value))
            return false;
        DEBUG << "value: string";
        END;
        *val = Value::fromHeapObject(engine->newString(value));
        return true;
    }
    case BeginArray: {
        *val = parseArray();
        if (val->isUndefined())
            return false;
        DEBUG << "value: array";
        END;
        return true;
    }
    case BeginObject: {
        *val = parseObject();
        if (val->isUndefined())
            return false;
        DEBUG << "value: object";
        END;
        return true;
    }
    case EndArray:
        lastError = QJsonParseError::MissingObject;
        return false;
    default:
        --json;
        if (!parseNumber(val))
            return false;
        DEBUG << "value: number";
        END;
    }

    return true;
}





/*
        number = [ minus ] int [ frac ] [ exp ]
        decimal-point = %x2E       ; .
        digit1-9 = %x31-39         ; 1-9
        e = %x65 / %x45            ; e E
        exp = e [ minus / plus ] 1*DIGIT
        frac = decimal-point 1*DIGIT
        int = zero / ( digit1-9 *DIGIT )
        minus = %x2D               ; -
        plus = %x2B                ; +
        zero = %x30                ; 0

*/

bool JsonParser::parseNumber(Value *val)
{
    BEGIN << "parseNumber" << *json;

    const QChar *start = json;
    bool isInt = true;

    // minus
    if (json < end && *json == '-')
        ++json;

    // int = zero / ( digit1-9 *DIGIT )
    if (json < end && *json == '0') {
        ++json;
    } else {
        while (json < end && *json >= '0' && *json <= '9')
            ++json;
    }

    // frac = decimal-point 1*DIGIT
    if (json < end && *json == '.') {
        isInt = false;
        ++json;
        while (json < end && *json >= '0' && *json <= '9')
            ++json;
    }

    // exp = e [ minus / plus ] 1*DIGIT
    if (json < end && (*json == 'e' || *json == 'E')) {
        isInt = false;
        ++json;
        if (json < end && (*json == '-' || *json == '+'))
            ++json;
        while (json < end && *json >= '0' && *json <= '9')
            ++json;
    }

    QString number(start, json - start);
    DEBUG << "numberstring" << number;

    if (isInt) {
        bool ok;
        int n = number.toInt(&ok);
        if (ok && n < (1<<25) && n > -(1<<25)) {
            *val = Primitive::fromInt32(n);
            END;
            return true;
        }
    }

    bool ok;
    double d;
    d = number.toDouble(&ok);

    if (!ok) {
        lastError = QJsonParseError::IllegalNumber;
        return false;
    }

    * val = Primitive::fromDouble(d);

    END;
    return true;
}

/*

        string = quotation-mark *char quotation-mark

        char = unescaped /
               escape (
                   %x22 /          ; "    quotation mark  U+0022
                   %x5C /          ; \    reverse solidus U+005C
                   %x2F /          ; /    solidus         U+002F
                   %x62 /          ; b    backspace       U+0008
                   %x66 /          ; f    form feed       U+000C
                   %x6E /          ; n    line feed       U+000A
                   %x72 /          ; r    carriage return U+000D
                   %x74 /          ; t    tab             U+0009
                   %x75 4HEXDIG )  ; uXXXX                U+XXXX

        escape = %x5C              ; \

        quotation-mark = %x22      ; "

        unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
 */
static inline bool addHexDigit(QChar digit, uint *result)
{
    ushort d = digit.unicode();
    *result <<= 4;
    if (d >= '0' && d <= '9')
        *result |= (d - '0');
    else if (d >= 'a' && d <= 'f')
        *result |= (d - 'a') + 10;
    else if (d >= 'A' && d <= 'F')
        *result |= (d - 'A') + 10;
    else
        return false;
    return true;
}

static inline bool scanEscapeSequence(const QChar *&json, const QChar *end, uint *ch)
{
    ++json;
    if (json >= end)
        return false;

    DEBUG << "scan escape";
    uint escaped = (json++)->unicode();
    switch (escaped) {
    case '"':
        *ch = '"'; break;
    case '\\':
        *ch = '\\'; break;
    case '/':
        *ch = '/'; break;
    case 'b':
        *ch = 0x8; break;
    case 'f':
        *ch = 0xc; break;
    case 'n':
        *ch = 0xa; break;
    case 'r':
        *ch = 0xd; break;
    case 't':
        *ch = 0x9; break;
    case 'u': {
        *ch = 0;
        if (json > end - 4)
            return false;
        for (int i = 0; i < 4; ++i) {
            if (!addHexDigit(*json, ch))
                return false;
            ++json;
        }
        return true;
    }
    default:
        return false;
    }
    return true;
}


bool JsonParser::parseString(QString *string)
{
    BEGIN << "parse string stringPos=" << json;

    while (json < end) {
        if (*json == '"')
            break;
        else if (*json == '\\') {
            uint ch = 0;
            if (!scanEscapeSequence(json, end, &ch)) {
                lastError = QJsonParseError::IllegalEscapeSequence;
                return false;
            }
            if (QChar::requiresSurrogates(ch)) {
                *string += QChar(QChar::highSurrogate(ch)) + QChar(QChar::lowSurrogate(ch));
            } else {
                *string += QChar(ch);
            }
        } else {
            if (json->unicode() <= 0x1f) {
                lastError = QJsonParseError::IllegalEscapeSequence;
                return false;
            }
            *string += *json;
            ++json;
        }
    }
    ++json;

    if (json > end) {
        lastError = QJsonParseError::UnterminatedString;
        return false;
    }

    END;
    return true;
}


struct Stringify
{
    ExecutionEngine *v4;
    FunctionObject *replacerFunction;
    QV4::String *propertyList;
    int propertyListSize;
    QString gap;
    QString indent;
    QStack<Object *> stack;

    bool stackContains(Object *o) {
        for (int i = 0; i < stack.size(); ++i)
            if (stack.at(i)->d() == o->d())
                return true;
        return false;
    }

    Stringify(ExecutionEngine *e) : v4(e), replacerFunction(0), propertyList(0), propertyListSize(0) {}

    QString Str(const QString &key, const Value &v);
    QString JA(ArrayObject *a);
    QString JO(Object *o);

    QString makeMember(const QString &key, const Value &v);
};

static QString quote(const QString &str)
{
    QString product;
    const int length = str.length();
    product.reserve(length + 2);
    product += QLatin1Char('"');
    for (int i = 0; i < length; ++i) {
        QChar c = str.at(i);
        switch (c.unicode()) {
        case '"':
            product += QLatin1String("\\\"");
            break;
        case '\\':
            product += QLatin1String("\\\\");
            break;
        case '\b':
            product += QLatin1String("\\b");
            break;
        case '\f':
            product += QLatin1String("\\f");
            break;
        case '\n':
            product += QLatin1String("\\n");
            break;
        case '\r':
            product += QLatin1String("\\r");
            break;
        case '\t':
            product += QLatin1String("\\t");
            break;
        default:
            if (c.unicode() <= 0x1f) {
                product += QLatin1String("\\u00");
                product += (c.unicode() > 0xf ? QLatin1Char('1') : QLatin1Char('0')) +
                        QLatin1Char("0123456789abcdef"[c.unicode() & 0xf]);
            } else {
                product += c;
            }
        }
    }
    product += QLatin1Char('"');
    return product;
}

QString Stringify::Str(const QString &key, const Value &v)
{
    Scope scope(v4);
    scope.result = v;

    ScopedObject o(scope, scope.result);
    if (o) {
        ScopedString s(scope, v4->newString(QStringLiteral("toJSON")));
        ScopedFunctionObject toJSON(scope, o->get(s));
        if (!!toJSON) {
            ScopedCallData callData(scope, 1);
            callData->thisObject = scope.result;
            callData->args[0] = v4->newString(key);
            toJSON->call(scope, callData);
        }
    }

    if (replacerFunction) {
        ScopedObject holder(scope, v4->newObject());
        holder->put(scope.engine, QString(), scope.result);
        ScopedCallData callData(scope, 2);
        callData->args[0] = v4->newString(key);
        callData->args[1] = scope.result;
        callData->thisObject = holder;
        replacerFunction->call(scope, callData);
    }

    o = scope.result.asReturnedValue();
    if (o) {
        if (NumberObject *n = o->as<NumberObject>())
            scope.result = Encode(n->value());
        else if (StringObject *so = o->as<StringObject>())
            scope.result = so->d()->string;
        else if (BooleanObject *b = o->as<BooleanObject>())
            scope.result = Encode(b->value());
    }

    if (scope.result.isNull())
        return QStringLiteral("null");
    if (scope.result.isBoolean())
        return scope.result.booleanValue() ? QStringLiteral("true") : QStringLiteral("false");
    if (scope.result.isString())
        return quote(scope.result.stringValue()->toQString());

    if (scope.result.isNumber()) {
        double d = scope.result.toNumber();
        return std::isfinite(d) ? scope.result.toQString() : QStringLiteral("null");
    }

    if (const QV4::VariantObject *v = scope.result.as<QV4::VariantObject>()) {
        return v->d()->data().toString();
    }

    o = scope.result.asReturnedValue();
    if (o) {
        if (!o->as<FunctionObject>()) {
            if (o->as<ArrayObject>()) {
                return JA(static_cast<ArrayObject *>(o.getPointer()));
            } else {
                return JO(o);
            }
        }
    }

    return QString();
}

QString Stringify::makeMember(const QString &key, const Value &v)
{
    QString strP = Str(key, v);
    if (!strP.isEmpty()) {
        QString member = quote(key) + QLatin1Char(':');
        if (!gap.isEmpty())
            member += QLatin1Char(' ');
        member += strP;
        return member;
    }
    return QString();
}

QString Stringify::JO(Object *o)
{
    if (stackContains(o)) {
        v4->throwTypeError();
        return QString();
    }

    Scope scope(v4);

    QString result;
    stack.push(o);
    QString stepback = indent;
    indent += gap;

    QStringList partial;
    if (!propertyListSize) {
        ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
        ScopedValue name(scope);

        ScopedValue val(scope);
        while (1) {
            name = it.nextPropertyNameAsString(val);
            if (name->isNull())
                break;
            QString key = name->toQString();
            QString member = makeMember(key, val);
            if (!member.isEmpty())
                partial += member;
        }
    } else {
        ScopedValue v(scope);
        for (int i = 0; i < propertyListSize; ++i) {
            bool exists;
            String *s = propertyList + i;
            if (!s)
                continue;
            v = o->get(s, &exists);
            if (!exists)
                continue;
            QString member = makeMember(s->toQString(), v);
            if (!member.isEmpty())
                partial += member;
        }
    }

    if (partial.isEmpty()) {
        result = QStringLiteral("{}");
    } else if (gap.isEmpty()) {
        result = QLatin1Char('{') + partial.join(QLatin1Char(',')) + QLatin1Char('}');
    } else {
        QString separator = QLatin1String(",\n") + indent;
        result = QLatin1String("{\n") + indent + partial.join(separator) + QLatin1Char('\n')
                 + stepback + QLatin1Char('}');
    }

    indent = stepback;
    stack.pop();
    return result;
}

QString Stringify::JA(ArrayObject *a)
{
    if (stackContains(a)) {
        v4->throwTypeError();
        return QString();
    }

    Scope scope(a->engine());

    QString result;
    stack.push(a);
    QString stepback = indent;
    indent += gap;

    QStringList partial;
    uint len = a->getLength();
    ScopedValue v(scope);
    for (uint i = 0; i < len; ++i) {
        bool exists;
        v = a->getIndexed(i, &exists);
        if (!exists) {
            partial += QStringLiteral("null");
            continue;
        }
        QString strP = Str(QString::number(i), v);
        if (!strP.isEmpty())
            partial += strP;
        else
            partial += QStringLiteral("null");
    }

    if (partial.isEmpty()) {
        result = QStringLiteral("[]");
    } else if (gap.isEmpty()) {
        result = QLatin1Char('[') + partial.join(QLatin1Char(',')) + QLatin1Char(']');
    } else {
        QString separator = QLatin1String(",\n") + indent;
        result = QLatin1String("[\n") + indent + partial.join(separator) + QLatin1Char('\n') + stepback + QLatin1Char(']');
    }

    indent = stepback;
    stack.pop();
    return result;
}


void Heap::JsonObject::init()
{
    Object::init();
    Scope scope(internalClass->engine);
    ScopedObject o(scope, this);

    o->defineDefaultProperty(QStringLiteral("parse"), QV4::JsonObject::method_parse, 2);
    o->defineDefaultProperty(QStringLiteral("stringify"), QV4::JsonObject::method_stringify, 3);
}


ReturnedValue JsonObject::method_parse(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, ctx->argument(0));
    QString jtext = v->toQString();

    DEBUG << "parsing source = " << jtext;
    JsonParser parser(scope.engine, jtext.constData(), jtext.length());
    QJsonParseError error;
    ScopedValue result(scope, parser.parse(&error));
    if (error.error != QJsonParseError::NoError) {
        DEBUG << "parse error" << error.errorString();
        return ctx->engine()->throwSyntaxError(QStringLiteral("JSON.parse: Parse error"));
    }

    return result->asReturnedValue();
}

ReturnedValue JsonObject::method_stringify(CallContext *ctx)
{
    Scope scope(ctx);

    Stringify stringify(scope.engine);

    ScopedObject o(scope, ctx->argument(1));
    if (o) {
        stringify.replacerFunction = o->as<FunctionObject>();
        if (o->isArrayObject()) {
            uint arrayLen = o->getLength();
            stringify.propertyList = static_cast<QV4::String *>(scope.alloc(arrayLen));
            for (uint i = 0; i < arrayLen; ++i) {
                Value *v = stringify.propertyList + i;
                *v = o->getIndexed(i);
                if (v->as<NumberObject>() || v->as<StringObject>() || v->isNumber())
                    *v = RuntimeHelpers::toString(scope.engine, *v);
                if (!v->isString()) {
                    v->setM(0);
                } else {
                    for (uint j = 0; j <i; ++j) {
                        if (stringify.propertyList[j].m() == v->m()) {
                            v->setM(0);
                            break;
                        }
                    }
                }
            }
        }
    }

    ScopedValue s(scope, ctx->argument(2));
    if (NumberObject *n = s->as<NumberObject>())
        s = Encode(n->value());
    else if (StringObject *so = s->as<StringObject>())
        s = so->d()->string;

    if (s->isNumber()) {
        stringify.gap = QString(qMin(10, (int)s->toInteger()), ' ');
    } else if (s->isString()) {
        stringify.gap = s->stringValue()->toQString().left(10);
    }


    ScopedValue arg0(scope, ctx->argument(0));
    QString result = stringify.Str(QString(), arg0);
    if (result.isEmpty() || scope.engine->hasException)
        return Encode::undefined();
    return ctx->d()->engine->newString(result)->asReturnedValue();
}



ReturnedValue JsonObject::fromJsonValue(ExecutionEngine *engine, const QJsonValue &value)
{
    if (value.isString())
        return engine->newString(value.toString())->asReturnedValue();
    else if (value.isDouble())
        return Encode(value.toDouble());
    else if (value.isBool())
        return Encode(value.toBool());
    else if (value.isArray())
        return fromJsonArray(engine, value.toArray());
    else if (value.isObject())
        return fromJsonObject(engine, value.toObject());
    else if (value.isNull())
        return Encode::null();
    else
        return Encode::undefined();
}

QJsonValue JsonObject::toJsonValue(const Value &value, V4ObjectSet &visitedObjects)
{
    if (value.isNumber())
        return QJsonValue(value.toNumber());
    else if (value.isBoolean())
        return QJsonValue((bool)value.booleanValue());
    else if (value.isNull())
        return QJsonValue(QJsonValue::Null);
    else if (value.isUndefined())
        return QJsonValue(QJsonValue::Undefined);
    else if (value.isString())
        return QJsonValue(value.toQString());

    Q_ASSERT(value.isObject());
    Scope scope(value.as<Object>()->engine());
    ScopedArrayObject a(scope, value);
    if (a)
        return toJsonArray(a, visitedObjects);
    ScopedObject o(scope, value);
    if (o)
        return toJsonObject(o, visitedObjects);
    return QJsonValue(value.toQString());
}

QV4::ReturnedValue JsonObject::fromJsonObject(ExecutionEngine *engine, const QJsonObject &object)
{
    Scope scope(engine);
    ScopedObject o(scope, engine->newObject());
    ScopedString s(scope);
    ScopedValue v(scope);
    for (QJsonObject::const_iterator it = object.begin(), cend = object.end(); it != cend; ++it) {
        v = fromJsonValue(engine, it.value());
        o->put((s = engine->newString(it.key())), v);
    }
    return o.asReturnedValue();
}

QJsonObject JsonObject::toJsonObject(const Object *o, V4ObjectSet &visitedObjects)
{
    QJsonObject result;
    if (!o || o->as<FunctionObject>())
        return result;

    Scope scope(o->engine());

    if (visitedObjects.contains(ObjectItem(o))) {
        // Avoid recursion.
        // For compatibility with QVariant{List,Map} conversion, we return an
        // empty object (and no error is thrown).
        return result;
    }

    visitedObjects.insert(ObjectItem(o));

    ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
    ScopedValue name(scope);
    QV4::ScopedValue val(scope);
    while (1) {
        name = it.nextPropertyNameAsString(val);
        if (name->isNull())
            break;

        QString key = name->toQStringNoThrow();
        if (!val->as<FunctionObject>())
            result.insert(key, toJsonValue(val, visitedObjects));
    }

    visitedObjects.remove(ObjectItem(o));

    return result;
}

QV4::ReturnedValue JsonObject::fromJsonArray(ExecutionEngine *engine, const QJsonArray &array)
{
    Scope scope(engine);
    int size = array.size();
    ScopedArrayObject a(scope, engine->newArrayObject());
    a->arrayReserve(size);
    ScopedValue v(scope);
    for (int i = 0; i < size; i++)
        a->arrayPut(i, (v = fromJsonValue(engine, array.at(i))));
    a->setArrayLengthUnchecked(size);
    return a.asReturnedValue();
}

QJsonArray JsonObject::toJsonArray(const ArrayObject *a, V4ObjectSet &visitedObjects)
{
    QJsonArray result;
    if (!a)
        return result;

    Scope scope(a->engine());

    if (visitedObjects.contains(ObjectItem(a))) {
        // Avoid recursion.
        // For compatibility with QVariant{List,Map} conversion, we return an
        // empty array (and no error is thrown).
        return result;
    }

    visitedObjects.insert(ObjectItem(a));

    ScopedValue v(scope);
    quint32 length = a->getLength();
    for (quint32 i = 0; i < length; ++i) {
        v = a->getIndexed(i);
        if (v->as<FunctionObject>())
            v = Encode::null();
        result.append(toJsonValue(v, visitedObjects));
    }

    visitedObjects.remove(ObjectItem(a));

    return result;
}
