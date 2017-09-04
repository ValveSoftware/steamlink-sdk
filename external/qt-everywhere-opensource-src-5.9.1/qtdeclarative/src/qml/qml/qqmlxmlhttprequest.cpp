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

#include "qqmlxmlhttprequest_p.h"

#include <private/qv8engine_p.h>

#include "qqmlengine.h"
#include "qqmlengine_p.h"
#include <private/qqmlrefcount_p.h>
#include "qqmlengine_p.h"
#include "qqmlexpression_p.h"
#include "qqmlglobal_p.h"
#include <private/qv4domerrors_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4scopedvalue_p.h>

#include <QtCore/qobject.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsengine.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qxmlstream.h>
#include <QtCore/qstack.h>
#include <QtCore/qdebug.h>
#include <QtCore/qbuffer.h>

#include <private/qv4objectproto_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4arraybuffer_p.h>
#include <private/qv4jsonobject_p.h>

using namespace QV4;

#if QT_CONFIG(xmlstreamreader) && QT_CONFIG(qml_network)

#define V4THROW_REFERENCE(string) \
    do { \
        ScopedObject error(scope, scope.engine->newReferenceErrorObject(QStringLiteral(string))); \
        scope.result = scope.engine->throwError(error); \
        return; \
    } while (false)

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(xhrDump, QML_XHR_DUMP);

struct QQmlXMLHttpRequestData {
    QQmlXMLHttpRequestData();
    ~QQmlXMLHttpRequestData();

    PersistentValue nodeFunction;

    PersistentValue nodePrototype;
    PersistentValue elementPrototype;
    PersistentValue attrPrototype;
    PersistentValue characterDataPrototype;
    PersistentValue textPrototype;
    PersistentValue cdataPrototype;
    PersistentValue documentPrototype;
};

static inline QQmlXMLHttpRequestData *xhrdata(ExecutionEngine *v4)
{
    return (QQmlXMLHttpRequestData *)v4->v8Engine->xmlHttpRequestData();
}

QQmlXMLHttpRequestData::QQmlXMLHttpRequestData()
{
}

QQmlXMLHttpRequestData::~QQmlXMLHttpRequestData()
{
}

namespace QV4 {

class DocumentImpl;
class NodeImpl
{
public:
    NodeImpl() : type(Element), document(0), parent(0) {}
    virtual ~NodeImpl() {
        qDeleteAll(children);
        qDeleteAll(attributes);
    }

    // These numbers are copied from the Node IDL definition
    enum Type {
        Attr = 2,
        CDATA = 4,
        Comment = 8,
        Document = 9,
        DocumentFragment = 11,
        DocumentType = 10,
        Element = 1,
        Entity = 6,
        EntityReference = 5,
        Notation = 12,
        ProcessingInstruction = 7,
        Text = 3
    };
    Type type;

    QString namespaceUri;
    QString name;

    QString data;

    void addref();
    void release();

    DocumentImpl *document;
    NodeImpl *parent;

    QList<NodeImpl *> children;
    QList<NodeImpl *> attributes;
};

class DocumentImpl : public QQmlRefCount, public NodeImpl
{
public:
    DocumentImpl() : root(0) { type = Document; }
    virtual ~DocumentImpl() {
        delete root;
    }

    QString version;
    QString encoding;
    bool isStandalone;

    NodeImpl *root;

    void addref() { QQmlRefCount::addref(); }
    void release() { QQmlRefCount::release(); }
};

namespace Heap {

struct NamedNodeMap : Object {
    void init(NodeImpl *data, const QList<NodeImpl *> &list);
    void destroy() {
        delete listPtr;
        if (d)
            d->release();
        Object::destroy();
    }
    QList<NodeImpl *> &list() {
        if (listPtr == nullptr)
            listPtr = new QList<NodeImpl *>;
        return *listPtr;
    }

    QList<NodeImpl *> *listPtr; // Only used in NamedNodeMap
    NodeImpl *d;
};

struct NodeList : Object {
    void init(NodeImpl *data);
    void destroy() {
        if (d)
            d->release();
        Object::destroy();
    }
    NodeImpl *d;
};

struct NodePrototype : Object {
    void init();
};

struct Node : Object {
    void init(NodeImpl *data);
    void destroy() {
        if (d)
            d->release();
        Object::destroy();
    }
    NodeImpl *d;
};

}

class NamedNodeMap : public Object
{
public:
    V4_OBJECT2(NamedNodeMap, Object)
    V4_NEEDS_DESTROY

    // C++ API
    static ReturnedValue create(ExecutionEngine *, NodeImpl *, const QList<NodeImpl *> &);

    // JS API
    static ReturnedValue get(const Managed *m, String *name, bool *hasProperty);
    static ReturnedValue getIndexed(const Managed *m, uint index, bool *hasProperty);
};

void Heap::NamedNodeMap::init(NodeImpl *data, const QList<NodeImpl *> &list)
{
    Object::init();
    d = data;
    this->list() = list;
    if (d)
        d->addref();
}

DEFINE_OBJECT_VTABLE(NamedNodeMap);

class NodeList : public Object
{
public:
    V4_OBJECT2(NodeList, Object)
    V4_NEEDS_DESTROY

    // JS API
    static ReturnedValue get(const Managed *m, String *name, bool *hasProperty);
    static ReturnedValue getIndexed(const Managed *m, uint index, bool *hasProperty);

    // C++ API
    static ReturnedValue create(ExecutionEngine *, NodeImpl *);

};

void Heap::NodeList::init(NodeImpl *data)
{
    Object::init();
    d = data;
    if (d)
        d->addref();
}

DEFINE_OBJECT_VTABLE(NodeList);

class NodePrototype : public Object
{
public:
    V4_OBJECT2(NodePrototype, Object)

    static void initClass(ExecutionEngine *engine);

    // JS API
    static void method_get_nodeName(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_nodeValue(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_nodeType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_namespaceUri(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    static void method_get_parentNode(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_childNodes(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_firstChild(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_lastChild(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_previousSibling(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_nextSibling(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_attributes(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    //static void ownerDocument(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    //static void namespaceURI(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    //static void prefix(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    //static void localName(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    //static void baseURI(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    //static void textContent(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    static ReturnedValue getProto(ExecutionEngine *v4);

};

void Heap::NodePrototype::init()
{
    Object::init();
    Scope scope(internalClass->engine);
    ScopedObject o(scope, this);

    o->defineAccessorProperty(QStringLiteral("nodeName"), QV4::NodePrototype::method_get_nodeName, 0);
    o->defineAccessorProperty(QStringLiteral("nodeValue"), QV4::NodePrototype::method_get_nodeValue, 0);
    o->defineAccessorProperty(QStringLiteral("nodeType"), QV4::NodePrototype::method_get_nodeType, 0);
    o->defineAccessorProperty(QStringLiteral("namespaceUri"), QV4::NodePrototype::method_get_namespaceUri, 0);

    o->defineAccessorProperty(QStringLiteral("parentNode"), QV4::NodePrototype::method_get_parentNode, 0);
    o->defineAccessorProperty(QStringLiteral("childNodes"), QV4::NodePrototype::method_get_childNodes, 0);
    o->defineAccessorProperty(QStringLiteral("firstChild"), QV4::NodePrototype::method_get_firstChild, 0);
    o->defineAccessorProperty(QStringLiteral("lastChild"), QV4::NodePrototype::method_get_lastChild, 0);
    o->defineAccessorProperty(QStringLiteral("previousSibling"), QV4::NodePrototype::method_get_previousSibling, 0);
    o->defineAccessorProperty(QStringLiteral("nextSibling"), QV4::NodePrototype::method_get_nextSibling, 0);
    o->defineAccessorProperty(QStringLiteral("attributes"), QV4::NodePrototype::method_get_attributes, 0);
}


DEFINE_OBJECT_VTABLE(NodePrototype);

struct Node : public Object
{
    V4_OBJECT2(Node, Object)
    V4_NEEDS_DESTROY

    // C++ API
    static ReturnedValue create(ExecutionEngine *v4, NodeImpl *);

    bool isNull() const;
};

void Heap::Node::init(NodeImpl *data)
{
    Object::init();
    d = data;
    if (d)
        d->addref();
}

DEFINE_OBJECT_VTABLE(Node);

class Element : public Node
{
public:
    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
};

class Attr : public Node
{
public:
    // JS API
    static void method_name(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
//    static void specified(CallContext *);
    static void method_value(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_ownerElement(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
//    static void schemaTypeInfo(CallContext *);
//    static void isId(CallContext *c);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
};

class CharacterData : public Node
{
public:
    // JS API
    static void method_length(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *v4);
};

class Text : public CharacterData
{
public:
    // JS API
    static void method_isElementContentWhitespace(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_wholeText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
};

class CDATA : public Text
{
public:
    // C++ API
    static ReturnedValue prototype(ExecutionEngine *v4);
};

class Document : public Node
{
public:
    // JS API
    static void method_xmlVersion(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_xmlEncoding(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_xmlStandalone(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_documentElement(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
    static ReturnedValue load(ExecutionEngine *engine, const QByteArray &data);
};

}

void NodeImpl::addref()
{
    document->addref();
}

void NodeImpl::release()
{
    document->release();
}

void NodePrototype::method_get_nodeName(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    QString name;
    switch (r->d()->d->type) {
    case NodeImpl::Document:
        name = QStringLiteral("#document");
        break;
    case NodeImpl::CDATA:
        name = QStringLiteral("#cdata-section");
        break;
    case NodeImpl::Text:
        name = QStringLiteral("#text");
        break;
    default:
        name = r->d()->d->name;
        break;
    }
    scope.result = Encode(scope.engine->newString(name));
}

void NodePrototype::method_get_nodeValue(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (r->d()->d->type == NodeImpl::Document ||
        r->d()->d->type == NodeImpl::DocumentFragment ||
        r->d()->d->type == NodeImpl::DocumentType ||
        r->d()->d->type == NodeImpl::Element ||
        r->d()->d->type == NodeImpl::Entity ||
        r->d()->d->type == NodeImpl::EntityReference ||
        r->d()->d->type == NodeImpl::Notation)
        RETURN_RESULT(Encode::null());

    scope.result = Encode(scope.engine->newString(r->d()->d->data));
}

void NodePrototype::method_get_nodeType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    scope.result = Encode(r->d()->d->type);
}

void NodePrototype::method_get_namespaceUri(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    scope.result = Encode(scope.engine->newString(r->d()->d->namespaceUri));
}

void NodePrototype::method_get_parentNode(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (r->d()->d->parent)
        scope.result = Node::create(scope.engine, r->d()->d->parent);
    else
        scope.result = Encode::null();
}

void NodePrototype::method_get_childNodes(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    scope.result = NodeList::create(scope.engine, r->d()->d);
}

void NodePrototype::method_get_firstChild(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (r->d()->d->children.isEmpty())
        scope.result = Encode::null();
    else
        scope.result = Node::create(scope.engine, r->d()->d->children.constFirst());
}

void NodePrototype::method_get_lastChild(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (r->d()->d->children.isEmpty())
        scope.result = Encode::null();
    else
        scope.result = Node::create(scope.engine, r->d()->d->children.constLast());
}

void NodePrototype::method_get_previousSibling(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (!r->d()->d->parent)
        RETURN_RESULT(Encode::null());

    for (int ii = 0; ii < r->d()->d->parent->children.count(); ++ii) {
        if (r->d()->d->parent->children.at(ii) == r->d()->d) {
            if (ii == 0)
                scope.result = Encode::null();
            else
                scope.result = Node::create(scope.engine, r->d()->d->parent->children.at(ii - 1));
            return;
        }
    }

    scope.result = Encode::null();
}

void NodePrototype::method_get_nextSibling(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (!r->d()->d->parent)
        RETURN_RESULT(Encode::null());

    for (int ii = 0; ii < r->d()->d->parent->children.count(); ++ii) {
        if (r->d()->d->parent->children.at(ii) == r->d()->d) {
            if ((ii + 1) == r->d()->d->parent->children.count())
                scope.result = Encode::null();
            else
                scope.result = Node::create(scope.engine, r->d()->d->parent->children.at(ii + 1));
            return;
        }
    }

    scope.result = Encode::null();
}

void NodePrototype::method_get_attributes(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        THROW_TYPE_ERROR();

    if (r->d()->d->type != NodeImpl::Element)
        scope.result = Encode::null();
    else
        scope.result = NamedNodeMap::create(scope.engine, r->d()->d, r->d()->d->attributes);
}

ReturnedValue NodePrototype::getProto(ExecutionEngine *v4)
{
    Scope scope(v4);
    QQmlXMLHttpRequestData *d = xhrdata(v4);
    if (d->nodePrototype.isUndefined()) {
        ScopedObject p(scope, v4->memoryManager->allocObject<NodePrototype>());
        d->nodePrototype.set(v4, p);
        v4->v8Engine->freezeObject(p);
    }
    return d->nodePrototype.value();
}

ReturnedValue Node::create(ExecutionEngine *v4, NodeImpl *data)
{
    Scope scope(v4);

    Scoped<Node> instance(scope, v4->memoryManager->allocObject<Node>(data));
    ScopedObject p(scope);

    switch (data->type) {
    case NodeImpl::Attr:
        instance->setPrototype((p = Attr::prototype(v4)));
        break;
    case NodeImpl::Comment:
    case NodeImpl::Document:
    case NodeImpl::DocumentFragment:
    case NodeImpl::DocumentType:
    case NodeImpl::Entity:
    case NodeImpl::EntityReference:
    case NodeImpl::Notation:
    case NodeImpl::ProcessingInstruction:
        return Encode::undefined();
    case NodeImpl::CDATA:
        instance->setPrototype((p = CDATA::prototype(v4)));
        break;
    case NodeImpl::Text:
        instance->setPrototype((p = Text::prototype(v4)));
        break;
    case NodeImpl::Element:
        instance->setPrototype((p = Element::prototype(v4)));
        break;
    }

    return instance.asReturnedValue();
}

ReturnedValue Element::prototype(ExecutionEngine *engine)
{
    QQmlXMLHttpRequestData *d = xhrdata(engine);
    if (d->elementPrototype.isUndefined()) {
        Scope scope(engine);
        ScopedObject p(scope, engine->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(engine)));
        p->defineAccessorProperty(QStringLiteral("tagName"), NodePrototype::method_get_nodeName, 0);
        d->elementPrototype.set(engine, p);
        engine->v8Engine->freezeObject(p);
    }
    return d->elementPrototype.value();
}

ReturnedValue Attr::prototype(ExecutionEngine *engine)
{
    QQmlXMLHttpRequestData *d = xhrdata(engine);
    if (d->attrPrototype.isUndefined()) {
        Scope scope(engine);
        ScopedObject p(scope, engine->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(engine)));
        p->defineAccessorProperty(QStringLiteral("name"), method_name, 0);
        p->defineAccessorProperty(QStringLiteral("value"), method_value, 0);
        p->defineAccessorProperty(QStringLiteral("ownerElement"), method_ownerElement, 0);
        d->attrPrototype.set(engine, p);
        engine->v8Engine->freezeObject(p);
    }
    return d->attrPrototype.value();
}

void Attr::method_name(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = scope.engine->newString(r->d()->d->name);
}

void Attr::method_value(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = scope.engine->newString(r->d()->d->data);
}

void Attr::method_ownerElement(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = Node::create(scope.engine, r->d()->d->parent);
}

void CharacterData::method_length(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = Encode(r->d()->d->data.length());
}

ReturnedValue CharacterData::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4);
    if (d->characterDataPrototype.isUndefined()) {
        Scope scope(v4);
        ScopedObject p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(v4)));
        p->defineAccessorProperty(QStringLiteral("data"), NodePrototype::method_get_nodeValue, 0);
        p->defineAccessorProperty(QStringLiteral("length"), method_length, 0);
        d->characterDataPrototype.set(v4, p);
        v4->v8Engine->freezeObject(p);
    }
    return d->characterDataPrototype.value();
}

void Text::method_isElementContentWhitespace(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = Encode(QStringRef(&r->d()->d->data).trimmed().isEmpty());
}

void Text::method_wholeText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r)
        RETURN_UNDEFINED();

    scope.result = scope.engine->newString(r->d()->d->data);
}

ReturnedValue Text::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4);
    if (d->textPrototype.isUndefined()) {
        Scope scope(v4);
        ScopedObject p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = CharacterData::prototype(v4)));
        p->defineAccessorProperty(QStringLiteral("isElementContentWhitespace"), method_isElementContentWhitespace, 0);
        p->defineAccessorProperty(QStringLiteral("wholeText"), method_wholeText, 0);
        d->textPrototype.set(v4, p);
        v4->v8Engine->freezeObject(p);
    }
    return d->textPrototype.value();
}

ReturnedValue CDATA::prototype(ExecutionEngine *v4)
{
    // ### why not just use TextProto???
    QQmlXMLHttpRequestData *d = xhrdata(v4);
    if (d->cdataPrototype.isUndefined()) {
        Scope scope(v4);
        ScopedObject p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = Text::prototype(v4)));
        d->cdataPrototype.set(v4, p);
        v4->v8Engine->freezeObject(p);
    }
    return d->cdataPrototype.value();
}

ReturnedValue Document::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4);
    if (d->documentPrototype.isUndefined()) {
        Scope scope(v4);
        ScopedObject p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(v4)));
        p->defineAccessorProperty(QStringLiteral("xmlVersion"), method_xmlVersion, 0);
        p->defineAccessorProperty(QStringLiteral("xmlEncoding"), method_xmlEncoding, 0);
        p->defineAccessorProperty(QStringLiteral("xmlStandalone"), method_xmlStandalone, 0);
        p->defineAccessorProperty(QStringLiteral("documentElement"), method_documentElement, 0);
        d->documentPrototype.set(v4, p);
        v4->v8Engine->freezeObject(p);
    }
    return d->documentPrototype.value();
}

ReturnedValue Document::load(ExecutionEngine *v4, const QByteArray &data)
{
    Scope scope(v4);

    DocumentImpl *document = 0;
    QStack<NodeImpl *> nodeStack;

    QXmlStreamReader reader(data);

    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::NoToken:
            break;
        case QXmlStreamReader::Invalid:
            break;
        case QXmlStreamReader::StartDocument:
            Q_ASSERT(!document);
            document = new DocumentImpl;
            document->document = document;
            document->version = reader.documentVersion().toString();
            document->encoding = reader.documentEncoding().toString();
            document->isStandalone = reader.isStandaloneDocument();
            break;
        case QXmlStreamReader::EndDocument:
            break;
        case QXmlStreamReader::StartElement:
        {
            Q_ASSERT(document);
            NodeImpl *node = new NodeImpl;
            node->document = document;
            node->namespaceUri = reader.namespaceUri().toString();
            node->name = reader.name().toString();
            if (nodeStack.isEmpty()) {
                document->root = node;
            } else {
                node->parent = nodeStack.top();
                node->parent->children.append(node);
            }
            nodeStack.append(node);

            const auto attributes = reader.attributes();
            for (const QXmlStreamAttribute &a : attributes) {
                NodeImpl *attr = new NodeImpl;
                attr->document = document;
                attr->type = NodeImpl::Attr;
                attr->namespaceUri = a.namespaceUri().toString();
                attr->name = a.name().toString();
                attr->data = a.value().toString();
                attr->parent = node;
                node->attributes.append(attr);
            }
        }
            break;
        case QXmlStreamReader::EndElement:
            nodeStack.pop();
            break;
        case QXmlStreamReader::Characters:
        {
            NodeImpl *node = new NodeImpl;
            node->document = document;
            node->type = reader.isCDATA()?NodeImpl::CDATA:NodeImpl::Text;
            node->parent = nodeStack.top();
            node->parent->children.append(node);
            node->data = reader.text().toString();
        }
            break;
        case QXmlStreamReader::Comment:
            break;
        case QXmlStreamReader::DTD:
            break;
        case QXmlStreamReader::EntityReference:
            break;
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    }

    if (!document || reader.hasError()) {
        if (document)
            document->release();
        return Encode::null();
    }

    ScopedObject instance(scope, v4->memoryManager->allocObject<Node>(document));
    document->release(); // the GC should own the NodeImpl via Node now
    ScopedObject p(scope);
    instance->setPrototype((p = Document::prototype(v4)));
    return instance.asReturnedValue();
}

bool Node::isNull() const
{
    return d()->d == 0;
}

ReturnedValue NamedNodeMap::getIndexed(const Managed *m, uint index, bool *hasProperty)
{
    Q_ASSERT(m->as<NamedNodeMap>());
    const NamedNodeMap *r = static_cast<const NamedNodeMap *>(m);
    QV4::ExecutionEngine *v4 = r->engine();

    if ((int)index < r->d()->list().count()) {
        if (hasProperty)
            *hasProperty = true;
        return Node::create(v4, r->d()->list().at(index));
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NamedNodeMap::get(const Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<NamedNodeMap>());
    const NamedNodeMap *r = static_cast<const NamedNodeMap *>(m);
    QV4::ExecutionEngine *v4 = r->engine();

    name->makeIdentifier();
    if (name->equals(v4->id_length()))
        return Primitive::fromInt32(r->d()->list().count()).asReturnedValue();

    QString str = name->toQString();
    for (int ii = 0; ii < r->d()->list().count(); ++ii) {
        if (r->d()->list().at(ii)->name == str) {
            if (hasProperty)
                *hasProperty = true;
            return Node::create(v4, r->d()->list().at(ii));
        }
    }

    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NamedNodeMap::create(ExecutionEngine *v4, NodeImpl *data, const QList<NodeImpl *> &list)
{
    return (v4->memoryManager->allocObject<NamedNodeMap>(data, list))->asReturnedValue();
}

ReturnedValue NodeList::getIndexed(const Managed *m, uint index, bool *hasProperty)
{
    Q_ASSERT(m->as<NodeList>());
    const NodeList *r = static_cast<const NodeList *>(m);
    QV4::ExecutionEngine *v4 = r->engine();

    if ((int)index < r->d()->d->children.count()) {
        if (hasProperty)
            *hasProperty = true;
        return Node::create(v4, r->d()->d->children.at(index));
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NodeList::get(const Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<NodeList>());
    const NodeList *r = static_cast<const NodeList *>(m);
    QV4::ExecutionEngine *v4 = r->engine();

    name->makeIdentifier();

    if (name->equals(v4->id_length()))
        return Primitive::fromInt32(r->d()->d->children.count()).asReturnedValue();
    return Object::get(m, name, hasProperty);
}

ReturnedValue NodeList::create(ExecutionEngine *v4, NodeImpl *data)
{
    return (v4->memoryManager->allocObject<NodeList>(data))->asReturnedValue();
}

void Document::method_documentElement(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        RETURN_UNDEFINED();

    scope.result = Node::create(scope.engine, static_cast<DocumentImpl *>(r->d()->d)->root);
}

void Document::method_xmlStandalone(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        RETURN_UNDEFINED();

    scope.result = Encode(static_cast<DocumentImpl *>(r->d()->d)->isStandalone);
}

void Document::method_xmlVersion(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        RETURN_UNDEFINED();

    scope.result = scope.engine->newString(static_cast<DocumentImpl *>(r->d()->d)->version);
}

void Document::method_xmlEncoding(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<Node> r(scope, callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        RETURN_UNDEFINED();

    scope.result = scope.engine->newString(static_cast<DocumentImpl *>(r->d()->d)->encoding);
}

class QQmlXMLHttpRequest : public QObject
{
    Q_OBJECT
public:
    enum LoadType {
        AsynchronousLoad,
        SynchronousLoad
    };
    enum State { Unsent = 0,
                 Opened = 1, HeadersReceived = 2,
                 Loading = 3, Done = 4 };

    QQmlXMLHttpRequest(QNetworkAccessManager *manager);
    virtual ~QQmlXMLHttpRequest();

    bool sendFlag() const;
    bool errorFlag() const;
    quint32 readyState() const;
    int replyStatus() const;
    QString replyStatusText() const;

    ReturnedValue open(Object *thisObject, QQmlContextData *context, const QString &, const QUrl &, LoadType);
    ReturnedValue send(Object *thisObject, QQmlContextData *context, const QByteArray &);
    ReturnedValue abort(Object *thisObject, QQmlContextData *context);

    void addHeader(const QString &, const QString &);
    QString header(const QString &name) const;
    QString headers() const;

    QString responseBody();
    const QByteArray & rawResponseBody() const;
    bool receivedXml() const;

    const QString & responseType() const;
    void setResponseType(const QString &);

    QV4::ReturnedValue jsonResponseBody(QV4::ExecutionEngine*);
    QV4::ReturnedValue xmlResponseBody(QV4::ExecutionEngine*);
private slots:
    void readyRead();
    void error(QNetworkReply::NetworkError);
    void finished();

private:
    void requestFromUrl(const QUrl &url);

    State m_state;
    bool m_errorFlag;
    bool m_sendFlag;
    QString m_method;
    QUrl m_url;
    QByteArray m_responseEntityBody;
    QByteArray m_data;
    int m_redirectCount;

    typedef QPair<QByteArray, QByteArray> HeaderPair;
    typedef QList<HeaderPair> HeadersList;
    HeadersList m_headersList;
    void fillHeadersList();

    bool m_gotXml;
    QByteArray m_mime;
    QByteArray m_charset;
    QTextCodec *m_textCodec;
#if QT_CONFIG(textcodec)
    QTextCodec* findTextCodec() const;
#endif
    void readEncoding();

    PersistentValue m_thisObject;
    QQmlGuardedContextData m_qmlContext;

    static void dispatchCallback(Object *thisObj, QQmlContextData *context);
    void dispatchCallback();

    int m_status;
    QString m_statusText;
    QNetworkRequest m_request;
    QStringList m_addedHeaders;
    QPointer<QNetworkReply> m_network;
    void destroyNetwork();

    QNetworkAccessManager *m_nam;
    QNetworkAccessManager *networkAccessManager() { return m_nam; }

    QString m_responseType;
    QV4::PersistentValue m_parsedDocument;
};

QQmlXMLHttpRequest::QQmlXMLHttpRequest(QNetworkAccessManager *manager)
    : m_state(Unsent), m_errorFlag(false), m_sendFlag(false)
    , m_redirectCount(0), m_gotXml(false), m_textCodec(0), m_network(0), m_nam(manager)
    , m_responseType()
    , m_parsedDocument()
{
}

QQmlXMLHttpRequest::~QQmlXMLHttpRequest()
{
    destroyNetwork();
}

bool QQmlXMLHttpRequest::sendFlag() const
{
    return m_sendFlag;
}

bool QQmlXMLHttpRequest::errorFlag() const
{
    return m_errorFlag;
}

quint32 QQmlXMLHttpRequest::readyState() const
{
    return m_state;
}

int QQmlXMLHttpRequest::replyStatus() const
{
    return m_status;
}

QString QQmlXMLHttpRequest::replyStatusText() const
{
    return m_statusText;
}

ReturnedValue QQmlXMLHttpRequest::open(Object *thisObject, QQmlContextData *context, const QString &method, const QUrl &url, LoadType loadType)
{
    destroyNetwork();
    m_sendFlag = false;
    m_errorFlag = false;
    m_responseEntityBody = QByteArray();
    m_method = method;
    m_url = url;
    m_request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, loadType == SynchronousLoad);
    m_state = Opened;
    m_addedHeaders.clear();
    dispatchCallback(thisObject, context);
    return Encode::undefined();
}

void QQmlXMLHttpRequest::addHeader(const QString &name, const QString &value)
{
    QByteArray utfname = name.toUtf8();

    if (m_addedHeaders.contains(name, Qt::CaseInsensitive)) {
        m_request.setRawHeader(utfname, m_request.rawHeader(utfname) + ',' + value.toUtf8());
    } else {
        m_request.setRawHeader(utfname, value.toUtf8());
        m_addedHeaders.append(name);
    }
}

QString QQmlXMLHttpRequest::header(const QString &name) const
{
    if (!m_headersList.isEmpty()) {
        const QByteArray utfname = name.toLower().toUtf8();
        for (const HeaderPair &header : m_headersList) {
            if (header.first == utfname)
                return QString::fromUtf8(header.second);
        }
    }
    return QString();
}

QString QQmlXMLHttpRequest::headers() const
{
    QString ret;

    for (const HeaderPair &header : m_headersList) {
        if (ret.length())
            ret.append(QLatin1String("\r\n"));
        ret += QString::fromUtf8(header.first) + QLatin1String(": ")
             + QString::fromUtf8(header.second);
    }
    return ret;
}

void QQmlXMLHttpRequest::fillHeadersList()
{
    const QList<QByteArray> headerList = m_network->rawHeaderList();

    m_headersList.clear();
    for (const QByteArray &header : headerList) {
        HeaderPair pair (header.toLower(), m_network->rawHeader(header));
        if (pair.first == "set-cookie" ||
            pair.first == "set-cookie2")
            continue;

        m_headersList << pair;
    }
}

void QQmlXMLHttpRequest::requestFromUrl(const QUrl &url)
{
    QNetworkRequest request = m_request;
    request.setUrl(url);
    if(m_method == QLatin1String("POST") ||
       m_method == QLatin1String("PUT")) {
        QVariant var = request.header(QNetworkRequest::ContentTypeHeader);
        if (var.isValid()) {
            QString str = var.toString();
            int charsetIdx = str.indexOf(QLatin1String("charset="));
            if (charsetIdx == -1) {
                // No charset - append
                if (!str.isEmpty()) str.append(QLatin1Char(';'));
                str.append(QLatin1String("charset=UTF-8"));
            } else {
                charsetIdx += 8;
                int n = 0;
                int semiColon = str.indexOf(QLatin1Char(';'), charsetIdx);
                if (semiColon == -1) {
                    n = str.length() - charsetIdx;
                } else {
                    n = semiColon - charsetIdx;
                }

                str.replace(charsetIdx, n, QLatin1String("UTF-8"));
            }
            request.setHeader(QNetworkRequest::ContentTypeHeader, str);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              QLatin1String("text/plain;charset=UTF-8"));
        }
    }

    if (xhrDump()) {
        qWarning().nospace() << "XMLHttpRequest: " << qPrintable(m_method) << ' ' << qPrintable(url.toString());
        if (!m_data.isEmpty()) {
            qWarning().nospace() << "                "
                                 << qPrintable(QString::fromUtf8(m_data));
        }
    }

    if (m_method == QLatin1String("GET")) {
        m_network = networkAccessManager()->get(request);
    } else if (m_method == QLatin1String("HEAD")) {
        m_network = networkAccessManager()->head(request);
    } else if (m_method == QLatin1String("POST")) {
        m_network = networkAccessManager()->post(request, m_data);
    } else if (m_method == QLatin1String("PUT")) {
        m_network = networkAccessManager()->put(request, m_data);
    } else if (m_method == QLatin1String("DELETE")) {
        m_network = networkAccessManager()->deleteResource(request);
    } else if ((m_method == QLatin1String("OPTIONS")) ||
               m_method == QLatin1String("PROPFIND") ||
               m_method == QLatin1String("PATCH")) {
        QBuffer *buffer = new QBuffer;
        buffer->setData(m_data);
        buffer->open(QIODevice::ReadOnly);
        m_network = networkAccessManager()->sendCustomRequest(request, QByteArray(m_method.toUtf8().constData()), buffer);
        buffer->setParent(m_network);
    }

    if (m_request.attribute(QNetworkRequest::SynchronousRequestAttribute).toBool()) {
        if (m_network->bytesAvailable() > 0)
            readyRead();

        QNetworkReply::NetworkError networkError = m_network->error();
        if (networkError != QNetworkReply::NoError) {
            error(networkError);
        } else {
            finished();
        }
    } else {
        QObject::connect(m_network, SIGNAL(readyRead()),
                         this, SLOT(readyRead()));
        QObject::connect(m_network, SIGNAL(error(QNetworkReply::NetworkError)),
                         this, SLOT(error(QNetworkReply::NetworkError)));
        QObject::connect(m_network, SIGNAL(finished()),
                         this, SLOT(finished()));
    }
}

ReturnedValue QQmlXMLHttpRequest::send(Object *thisObject, QQmlContextData *context, const QByteArray &data)
{
    m_errorFlag = false;
    m_sendFlag = true;
    m_redirectCount = 0;
    m_data = data;

    m_thisObject = thisObject;
    m_qmlContext = context;

    requestFromUrl(m_url);

    return Encode::undefined();
}

ReturnedValue QQmlXMLHttpRequest::abort(Object *thisObject, QQmlContextData *context)
{
    destroyNetwork();
    m_responseEntityBody = QByteArray();
    m_errorFlag = true;
    m_request = QNetworkRequest();

    if (!(m_state == Unsent ||
          (m_state == Opened && !m_sendFlag) ||
          m_state == Done)) {

        m_state = Done;
        m_sendFlag = false;
        dispatchCallback(thisObject, context);
    }

    m_state = Unsent;

    return Encode::undefined();
}

void QQmlXMLHttpRequest::readyRead()
{
    m_status =
        m_network->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_statusText =
        QString::fromUtf8(m_network->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray());

    // ### We assume if this is called the headers are now available
    if (m_state < HeadersReceived) {
        m_state = HeadersReceived;
        fillHeadersList ();
        dispatchCallback();
    }

    bool wasEmpty = m_responseEntityBody.isEmpty();
    m_responseEntityBody.append(m_network->readAll());
    if (wasEmpty && !m_responseEntityBody.isEmpty())
        m_state = Loading;

    dispatchCallback();
}

static const char *errorToString(QNetworkReply::NetworkError error)
{
    int idx = QNetworkReply::staticMetaObject.indexOfEnumerator("NetworkError");
    if (idx == -1) return "EnumLookupFailed";

    QMetaEnum e = QNetworkReply::staticMetaObject.enumerator(idx);

    const char *name = e.valueToKey(error);
    if (!name) return "EnumLookupFailed";
    else return name;
}

void QQmlXMLHttpRequest::error(QNetworkReply::NetworkError error)
{
    m_status =
        m_network->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_statusText =
        QString::fromUtf8(m_network->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray());

    m_request = QNetworkRequest();
    m_data.clear();
    destroyNetwork();

    if (xhrDump()) {
        qWarning().nospace() << "XMLHttpRequest: ERROR " << qPrintable(m_url.toString());
        qWarning().nospace() << "    " << error << ' ' << errorToString(error) << ' ' << m_statusText;
    }

    if (error == QNetworkReply::ContentAccessDenied ||
        error == QNetworkReply::ContentOperationNotPermittedError ||
        error == QNetworkReply::ContentNotFoundError ||
        error == QNetworkReply::AuthenticationRequiredError ||
        error == QNetworkReply::ContentReSendError ||
        error == QNetworkReply::UnknownContentError ||
        error == QNetworkReply::ProtocolInvalidOperationError ||
        error == QNetworkReply::InternalServerError ||
        error == QNetworkReply::OperationNotImplementedError ||
        error == QNetworkReply::ServiceUnavailableError ||
        error == QNetworkReply::UnknownServerError) {
        m_state = Loading;
        dispatchCallback();
    } else {
        m_errorFlag = true;
        m_responseEntityBody = QByteArray();
    }

    m_state = Done;
    dispatchCallback();
}

#define XMLHTTPREQUEST_MAXIMUM_REDIRECT_RECURSION 15
void QQmlXMLHttpRequest::finished()
{
    m_redirectCount++;
    if (m_redirectCount < XMLHTTPREQUEST_MAXIMUM_REDIRECT_RECURSION) {
        QVariant redirect = m_network->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = m_network->url().resolved(redirect.toUrl());
            if (url.scheme() != QLatin1String("file")) {
                // See http://www.ietf.org/rfc/rfc2616.txt, section 10.3.4 "303 See Other":
                // Result of 303 redirection should be a new "GET" request.
                const QVariant code = m_network->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                if (code.isValid() && code.toInt() == 303 && m_method != QLatin1String("GET"))
                    m_method = QStringLiteral("GET");
                destroyNetwork();
                requestFromUrl(url);
                return;
            }
        }
    }

    m_status =
        m_network->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_statusText =
        QString::fromUtf8(m_network->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray());

    if (m_state < HeadersReceived) {
        m_state = HeadersReceived;
        fillHeadersList ();
        dispatchCallback();
    }
    m_responseEntityBody.append(m_network->readAll());
    readEncoding();

    if (xhrDump()) {
        qWarning().nospace() << "XMLHttpRequest: RESPONSE " << qPrintable(m_url.toString());
        if (!m_responseEntityBody.isEmpty()) {
            qWarning().nospace() << "                "
                                 << qPrintable(QString::fromUtf8(m_responseEntityBody));
        }
    }

    m_data.clear();
    destroyNetwork();
    if (m_state < Loading) {
        m_state = Loading;
        dispatchCallback();
    }
    m_state = Done;

    dispatchCallback();

    m_thisObject.clear();
    m_qmlContext.setContextData(0);
}


void QQmlXMLHttpRequest::readEncoding()
{
    for (const HeaderPair &header : qAsConst(m_headersList)) {
        if (header.first == "content-type") {
            int separatorIdx = header.second.indexOf(';');
            if (separatorIdx == -1) {
                m_mime = header.second;
            } else {
                m_mime = header.second.mid(0, separatorIdx);
                int charsetIdx = header.second.indexOf("charset=");
                if (charsetIdx != -1) {
                    charsetIdx += 8;
                    separatorIdx = header.second.indexOf(';', charsetIdx);
                    m_charset = header.second.mid(charsetIdx, separatorIdx >= 0 ? separatorIdx : header.second.length());
                }
            }
            break;
        }
    }

    if (m_mime.isEmpty() || m_mime == "text/xml" || m_mime == "application/xml" || m_mime.endsWith("+xml"))
        m_gotXml = true;
}

bool QQmlXMLHttpRequest::receivedXml() const
{
    return m_gotXml;
}

const QString & QQmlXMLHttpRequest::responseType() const
{
    return m_responseType;
}

void QQmlXMLHttpRequest::setResponseType(const QString &responseType)
{
    m_responseType = responseType;
}

QV4::ReturnedValue QQmlXMLHttpRequest::jsonResponseBody(QV4::ExecutionEngine* engine)
{
    if (m_parsedDocument.isEmpty()) {
        Scope scope(engine);

        QJsonParseError error;
        const QString& jtext = responseBody();
        JsonParser parser(scope.engine, jtext.constData(), jtext.length());
        ScopedValue jsonObject(scope, parser.parse(&error));
        if (error.error != QJsonParseError::NoError)
            return engine->throwSyntaxError(QStringLiteral("JSON.parse: Parse error"));

        m_parsedDocument.set(scope.engine, jsonObject);
    }

    return m_parsedDocument.value();
}

QV4::ReturnedValue QQmlXMLHttpRequest::xmlResponseBody(QV4::ExecutionEngine* engine)
{
    if (m_parsedDocument.isEmpty()) {
        m_parsedDocument.set(engine, Document::load(engine, rawResponseBody()));
    }

    return m_parsedDocument.value();
}

#if QT_CONFIG(textcodec)
QTextCodec* QQmlXMLHttpRequest::findTextCodec() const
{
    QTextCodec *codec = 0;

    if (!m_charset.isEmpty())
        codec = QTextCodec::codecForName(m_charset);

    if (!codec && m_gotXml) {
        QXmlStreamReader reader(m_responseEntityBody);
        reader.readNext();
        codec = QTextCodec::codecForName(reader.documentEncoding().toString().toUtf8());
    }

    if (!codec && m_mime == "text/html")
        codec = QTextCodec::codecForHtml(m_responseEntityBody, 0);

    if (!codec)
        codec = QTextCodec::codecForUtfText(m_responseEntityBody, 0);

    if (!codec)
        codec = QTextCodec::codecForName("UTF-8");
    return codec;
}
#endif


QString QQmlXMLHttpRequest::responseBody()
{
#if QT_CONFIG(textcodec)
    if (!m_textCodec)
        m_textCodec = findTextCodec();
    if (m_textCodec)
        return m_textCodec->toUnicode(m_responseEntityBody);
#endif

    return QString::fromUtf8(m_responseEntityBody);
}

const QByteArray &QQmlXMLHttpRequest::rawResponseBody() const
{
    return m_responseEntityBody;
}

void QQmlXMLHttpRequest::dispatchCallback(Object *thisObj, QQmlContextData *context)
{
    Q_ASSERT(thisObj);

    if (!context)
        // if the calling context object is no longer valid, then it has been
        // deleted explicitly (e.g., by a Loader deleting the itemContext when
        // the source is changed).  We do nothing in this case, as the evaluation
        // cannot succeed.
        return;

    QV4::Scope scope(thisObj->engine());
    ScopedString s(scope, scope.engine->newString(QStringLiteral("onreadystatechange")));
    ScopedFunctionObject callback(scope, thisObj->get(s));
    if (!callback) {
        // not an error, but no onreadystatechange function to call.
        return;
    }

    QV4::ScopedCallData callData(scope);
    callData->thisObject = Encode::undefined();
    callback->call(scope, callData);

    if (scope.engine->hasException) {
        QQmlError error = scope.engine->catchExceptionAsQmlError();
        QQmlEnginePrivate::warning(QQmlEnginePrivate::get(scope.engine->qmlEngine()), error);
    }
}

void QQmlXMLHttpRequest::dispatchCallback()
{
    dispatchCallback(m_thisObject.as<Object>(), m_qmlContext.contextData());
}

void QQmlXMLHttpRequest::destroyNetwork()
{
    if (m_network) {
        m_network->disconnect();
        m_network->deleteLater();
        m_network = 0;
    }
}

namespace QV4 {
namespace Heap {

struct QQmlXMLHttpRequestWrapper : Object {
    void init(QQmlXMLHttpRequest *request) {
        Object::init();
        this->request = request;
    }

    void destroy() {
        delete request;
        Object::destroy();
    }
    QQmlXMLHttpRequest *request;
};

struct QQmlXMLHttpRequestCtor : FunctionObject {
    void init(ExecutionEngine *engine);

    Pointer<Object> proto;
};

}

struct QQmlXMLHttpRequestWrapper : public Object
{
    V4_OBJECT2(QQmlXMLHttpRequestWrapper, Object)
    V4_NEEDS_DESTROY
};

struct QQmlXMLHttpRequestCtor : public FunctionObject
{
    V4_OBJECT2(QQmlXMLHttpRequestCtor, FunctionObject)
    static void markObjects(Heap::Base *that, ExecutionEngine *e) {
        QQmlXMLHttpRequestCtor::Data *c = static_cast<QQmlXMLHttpRequestCtor::Data *>(that);
        if (c->proto)
            c->proto->mark(e);
        FunctionObject::markObjects(that, e);
    }
    static void construct(const Managed *that, Scope &scope, QV4::CallData *)
    {
        Scoped<QQmlXMLHttpRequestCtor> ctor(scope, that->as<QQmlXMLHttpRequestCtor>());
        if (!ctor) {
            scope.result = scope.engine->throwTypeError();
            return;
        }

        QQmlXMLHttpRequest *r = new QQmlXMLHttpRequest(scope.engine->v8Engine->networkAccessManager());
        Scoped<QQmlXMLHttpRequestWrapper> w(scope, scope.engine->memoryManager->allocObject<QQmlXMLHttpRequestWrapper>(r));
        ScopedObject proto(scope, ctor->d()->proto);
        w->setPrototype(proto);
        scope.result = w.asReturnedValue();
    }

    static void call(const Managed *, Scope &scope, QV4::CallData *) {
        scope.result = Primitive::undefinedValue();
    }

    void setupProto();

    static void method_open(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_setRequestHeader(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_send(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_abort(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_getResponseHeader(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_getAllResponseHeaders(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);

    static void method_get_readyState(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_status(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_statusText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_responseText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_responseXML(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_response(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_get_responseType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
    static void method_set_responseType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
};

}

DEFINE_OBJECT_VTABLE(QQmlXMLHttpRequestWrapper);

void Heap::QQmlXMLHttpRequestCtor::init(ExecutionEngine *engine)
{
    Heap::FunctionObject::init(engine->rootContext(), QStringLiteral("XMLHttpRequest"));
    Scope scope(engine);
    Scoped<QV4::QQmlXMLHttpRequestCtor> ctor(scope, this);

    ctor->defineReadonlyProperty(QStringLiteral("UNSENT"), Primitive::fromInt32(0));
    ctor->defineReadonlyProperty(QStringLiteral("OPENED"), Primitive::fromInt32(1));
    ctor->defineReadonlyProperty(QStringLiteral("HEADERS_RECEIVED"), Primitive::fromInt32(2));
    ctor->defineReadonlyProperty(QStringLiteral("LOADING"), Primitive::fromInt32(3));
    ctor->defineReadonlyProperty(QStringLiteral("DONE"), Primitive::fromInt32(4));
    if (!ctor->d()->proto)
        ctor->setupProto();
    ScopedString s(scope, engine->id_prototype());
    ctor->defineDefaultProperty(s, ScopedObject(scope, ctor->d()->proto));
}

DEFINE_OBJECT_VTABLE(QQmlXMLHttpRequestCtor);

void QQmlXMLHttpRequestCtor::setupProto()
{
    ExecutionEngine *v4 = engine();
    Scope scope(v4);
    ScopedObject p(scope, v4->newObject());
    d()->proto = p->d();

    // Methods
    p->defineDefaultProperty(QStringLiteral("open"), method_open);
    p->defineDefaultProperty(QStringLiteral("setRequestHeader"), method_setRequestHeader);
    p->defineDefaultProperty(QStringLiteral("send"), method_send);
    p->defineDefaultProperty(QStringLiteral("abort"), method_abort);
    p->defineDefaultProperty(QStringLiteral("getResponseHeader"), method_getResponseHeader);
    p->defineDefaultProperty(QStringLiteral("getAllResponseHeaders"), method_getAllResponseHeaders);

    // Read-only properties
    p->defineAccessorProperty(QStringLiteral("readyState"), method_get_readyState, 0);
    p->defineAccessorProperty(QStringLiteral("status"),method_get_status, 0);
    p->defineAccessorProperty(QStringLiteral("statusText"),method_get_statusText, 0);
    p->defineAccessorProperty(QStringLiteral("responseText"),method_get_responseText, 0);
    p->defineAccessorProperty(QStringLiteral("responseXML"),method_get_responseXML, 0);
    p->defineAccessorProperty(QStringLiteral("response"),method_get_response, 0);

    // Read-write properties
    p->defineAccessorProperty(QStringLiteral("responseType"), method_get_responseType, method_set_responseType);

    // State values
    p->defineReadonlyProperty(QStringLiteral("UNSENT"), Primitive::fromInt32(0));
    p->defineReadonlyProperty(QStringLiteral("OPENED"), Primitive::fromInt32(1));
    p->defineReadonlyProperty(QStringLiteral("HEADERS_RECEIVED"), Primitive::fromInt32(2));
    p->defineReadonlyProperty(QStringLiteral("LOADING"), Primitive::fromInt32(3));
    p->defineReadonlyProperty(QStringLiteral("DONE"), Primitive::fromInt32(4));
}


// XMLHttpRequest methods
void QQmlXMLHttpRequestCtor::method_open(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (callData->argc < 2 || callData->argc > 5)
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    // Argument 0 - Method
    QString method = callData->args[0].toQStringNoThrow().toUpper();
    if (method != QLatin1String("GET") &&
        method != QLatin1String("PUT") &&
        method != QLatin1String("HEAD") &&
        method != QLatin1String("POST") &&
        method != QLatin1String("DELETE") &&
        method != QLatin1String("OPTIONS") &&
        method != QLatin1String("PROPFIND") &&
        method != QLatin1String("PATCH"))
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Unsupported HTTP method type");

    // Argument 1 - URL
    QUrl url = QUrl(callData->args[1].toQStringNoThrow());

    if (url.isRelative())
        url = scope.engine->callingQmlContext()->resolvedUrl(url);

    bool async = true;
    // Argument 2 - async (optional)
    if (callData->argc > 2) {
        async = callData->args[2].booleanValue();
    }

    // Argument 3/4 - user/pass (optional)
    QString username, password;
    if (callData->argc > 3)
        username = callData->args[3].toQStringNoThrow();
    if (callData->argc > 4)
        password = callData->args[4].toQStringNoThrow();

    // Clear the fragment (if any)
    url.setFragment(QString());

    // Set username/password
    if (!username.isNull()) url.setUserName(username);
    if (!password.isNull()) url.setPassword(password);

    scope.result = r->open(w, scope.engine->callingQmlContext(), method, url, async ? QQmlXMLHttpRequest::AsynchronousLoad : QQmlXMLHttpRequest::SynchronousLoad);
}

void QQmlXMLHttpRequestCtor::method_setRequestHeader(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (callData->argc != 2)
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Opened || r->sendFlag())
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    QString name = callData->args[0].toQStringNoThrow();
    QString value = callData->args[1].toQStringNoThrow();

    // ### Check that name and value are well formed

    QString nameUpper = name.toUpper();
    if (nameUpper == QLatin1String("ACCEPT-CHARSET") ||
        nameUpper == QLatin1String("ACCEPT-ENCODING") ||
        nameUpper == QLatin1String("CONNECTION") ||
        nameUpper == QLatin1String("CONTENT-LENGTH") ||
        nameUpper == QLatin1String("COOKIE") ||
        nameUpper == QLatin1String("COOKIE2") ||
        nameUpper == QLatin1String("CONTENT-TRANSFER-ENCODING") ||
        nameUpper == QLatin1String("DATE") ||
        nameUpper == QLatin1String("EXPECT") ||
        nameUpper == QLatin1String("HOST") ||
        nameUpper == QLatin1String("KEEP-ALIVE") ||
        nameUpper == QLatin1String("REFERER") ||
        nameUpper == QLatin1String("TE") ||
        nameUpper == QLatin1String("TRAILER") ||
        nameUpper == QLatin1String("TRANSFER-ENCODING") ||
        nameUpper == QLatin1String("UPGRADE") ||
        nameUpper == QLatin1String("USER-AGENT") ||
        nameUpper == QLatin1String("VIA") ||
        nameUpper.startsWith(QLatin1String("PROXY-")) ||
        nameUpper.startsWith(QLatin1String("SEC-")))
        RETURN_UNDEFINED();

    r->addHeader(name, value);

    RETURN_UNDEFINED();
}

void QQmlXMLHttpRequestCtor::method_send(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() != QQmlXMLHttpRequest::Opened ||
        r->sendFlag())
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    QByteArray data;
    if (callData->argc > 0)
        data = callData->args[0].toQStringNoThrow().toUtf8();

    scope.result = r->send(w, scope.engine->callingQmlContext(), data);
}

void QQmlXMLHttpRequestCtor::method_abort(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    scope.result = r->abort(w, scope.engine->callingQmlContext());
}

void QQmlXMLHttpRequestCtor::method_getResponseHeader(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (callData->argc != 1)
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done &&
        r->readyState() != QQmlXMLHttpRequest::HeadersReceived)
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    scope.result = scope.engine->newString(r->header(callData->args[0].toQStringNoThrow()));
}

void QQmlXMLHttpRequestCtor::method_getAllResponseHeaders(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (callData->argc != 0)
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done &&
        r->readyState() != QQmlXMLHttpRequest::HeadersReceived)
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    scope.result = scope.engine->newString(r->headers());
}

// XMLHttpRequest properties
void QQmlXMLHttpRequestCtor::method_get_readyState(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    scope.result = Encode(r->readyState());
}

void QQmlXMLHttpRequestCtor::method_get_status(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() == QQmlXMLHttpRequest::Unsent ||
        r->readyState() == QQmlXMLHttpRequest::Opened)
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    if (r->errorFlag())
        scope.result = Encode(0);
    else
        scope.result = Encode(r->replyStatus());
}

void QQmlXMLHttpRequestCtor::method_get_statusText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() == QQmlXMLHttpRequest::Unsent ||
        r->readyState() == QQmlXMLHttpRequest::Opened)
        THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    if (r->errorFlag())
        scope.result = scope.engine->newString(QString());
    else
        scope.result = scope.engine->newString(r->replyStatusText());
}

void QQmlXMLHttpRequestCtor::method_get_responseText(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done)
        scope.result = scope.engine->newString(QString());
    else
        scope.result = scope.engine->newString(r->responseBody());
}

void QQmlXMLHttpRequestCtor::method_get_responseXML(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (!r->receivedXml() ||
        (r->readyState() != QQmlXMLHttpRequest::Loading &&
         r->readyState() != QQmlXMLHttpRequest::Done)) {
        scope.result = Encode::null();
    } else {
        if (r->responseType().isEmpty())
            r->setResponseType(QLatin1String("document"));
        scope.result = r->xmlResponseBody(scope.engine);
    }
}

void QQmlXMLHttpRequestCtor::method_get_response(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
            r->readyState() != QQmlXMLHttpRequest::Done)
        RETURN_RESULT(scope.engine->newString(QString()));

    const QString& responseType = r->responseType();
    if (responseType.compare(QLatin1String("text"), Qt::CaseInsensitive) == 0 || responseType.isEmpty()) {
        RETURN_RESULT(scope.engine->newString(r->responseBody()));
    } else if (responseType.compare(QLatin1String("arraybuffer"), Qt::CaseInsensitive) == 0) {
        RETURN_RESULT(scope.engine->newArrayBuffer(r->rawResponseBody()));
    } else if (responseType.compare(QLatin1String("json"), Qt::CaseInsensitive) == 0) {
        RETURN_RESULT(r->jsonResponseBody(scope.engine));
    } else if (responseType.compare(QLatin1String("document"), Qt::CaseInsensitive) == 0) {
        RETURN_RESULT(r->xmlResponseBody(scope.engine));
    } else {
        RETURN_RESULT(scope.engine->newString(QString()));
    }
}


void QQmlXMLHttpRequestCtor::method_get_responseType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;
    scope.result = scope.engine->newString(r->responseType());
}

void QQmlXMLHttpRequestCtor::method_set_responseType(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData)
{
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (callData->argc < 1)
        THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    // Argument 0 - response type
    r->setResponseType(callData->args[0].toQStringNoThrow());

    scope.result = Encode::undefined();
}

void qt_rem_qmlxmlhttprequest(ExecutionEngine * /* engine */, void *d)
{
    QQmlXMLHttpRequestData *data = (QQmlXMLHttpRequestData *)d;
    delete data;
}

void *qt_add_qmlxmlhttprequest(ExecutionEngine *v4)
{
    Scope scope(v4);

    Scoped<QQmlXMLHttpRequestCtor> ctor(scope, v4->memoryManager->allocObject<QQmlXMLHttpRequestCtor>(v4));
    ScopedString s(scope, v4->newString(QStringLiteral("XMLHttpRequest")));
    v4->globalObject->defineReadonlyProperty(s, ctor);

    QQmlXMLHttpRequestData *data = new QQmlXMLHttpRequestData;
    return data;
}

QT_END_NAMESPACE

#endif // xmlstreamreader && qml_network

#include <qqmlxmlhttprequest.moc>
