/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
#include <private/qqmlcontextwrapper_p.h>
#include <private/qv4scopedvalue_p.h>

#include <QtCore/qobject.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsengine.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qxmlstream.h>
#include <QtCore/qstack.h>
#include <QtCore/qdebug.h>

#include <private/qv4objectproto_p.h>
#include <private/qv4scopedvalue_p.h>

using namespace QV4;

#ifndef QT_NO_XMLSTREAMREADER

#define V4THROW_REFERENCE(string) { \
        Scoped<Object> error(scope, ctx->engine()->newReferenceErrorObject(QStringLiteral(string))); \
        return ctx->throwError(error); \
    }

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

static inline QQmlXMLHttpRequestData *xhrdata(QV8Engine *engine)
{
    return (QQmlXMLHttpRequestData *)engine->xmlHttpRequestData();
}

static ReturnedValue constructMeObject(const ValueRef thisObj, QV8Engine *e)
{
    ExecutionEngine *v4 = QV8Engine::getV4(e);
    Scope scope(v4);
    Scoped<Object> meObj(scope, v4->newObject());
    meObj->put(ScopedString(scope, v4->newString(QStringLiteral("ThisObject"))).getPointer(), thisObj);
    ScopedValue v(scope, QmlContextWrapper::qmlScope(e, e->callingContext(), 0));
    meObj->put(ScopedString(scope, v4->newString(QStringLiteral("ActivationObject"))).getPointer(), v);
    return meObj.asReturnedValue();
}

QQmlXMLHttpRequestData::QQmlXMLHttpRequestData()
{
}

QQmlXMLHttpRequestData::~QQmlXMLHttpRequestData()
{
}

namespace {

class DocumentImpl;
class NodeImpl
{
public:
    NodeImpl() : type(Element), document(0), parent(0) {}
    virtual ~NodeImpl() {
        for (int ii = 0; ii < children.count(); ++ii)
            delete children.at(ii);
        for (int ii = 0; ii < attributes.count(); ++ii)
            delete attributes.at(ii);
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
        if (root) delete root;
    }

    QString version;
    QString encoding;
    bool isStandalone;

    NodeImpl *root;

    void addref() { QQmlRefCount::addref(); }
    void release() { QQmlRefCount::release(); }
};

class NamedNodeMap : public Object
{
public:
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, NodeImpl *data, const QList<NodeImpl *> &list)
            : Object::Data(engine)
            , list(list)
            , d(data)
        {
            setVTable(staticVTable());

            if (d)
                d->addref();
        }
        ~Data() {
            if (d)
                d->release();
        }
        QList<NodeImpl *> list; // Only used in NamedNodeMap
        NodeImpl *d;
    };
    V4_OBJECT(Object)

    // C++ API
    static ReturnedValue create(QV8Engine *, NodeImpl *, const QList<NodeImpl *> &);

    // JS API
    static void destroy(Managed *that) {
        static_cast<NamedNodeMap *>(that)->d()->~Data();
    }
    static ReturnedValue get(Managed *m, String *name, bool *hasProperty);
    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
};

DEFINE_OBJECT_VTABLE(NamedNodeMap);

class NodeList : public Object
{
public:
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, NodeImpl *data)
            : Object::Data(engine)
            , d(data)
        {
            setVTable(staticVTable());

            if (d)
                d->addref();
        }
        ~Data() {
            if (d)
                d->release();
        }
        NodeImpl *d;
    };
    V4_OBJECT(Object)

    // JS API
    static void destroy(Managed *that) {
        static_cast<NodeList *>(that)->d()->~Data();
    }
    static ReturnedValue get(Managed *m, String *name, bool *hasProperty);
    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);

    // C++ API
    static ReturnedValue create(QV8Engine *, NodeImpl *);

};

DEFINE_OBJECT_VTABLE(NodeList);

class NodePrototype : public Object
{
public:
    struct Data : Object::Data {
        Data(ExecutionEngine *engine)
            : Object::Data(engine)
        {
            setVTable(staticVTable());

            Scope scope(engine);
            ScopedObject o(scope, this);

            o->defineAccessorProperty(QStringLiteral("nodeName"), method_get_nodeName, 0);
            o->defineAccessorProperty(QStringLiteral("nodeValue"), method_get_nodeValue, 0);
            o->defineAccessorProperty(QStringLiteral("nodeType"), method_get_nodeType, 0);

            o->defineAccessorProperty(QStringLiteral("parentNode"), method_get_parentNode, 0);
            o->defineAccessorProperty(QStringLiteral("childNodes"), method_get_childNodes, 0);
            o->defineAccessorProperty(QStringLiteral("firstChild"), method_get_firstChild, 0);
            o->defineAccessorProperty(QStringLiteral("lastChild"), method_get_lastChild, 0);
            o->defineAccessorProperty(QStringLiteral("previousSibling"), method_get_previousSibling, 0);
            o->defineAccessorProperty(QStringLiteral("nextSibling"), method_get_nextSibling, 0);
            o->defineAccessorProperty(QStringLiteral("attributes"), method_get_attributes, 0);
        }
    };
    V4_OBJECT(Object)

    static void initClass(ExecutionEngine *engine);

    // JS API
    static ReturnedValue method_get_nodeName(CallContext *ctx);
    static ReturnedValue method_get_nodeValue(CallContext *ctx);
    static ReturnedValue method_get_nodeType(CallContext *ctx);

    static ReturnedValue method_get_parentNode(CallContext *ctx);
    static ReturnedValue method_get_childNodes(CallContext *ctx);
    static ReturnedValue method_get_firstChild(CallContext *ctx);
    static ReturnedValue method_get_lastChild(CallContext *ctx);
    static ReturnedValue method_get_previousSibling(CallContext *ctx);
    static ReturnedValue method_get_nextSibling(CallContext *ctx);
    static ReturnedValue method_get_attributes(CallContext *ctx);

    //static ReturnedValue ownerDocument(CallContext *ctx);
    //static ReturnedValue namespaceURI(CallContext *ctx);
    //static ReturnedValue prefix(CallContext *ctx);
    //static ReturnedValue localName(CallContext *ctx);
    //static ReturnedValue baseURI(CallContext *ctx);
    //static ReturnedValue textContent(CallContext *ctx);

    static ReturnedValue getProto(ExecutionEngine *v4);

};

DEFINE_OBJECT_VTABLE(NodePrototype);

struct Node : public Object
{
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, NodeImpl *data)
            : Object::Data(engine)
            , d(data)
        {
            setVTable(staticVTable());

            if (d)
                d->addref();
        }
        ~Data() {
            if (d)
                d->release();
        }
        NodeImpl *d;
    };
    V4_OBJECT(Object)


    // JS API
    static void destroy(Managed *that) {
        static_cast<Node *>(that)->d()->~Data();
    }

    // C++ API
    static ReturnedValue create(QV8Engine *, NodeImpl *);

    bool isNull() const;

private:
    Node &operator=(const Node &);
    Node(const Node &o);
};

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
    static ReturnedValue method_name(CallContext *ctx);
//    static ReturnedValue specified(CallContext *);
    static ReturnedValue method_value(CallContext *ctx);
    static ReturnedValue method_ownerElement(CallContext *ctx);
//    static ReturnedValue schemaTypeInfo(CallContext *);
//    static ReturnedValue isId(CallContext *c);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
};

class CharacterData : public Node
{
public:
    // JS API
    static ReturnedValue method_length(CallContext *ctx);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *v4);
};

class Text : public CharacterData
{
public:
    // JS API
    static ReturnedValue method_isElementContentWhitespace(CallContext *ctx);
    static ReturnedValue method_wholeText(CallContext *ctx);

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
    static ReturnedValue method_xmlVersion(CallContext *ctx);
    static ReturnedValue method_xmlEncoding(CallContext *ctx);
    static ReturnedValue method_xmlStandalone(CallContext *ctx);
    static ReturnedValue method_documentElement(CallContext *ctx);

    // C++ API
    static ReturnedValue prototype(ExecutionEngine *);
    static ReturnedValue load(QV8Engine *engine, const QByteArray &data);
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

ReturnedValue NodePrototype::method_get_nodeName(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

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
    return Encode(ctx->d()->engine->newString(name));
}

ReturnedValue NodePrototype::method_get_nodeValue(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    if (r->d()->d->type == NodeImpl::Document ||
        r->d()->d->type == NodeImpl::DocumentFragment ||
        r->d()->d->type == NodeImpl::DocumentType ||
        r->d()->d->type == NodeImpl::Element ||
        r->d()->d->type == NodeImpl::Entity ||
        r->d()->d->type == NodeImpl::EntityReference ||
        r->d()->d->type == NodeImpl::Notation)
        return Encode::null();

    return Encode(ctx->d()->engine->newString(r->d()->d->data));
}

ReturnedValue NodePrototype::method_get_nodeType(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    return Encode(r->d()->d->type);
}

ReturnedValue NodePrototype::method_get_parentNode(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->d()->d->parent)
        return Node::create(engine, r->d()->d->parent);
    else
        return Encode::null();
}

ReturnedValue NodePrototype::method_get_childNodes(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return NodeList::create(engine, r->d()->d);
}

ReturnedValue NodePrototype::method_get_firstChild(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->d()->d->children.isEmpty())
        return Encode::null();
    else
        return Node::create(engine, r->d()->d->children.first());
}

ReturnedValue NodePrototype::method_get_lastChild(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->d()->d->children.isEmpty())
        return Encode::null();
    else
        return Node::create(engine, r->d()->d->children.last());
}

ReturnedValue NodePrototype::method_get_previousSibling(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (!r->d()->d->parent)
        return Encode::null();

    for (int ii = 0; ii < r->d()->d->parent->children.count(); ++ii) {
        if (r->d()->d->parent->children.at(ii) == r->d()->d) {
            if (ii == 0)
                return Encode::null();
            else
                return Node::create(engine, r->d()->d->parent->children.at(ii - 1));
        }
    }

    return Encode::null();
}

ReturnedValue NodePrototype::method_get_nextSibling(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (!r->d()->d->parent)
        return Encode::null();

    for (int ii = 0; ii < r->d()->d->parent->children.count(); ++ii) {
        if (r->d()->d->parent->children.at(ii) == r->d()->d) {
            if ((ii + 1) == r->d()->d->parent->children.count())
                return Encode::null();
            else
                return Node::create(engine, r->d()->d->parent->children.at(ii + 1));
        }
    }

    return Encode::null();
}

ReturnedValue NodePrototype::method_get_attributes(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return ctx->throwTypeError();

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->d()->d->type != NodeImpl::Element)
        return Encode::null();
    else
        return NamedNodeMap::create(engine, r->d()->d, r->d()->d->attributes);
}

ReturnedValue NodePrototype::getProto(ExecutionEngine *v4)
{
    Scope scope(v4);
    QQmlXMLHttpRequestData *d = xhrdata(v4->v8Engine);
    if (d->nodePrototype.isUndefined()) {
        ScopedObject p(scope, v4->memoryManager->alloc<NodePrototype>(v4));
        d->nodePrototype = p;
        v4->v8Engine->freezeObject(p);
    }
    return d->nodePrototype.value();
}

ReturnedValue Node::create(QV8Engine *engine, NodeImpl *data)
{
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
    Scope scope(v4);

    Scoped<Node> instance(scope, v4->memoryManager->alloc<Node>(v4, data));
    ScopedObject p(scope);

    switch (data->type) {
    case NodeImpl::Attr:
        instance->setPrototype((p = Attr::prototype(v4)).getPointer());
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
        instance->setPrototype((p = CDATA::prototype(v4)).getPointer());
        break;
    case NodeImpl::Text:
        instance->setPrototype((p = Text::prototype(v4)).getPointer());
        break;
    case NodeImpl::Element:
        instance->setPrototype((p = Element::prototype(v4)).getPointer());
        break;
    }

    return instance.asReturnedValue();
}

ReturnedValue Element::prototype(ExecutionEngine *engine)
{
    QQmlXMLHttpRequestData *d = xhrdata(engine->v8Engine);
    if (d->elementPrototype.isUndefined()) {
        Scope scope(engine);
        ScopedObject p(scope, engine->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(engine)).getPointer());
        p->defineAccessorProperty(QStringLiteral("tagName"), NodePrototype::method_get_nodeName, 0);
        d->elementPrototype = p;
        engine->v8Engine->freezeObject(p);
    }
    return d->elementPrototype.value();
}

ReturnedValue Attr::prototype(ExecutionEngine *engine)
{
    QQmlXMLHttpRequestData *d = xhrdata(engine->v8Engine);
    if (d->attrPrototype.isUndefined()) {
        Scope scope(engine);
        Scoped<Object> p(scope, engine->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(engine)).getPointer());
        p->defineAccessorProperty(QStringLiteral("name"), method_name, 0);
        p->defineAccessorProperty(QStringLiteral("value"), method_value, 0);
        p->defineAccessorProperty(QStringLiteral("ownerElement"), method_ownerElement, 0);
        d->attrPrototype = p;
        engine->v8Engine->freezeObject(p);
    }
    return d->attrPrototype.value();
}

ReturnedValue Attr::method_name(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return engine->toString(r->d()->d->name);
}

ReturnedValue Attr::method_value(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return engine->toString(r->d()->d->data);
}

ReturnedValue Attr::method_ownerElement(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return Node::create(engine, r->d()->d->parent);
}

ReturnedValue CharacterData::method_length(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;
    Q_UNUSED(engine)
    return Encode(r->d()->d->data.length());
}

ReturnedValue CharacterData::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4->v8Engine);
    if (d->characterDataPrototype.isUndefined()) {
        Scope scope(v4);
        Scoped<Object> p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(v4)).getPointer());
        p->defineAccessorProperty(QStringLiteral("data"), NodePrototype::method_get_nodeValue, 0);
        p->defineAccessorProperty(QStringLiteral("length"), method_length, 0);
        d->characterDataPrototype = p;
        v4->v8Engine->freezeObject(p);
    }
    return d->characterDataPrototype.value();
}

ReturnedValue Text::method_isElementContentWhitespace(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r) return Encode::undefined();

    return Encode(r->d()->d->data.trimmed().isEmpty());
}

ReturnedValue Text::method_wholeText(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return engine->toString(r->d()->d->data);
}

ReturnedValue Text::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4->v8Engine);
    if (d->textPrototype.isUndefined()) {
        Scope scope(v4);
        Scoped<Object> p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = CharacterData::prototype(v4)).getPointer());
        p->defineAccessorProperty(QStringLiteral("isElementContentWhitespace"), method_isElementContentWhitespace, 0);
        p->defineAccessorProperty(QStringLiteral("wholeText"), method_wholeText, 0);
        d->textPrototype = p;
        v4->v8Engine->freezeObject(p);
    }
    return d->textPrototype.value();
}

ReturnedValue CDATA::prototype(ExecutionEngine *v4)
{
    // ### why not just use TextProto???
    QQmlXMLHttpRequestData *d = xhrdata(v4->v8Engine);
    if (d->cdataPrototype.isUndefined()) {
        Scope scope(v4);
        Scoped<Object> p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = Text::prototype(v4)).getPointer());
        d->cdataPrototype = p;
        v4->v8Engine->freezeObject(p);
    }
    return d->cdataPrototype.value();
}

ReturnedValue Document::prototype(ExecutionEngine *v4)
{
    QQmlXMLHttpRequestData *d = xhrdata(v4->v8Engine);
    if (d->documentPrototype.isUndefined()) {
        Scope scope(v4);
        Scoped<Object> p(scope, v4->newObject());
        ScopedObject pp(scope);
        p->setPrototype((pp = NodePrototype::getProto(v4)).getPointer());
        p->defineAccessorProperty(QStringLiteral("xmlVersion"), method_xmlVersion, 0);
        p->defineAccessorProperty(QStringLiteral("xmlEncoding"), method_xmlEncoding, 0);
        p->defineAccessorProperty(QStringLiteral("xmlStandalone"), method_xmlStandalone, 0);
        p->defineAccessorProperty(QStringLiteral("documentElement"), method_documentElement, 0);
        d->documentPrototype = p;
        v4->v8Engine->freezeObject(p);
    }
    return d->documentPrototype.value();
}

ReturnedValue Document::load(QV8Engine *engine, const QByteArray &data)
{
    Q_ASSERT(engine);
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
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

            foreach (const QXmlStreamAttribute &a, reader.attributes()) {
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

    ScopedObject instance(scope, v4->memoryManager->alloc<Node>(v4, document));
    ScopedObject p(scope);
    instance->setPrototype((p = Document::prototype(v4)).getPointer());
    return instance.asReturnedValue();
}

bool Node::isNull() const
{
    return d()->d == 0;
}

ReturnedValue NamedNodeMap::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    QV4::ExecutionEngine *v4 = m->engine();
    NamedNodeMap *r = m->as<NamedNodeMap>();
    if (!r) {
        if (hasProperty)
            *hasProperty = false;
        return v4->currentContext()->throwTypeError();
    }

    QV8Engine *engine = v4->v8Engine;

    if ((int)index < r->d()->list.count()) {
        if (hasProperty)
            *hasProperty = true;
        return Node::create(engine, r->d()->list.at(index));
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NamedNodeMap::get(Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<NamedNodeMap>());
    NamedNodeMap *r = static_cast<NamedNodeMap *>(m);
    QV4::ExecutionEngine *v4 = m->engine();

    name->makeIdentifier();
    if (name->equals(v4->id_length))
        return Primitive::fromInt32(r->d()->list.count()).asReturnedValue();

    QV8Engine *engine = v4->v8Engine;

    QString str = name->toQString();
    for (int ii = 0; ii < r->d()->list.count(); ++ii) {
        if (r->d()->list.at(ii)->name == str) {
            if (hasProperty)
                *hasProperty = true;
            return Node::create(engine, r->d()->list.at(ii));
        }
    }

    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NamedNodeMap::create(QV8Engine *engine, NodeImpl *data, const QList<NodeImpl *> &list)
{
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
    return (v4->memoryManager->alloc<NamedNodeMap>(v4, data, list))->asReturnedValue();
}

ReturnedValue NodeList::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    Q_ASSERT(m->as<NodeList>());
    QV4::ExecutionEngine *v4 = m->engine();
    NodeList *r = static_cast<NodeList *>(m);

    QV8Engine *engine = v4->v8Engine;

    if ((int)index < r->d()->d->children.count()) {
        if (hasProperty)
            *hasProperty = true;
        return Node::create(engine, r->d()->d->children.at(index));
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue NodeList::get(Managed *m, String *name, bool *hasProperty)
{
    Q_ASSERT(m->as<NodeList>());
    QV4::ExecutionEngine *v4 = m->engine();
    NodeList *r = static_cast<NodeList *>(m);

    name->makeIdentifier();

    if (name->equals(v4->id_length))
        return Primitive::fromInt32(r->d()->d->children.count()).asReturnedValue();
    return Object::get(m, name, hasProperty);
}

ReturnedValue NodeList::create(QV8Engine *engine, NodeImpl *data)
{
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
    return (v4->memoryManager->alloc<NodeList>(v4, data))->asReturnedValue();
}

ReturnedValue Document::method_documentElement(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return Node::create(engine, static_cast<DocumentImpl *>(r->d()->d)->root);
}

ReturnedValue Document::method_xmlStandalone(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;
    Q_UNUSED(engine)
    return Encode(static_cast<DocumentImpl *>(r->d()->d)->isStandalone);
}

ReturnedValue Document::method_xmlVersion(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return engine->toString(static_cast<DocumentImpl *>(r->d()->d)->version);
}

ReturnedValue Document::method_xmlEncoding(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Node> r(scope, ctx->d()->callData->thisObject.as<Node>());
    if (!r || r->d()->d->type != NodeImpl::Document)
        return Encode::undefined();
    QV8Engine *engine = ctx->d()->engine->v8Engine;

    return engine->toString(static_cast<DocumentImpl *>(r->d()->d)->encoding);
}

class QQmlXMLHttpRequest : public QObject
{
    Q_OBJECT
public:
    enum State { Unsent = 0,
                 Opened = 1, HeadersReceived = 2,
                 Loading = 3, Done = 4 };

    QQmlXMLHttpRequest(QV8Engine *engine, QNetworkAccessManager *manager);
    virtual ~QQmlXMLHttpRequest();

    bool sendFlag() const;
    bool errorFlag() const;
    quint32 readyState() const;
    int replyStatus() const;
    QString replyStatusText() const;

    ReturnedValue open(const ValueRef me, const QString &, const QUrl &);
    ReturnedValue send(const ValueRef me, const QByteArray &);
    ReturnedValue abort(const ValueRef me);

    void addHeader(const QString &, const QString &);
    QString header(const QString &name);
    QString headers();


    QString responseBody();
    const QByteArray & rawResponseBody() const;
    bool receivedXml() const;
private slots:
    void readyRead();
    void error(QNetworkReply::NetworkError);
    void finished();

private:
    void requestFromUrl(const QUrl &url);

    ExecutionEngine *v4;
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
#ifndef QT_NO_TEXTCODEC
    QTextCodec* findTextCodec() const;
#endif
    void readEncoding();

    ReturnedValue getMe() const;
    void setMe(const ValueRef me);
    PersistentValue m_me;

    void dispatchCallbackImpl(const ValueRef me);
    void dispatchCallback(const ValueRef me);

    int m_status;
    QString m_statusText;
    QNetworkRequest m_request;
    QStringList m_addedHeaders;
    QPointer<QNetworkReply> m_network;
    void destroyNetwork();

    QNetworkAccessManager *m_nam;
    QNetworkAccessManager *networkAccessManager() { return m_nam; }
};

QQmlXMLHttpRequest::QQmlXMLHttpRequest(QV8Engine *engine, QNetworkAccessManager *manager)
    : v4(QV8Engine::getV4(engine))
    , m_state(Unsent), m_errorFlag(false), m_sendFlag(false)
    , m_redirectCount(0), m_gotXml(false), m_textCodec(0), m_network(0), m_nam(manager)
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

ReturnedValue QQmlXMLHttpRequest::open(const ValueRef me, const QString &method, const QUrl &url)
{
    destroyNetwork();
    m_sendFlag = false;
    m_errorFlag = false;
    m_responseEntityBody = QByteArray();
    m_method = method;
    m_url = url;
    m_state = Opened;
    m_addedHeaders.clear();
    dispatchCallback(me);
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

QString QQmlXMLHttpRequest::header(const QString &name)
{
    QByteArray utfname = name.toLower().toUtf8();

    foreach (const HeaderPair &header, m_headersList) {
        if (header.first == utfname)
            return QString::fromUtf8(header.second);
    }
    return QString();
}

QString QQmlXMLHttpRequest::headers()
{
    QString ret;

    foreach (const HeaderPair &header, m_headersList) {
        if (ret.length())
            ret.append(QLatin1String("\r\n"));
        ret = ret % QString::fromUtf8(header.first) % QLatin1String(": ")
                % QString::fromUtf8(header.second);
    }
    return ret;
}

void QQmlXMLHttpRequest::fillHeadersList()
{
    QList<QByteArray> headerList = m_network->rawHeaderList();

    m_headersList.clear();
    foreach (const QByteArray &header, headerList) {
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

    if (m_method == QLatin1String("GET"))
        m_network = networkAccessManager()->get(request);
    else if (m_method == QLatin1String("HEAD"))
        m_network = networkAccessManager()->head(request);
    else if (m_method == QLatin1String("POST"))
        m_network = networkAccessManager()->post(request, m_data);
    else if (m_method == QLatin1String("PUT"))
        m_network = networkAccessManager()->put(request, m_data);
    else if (m_method == QLatin1String("DELETE"))
        m_network = networkAccessManager()->deleteResource(request);

    QObject::connect(m_network, SIGNAL(readyRead()),
                     this, SLOT(readyRead()));
    QObject::connect(m_network, SIGNAL(error(QNetworkReply::NetworkError)),
                     this, SLOT(error(QNetworkReply::NetworkError)));
    QObject::connect(m_network, SIGNAL(finished()),
                     this, SLOT(finished()));
}

ReturnedValue QQmlXMLHttpRequest::send(const ValueRef me, const QByteArray &data)
{
    m_errorFlag = false;
    m_sendFlag = true;
    m_redirectCount = 0;
    m_data = data;

    setMe(me);

    requestFromUrl(m_url);

    return Encode::undefined();
}

ReturnedValue QQmlXMLHttpRequest::abort(const ValueRef me)
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
        dispatchCallback(me);
    }

    m_state = Unsent;

    return Encode::undefined();
}

ReturnedValue QQmlXMLHttpRequest::getMe() const
{
    return m_me.value();
}

void QQmlXMLHttpRequest::setMe(const ValueRef me)
{
    m_me = me;
}

void QQmlXMLHttpRequest::readyRead()
{
    m_status =
        m_network->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_statusText =
        QString::fromUtf8(m_network->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray());

    Scope scope(v4);
    ScopedValue me(scope, m_me.value());

    // ### We assume if this is called the headers are now available
    if (m_state < HeadersReceived) {
        m_state = HeadersReceived;
        fillHeadersList ();
        dispatchCallback(me);
    }

    bool wasEmpty = m_responseEntityBody.isEmpty();
    m_responseEntityBody.append(m_network->readAll());
    if (wasEmpty && !m_responseEntityBody.isEmpty())
        m_state = Loading;

    dispatchCallback(me);
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

    Scope scope(v4);
    ScopedValue me(scope, m_me.value());

    if (error == QNetworkReply::ContentAccessDenied ||
        error == QNetworkReply::ContentOperationNotPermittedError ||
        error == QNetworkReply::ContentNotFoundError ||
        error == QNetworkReply::AuthenticationRequiredError ||
        error == QNetworkReply::ContentReSendError ||
        error == QNetworkReply::UnknownContentError ||
        error == QNetworkReply::ProtocolInvalidOperationError) {
        m_state = Loading;
        dispatchCallback(me);
    } else {
        m_errorFlag = true;
        m_responseEntityBody = QByteArray();
    }

    m_state = Done;

    dispatchCallback(me);
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
        dispatchCallback(m_me);
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
        dispatchCallback(m_me);
    }
    m_state = Done;

    dispatchCallback(m_me);

    Scope scope(v4);
    ScopedValue v(scope, Primitive::undefinedValue());
    setMe(v);
}


void QQmlXMLHttpRequest::readEncoding()
{
    foreach (const HeaderPair &header, m_headersList) {
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


#ifndef QT_NO_TEXTCODEC
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
#ifndef QT_NO_TEXTCODEC
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

void QQmlXMLHttpRequest::dispatchCallbackImpl(const ValueRef me)
{
    ExecutionContext *ctx = v4->currentContext();
    QV4::Scope scope(v4);
    Scoped<Object> o(scope, me);
    if (!o) {
        ctx->throwError(QStringLiteral("QQmlXMLHttpRequest: internal error: empty ThisObject"));
        return;
    }

    ScopedString s(scope, v4->newString(QStringLiteral("ThisObject")));
    Scoped<Object> thisObj(scope, o->get(s.getPointer()));
    if (!thisObj) {
        ctx->throwError(QStringLiteral("QQmlXMLHttpRequest: internal error: empty ThisObject"));
        return;
    }

    s = v4->newString(QStringLiteral("onreadystatechange"));
    Scoped<FunctionObject> callback(scope, thisObj->get(s.getPointer()));
    if (!callback) {
        // not an error, but no onreadystatechange function to call.
        return;
    }

    s = v4->newString(QStringLiteral("ActivationObject"));
    Scoped<Object> activationObject(scope, o->get(s.getPointer()));
    if (!activationObject) {
        v4->currentContext()->throwError(QStringLiteral("QQmlXMLHttpRequest: internal error: empty ActivationObject"));
        return;
    }

    QQmlContextData *callingContext = QmlContextWrapper::getContext(activationObject);
    if (callingContext) {
        QV4::ScopedCallData callData(scope, 0);
        callData->thisObject = activationObject.asReturnedValue();
        callback->call(callData);
    }

    // if the callingContext object is no longer valid, then it has been
    // deleted explicitly (e.g., by a Loader deleting the itemContext when
    // the source is changed).  We do nothing in this case, as the evaluation
    // cannot succeed.

}

void QQmlXMLHttpRequest::dispatchCallback(const ValueRef me)
{
    ExecutionContext *ctx = v4->currentContext();
    dispatchCallbackImpl(me);
    if (v4->hasException) {
        QQmlError error = QV4::ExecutionEngine::catchExceptionAsQmlError(ctx);
        QQmlEnginePrivate::warning(QQmlEnginePrivate::get(v4->v8Engine->engine()), error);
    }
}

void QQmlXMLHttpRequest::destroyNetwork()
{
    if (m_network) {
        m_network->disconnect();
        m_network->deleteLater();
        m_network = 0;
    }
}


struct QQmlXMLHttpRequestWrapper : public Object
{
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, QQmlXMLHttpRequest *request)
            : Object::Data(engine)
            , request(request)
        {
            setVTable(staticVTable());
        }
        ~Data() {
            delete request;
        }
        QQmlXMLHttpRequest *request;
    };
    V4_OBJECT(Object)

    static void destroy(Managed *that) {
        static_cast<QQmlXMLHttpRequestWrapper *>(that)->d()->~Data();
    }
};

DEFINE_OBJECT_VTABLE(QQmlXMLHttpRequestWrapper);

struct QQmlXMLHttpRequestCtor : public FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionEngine *engine)
            : FunctionObject::Data(engine->rootContext, QStringLiteral("XMLHttpRequest"))
        {
            setVTable(staticVTable());
            Scope scope(engine);
            Scoped<QQmlXMLHttpRequestCtor> ctor(scope, this);

            ctor->defineReadonlyProperty(QStringLiteral("UNSENT"), Primitive::fromInt32(0));
            ctor->defineReadonlyProperty(QStringLiteral("OPENED"), Primitive::fromInt32(1));
            ctor->defineReadonlyProperty(QStringLiteral("HEADERS_RECEIVED"), Primitive::fromInt32(2));
            ctor->defineReadonlyProperty(QStringLiteral("LOADING"), Primitive::fromInt32(3));
            ctor->defineReadonlyProperty(QStringLiteral("DONE"), Primitive::fromInt32(4));
            if (!ctor->d()->proto)
                ctor->setupProto();
            ScopedString s(scope, engine->id_prototype);
            ctor->defineDefaultProperty(s.getPointer(), ScopedObject(scope, ctor->d()->proto));
        }
        Object *proto;
    };
    V4_OBJECT(FunctionObject)
    static void markObjects(Managed *that, ExecutionEngine *e) {
        QQmlXMLHttpRequestCtor *c = static_cast<QQmlXMLHttpRequestCtor *>(that);
        if (c->d()->proto)
            c->d()->proto->mark(e);
        FunctionObject::markObjects(that, e);
    }
    static ReturnedValue construct(Managed *that, QV4::CallData *)
    {
        Scope scope(that->engine());
        Scoped<QQmlXMLHttpRequestCtor> ctor(scope, that->as<QQmlXMLHttpRequestCtor>());
        if (!ctor)
            return that->engine()->currentContext()->throwTypeError();

        QV8Engine *engine = that->engine()->v8Engine;
        QQmlXMLHttpRequest *r = new QQmlXMLHttpRequest(engine, engine->networkAccessManager());
        Scoped<QQmlXMLHttpRequestWrapper> w(scope, that->engine()->memoryManager->alloc<QQmlXMLHttpRequestWrapper>(that->engine(), r));
        w->setPrototype(ctor->d()->proto);
        return w.asReturnedValue();
    }

    static ReturnedValue call(Managed *, QV4::CallData *) {
        return Primitive::undefinedValue().asReturnedValue();
    }

    void setupProto();

    static ReturnedValue method_open(CallContext *ctx);
    static ReturnedValue method_setRequestHeader(CallContext *ctx);
    static ReturnedValue method_send(CallContext *ctx);
    static ReturnedValue method_abort(CallContext *ctx);
    static ReturnedValue method_getResponseHeader(CallContext *ctx);
    static ReturnedValue method_getAllResponseHeaders(CallContext *ctx);

    static ReturnedValue method_get_readyState(CallContext *ctx);
    static ReturnedValue method_get_status(CallContext *ctx);
    static ReturnedValue method_get_statusText(CallContext *ctx);
    static ReturnedValue method_get_responseText(CallContext *ctx);
    static ReturnedValue method_get_responseXML(CallContext *ctx);
};

DEFINE_OBJECT_VTABLE(QQmlXMLHttpRequestCtor);

void QQmlXMLHttpRequestCtor::setupProto()
{
    ExecutionEngine *v4 = engine();
    Scope scope(v4);
    Scoped<Object> p(scope, v4->newObject());
    d()->proto = p.getPointer();

    // Methods
    d()->proto->defineDefaultProperty(QStringLiteral("open"), method_open);
    d()->proto->defineDefaultProperty(QStringLiteral("setRequestHeader"), method_setRequestHeader);
    d()->proto->defineDefaultProperty(QStringLiteral("send"), method_send);
    d()->proto->defineDefaultProperty(QStringLiteral("abort"), method_abort);
    d()->proto->defineDefaultProperty(QStringLiteral("getResponseHeader"), method_getResponseHeader);
    d()->proto->defineDefaultProperty(QStringLiteral("getAllResponseHeaders"), method_getAllResponseHeaders);

    // Read-only properties
    d()->proto->defineAccessorProperty(QStringLiteral("readyState"), method_get_readyState, 0);
    d()->proto->defineAccessorProperty(QStringLiteral("status"),method_get_status, 0);
    d()->proto->defineAccessorProperty(QStringLiteral("statusText"),method_get_statusText, 0);
    d()->proto->defineAccessorProperty(QStringLiteral("responseText"),method_get_responseText, 0);
    d()->proto->defineAccessorProperty(QStringLiteral("responseXML"),method_get_responseXML, 0);

    // State values
    d()->proto->defineReadonlyProperty(QStringLiteral("UNSENT"), Primitive::fromInt32(0));
    d()->proto->defineReadonlyProperty(QStringLiteral("OPENED"), Primitive::fromInt32(1));
    d()->proto->defineReadonlyProperty(QStringLiteral("HEADERS_RECEIVED"), Primitive::fromInt32(2));
    d()->proto->defineReadonlyProperty(QStringLiteral("LOADING"), Primitive::fromInt32(3));
    d()->proto->defineReadonlyProperty(QStringLiteral("DONE"), Primitive::fromInt32(4));
}


// XMLHttpRequest methods
ReturnedValue QQmlXMLHttpRequestCtor::method_open(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (ctx->d()->callData->argc < 2 || ctx->d()->callData->argc > 5)
        V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    // Argument 0 - Method
    QString method = ctx->d()->callData->args[0].toQStringNoThrow().toUpper();
    if (method != QLatin1String("GET") &&
        method != QLatin1String("PUT") &&
        method != QLatin1String("HEAD") &&
        method != QLatin1String("POST") &&
        method != QLatin1String("DELETE"))
        V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Unsupported HTTP method type");

    // Argument 1 - URL
    QUrl url = QUrl(ctx->d()->callData->args[1].toQStringNoThrow());

    if (url.isRelative())
        url = engine->callingContext()->resolvedUrl(url);

    // Argument 2 - async (optional)
    if (ctx->d()->callData->argc > 2 && !ctx->d()->callData->args[2].booleanValue())
        V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "Synchronous XMLHttpRequest calls are not supported");

    // Argument 3/4 - user/pass (optional)
    QString username, password;
    if (ctx->d()->callData->argc > 3)
        username = ctx->d()->callData->args[3].toQStringNoThrow();
    if (ctx->d()->callData->argc > 4)
        password = ctx->d()->callData->args[4].toQStringNoThrow();

    // Clear the fragment (if any)
    url.setFragment(QString());

    // Set username/password
    if (!username.isNull()) url.setUserName(username);
    if (!password.isNull()) url.setPassword(password);

    ScopedValue meObject(scope, constructMeObject(ctx->d()->callData->thisObject, engine));
    return r->open(meObject, method, url);
}

ReturnedValue QQmlXMLHttpRequestCtor::method_setRequestHeader(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (ctx->d()->callData->argc != 2)
        V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Opened || r->sendFlag())
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    QString name = ctx->d()->callData->args[0].toQStringNoThrow();
    QString value = ctx->d()->callData->args[1].toQStringNoThrow();

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
        return Encode::undefined();

    r->addHeader(name, value);

    return Encode::undefined();
}

ReturnedValue QQmlXMLHttpRequestCtor::method_send(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->readyState() != QQmlXMLHttpRequest::Opened ||
        r->sendFlag())
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    QByteArray data;
    if (ctx->d()->callData->argc > 0)
        data = ctx->d()->callData->args[0].toQStringNoThrow().toUtf8();

    ScopedValue meObject(scope, constructMeObject(ctx->d()->callData->thisObject, engine));
    return r->send(meObject, data);
}

ReturnedValue QQmlXMLHttpRequestCtor::method_abort(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    ScopedValue meObject(scope, constructMeObject(ctx->d()->callData->thisObject, ctx->d()->engine->v8Engine));
    return r->abort(meObject);
}

ReturnedValue QQmlXMLHttpRequestCtor::method_getResponseHeader(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (ctx->d()->callData->argc != 1)
        V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done &&
        r->readyState() != QQmlXMLHttpRequest::HeadersReceived)
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    return engine->toString(r->header(ctx->d()->callData->args[0].toQStringNoThrow()));
}

ReturnedValue QQmlXMLHttpRequestCtor::method_getAllResponseHeaders(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (ctx->d()->callData->argc != 0)
        V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "Incorrect argument count");

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done &&
        r->readyState() != QQmlXMLHttpRequest::HeadersReceived)
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    return engine->toString(r->headers());
}

// XMLHttpRequest properties
ReturnedValue QQmlXMLHttpRequestCtor::method_get_readyState(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    return Encode(r->readyState());
}

ReturnedValue QQmlXMLHttpRequestCtor::method_get_status(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (r->readyState() == QQmlXMLHttpRequest::Unsent ||
        r->readyState() == QQmlXMLHttpRequest::Opened)
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    if (r->errorFlag())
        return Encode(0);
    else
        return Encode(r->replyStatus());
}

ReturnedValue QQmlXMLHttpRequestCtor::method_get_statusText(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->readyState() == QQmlXMLHttpRequest::Unsent ||
        r->readyState() == QQmlXMLHttpRequest::Opened)
        V4THROW_DOM(DOMEXCEPTION_INVALID_STATE_ERR, "Invalid state");

    if (r->errorFlag())
        return engine->toString(QString());
    else
        return engine->toString(r->replyStatusText());
}

ReturnedValue QQmlXMLHttpRequestCtor::method_get_responseText(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    QV8Engine *engine = ctx->d()->engine->v8Engine;

    if (r->readyState() != QQmlXMLHttpRequest::Loading &&
        r->readyState() != QQmlXMLHttpRequest::Done)
        return engine->toString(QString());
    else
        return engine->toString(r->responseBody());
}

ReturnedValue QQmlXMLHttpRequestCtor::method_get_responseXML(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<QQmlXMLHttpRequestWrapper> w(scope, ctx->d()->callData->thisObject.as<QQmlXMLHttpRequestWrapper>());
    if (!w)
        V4THROW_REFERENCE("Not an XMLHttpRequest object");
    QQmlXMLHttpRequest *r = w->d()->request;

    if (!r->receivedXml() ||
        (r->readyState() != QQmlXMLHttpRequest::Loading &&
         r->readyState() != QQmlXMLHttpRequest::Done)) {
        return Encode::null();
    } else {
        return Document::load(ctx->d()->engine->v8Engine, r->rawResponseBody());
    }
}

void qt_rem_qmlxmlhttprequest(QV8Engine * /* engine */, void *d)
{
    QQmlXMLHttpRequestData *data = (QQmlXMLHttpRequestData *)d;
    delete data;
}

void *qt_add_qmlxmlhttprequest(QV8Engine *engine)
{
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
    Scope scope(v4);

    Scoped<QQmlXMLHttpRequestCtor> ctor(scope, v4->memoryManager->alloc<QQmlXMLHttpRequestCtor>(v4));
    ScopedString s(scope, v4->newString(QStringLiteral("XMLHttpRequest")));
    v4->globalObject->defineReadonlyProperty(s.getPointer(), ctor);

    QQmlXMLHttpRequestData *data = new QQmlXMLHttpRequestData;
    return data;
}

QT_END_NAMESPACE

#endif // QT_NO_XMLSTREAMREADER

#include <qqmlxmlhttprequest.moc>
