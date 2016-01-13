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

#include <private/qqmlglobal_p.h>

#include <QtCore/qvariant.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qdebug.h>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

QQmlValueTypeProvider::QQmlValueTypeProvider()
    : next(0)
{
}

QQmlValueTypeProvider::~QQmlValueTypeProvider()
{
    QQml_removeValueTypeProvider(this);
}

QQmlValueType *QQmlValueTypeProvider::createValueType(int type)
{
    QQmlValueType *value = 0;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->create(type, value))
            return value;
    } while ((p = p->next));

    return 0;
}

bool QQmlValueTypeProvider::initValueType(int type, void *data, size_t n)
{
    Q_ASSERT(data);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->init(type, data, n))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::destroyValueType(int type, void *data, size_t n)
{
    Q_ASSERT(data);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->destroy(type, data, n))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::copyValueType(int type, const void *src, void *dst, size_t n)
{
    Q_ASSERT(src);
    Q_ASSERT(dst);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->copy(type, src, dst, n))
            return true;
    } while ((p = p->next));

    return false;
}

QVariant QQmlValueTypeProvider::createValueType(int type, int argc, const void *argv[])
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->create(type, argc, argv, &v))
            return v;
    } while ((p = p->next));

    return QVariant();
}

bool QQmlValueTypeProvider::createValueFromString(int type, const QString &s, void *data, size_t n)
{
    Q_ASSERT(data);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->createFromString(type, s, data, n))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::createStringFromValue(int type, const void *data, QString *s)
{
    Q_ASSERT(data);
    Q_ASSERT(s);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->createStringFrom(type, data, s))
            return true;
    } while ((p = p->next));

    return false;
}

QVariant QQmlValueTypeProvider::createVariantFromString(const QString &s)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->variantFromString(s, &v))
            return v;
    } while ((p = p->next));

    // Return a variant containing the string itself
    return QVariant(s);
}

QVariant QQmlValueTypeProvider::createVariantFromString(int type, const QString &s, bool *ok)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->variantFromString(type, s, &v)) {
            if (ok) *ok = true;
            return v;
        }
    } while ((p = p->next));

    if (ok) *ok = false;
    return QVariant();
}

QVariant QQmlValueTypeProvider::createVariantFromJsObject(int type, QQmlV4Handle obj, QV8Engine *e, bool *ok)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->variantFromJsObject(type, obj, e, &v)) {
            if (ok) *ok = true;
            return v;
        }
    } while ((p = p->next));

    if (ok) *ok = false;
    return QVariant();
}

bool QQmlValueTypeProvider::equalValueType(int type, const void *lhs, const void *rhs, size_t rhsSize)
{
    Q_ASSERT(lhs);
    Q_ASSERT(rhs);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->equal(type, lhs, rhs, rhsSize))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::storeValueType(int type, const void *src, void *dst, size_t dstSize)
{
    Q_ASSERT(src);
    Q_ASSERT(dst);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->store(type, src, dst, dstSize))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::readValueType(int srcType, const void *src, size_t srcSize, int dstType, void *dst)
{
    Q_ASSERT(src);
    Q_ASSERT(dst);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->read(srcType, src, srcSize, dstType, dst))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::writeValueType(int type, const void *src, void *dst, size_t n)
{
    Q_ASSERT(src);
    Q_ASSERT(dst);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->write(type, src, dst, n))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::create(int, QQmlValueType *&) { return false; }
bool QQmlValueTypeProvider::init(int, void *, size_t) { return false; }
bool QQmlValueTypeProvider::destroy(int, void *, size_t) { return false; }
bool QQmlValueTypeProvider::copy(int, const void *, void *, size_t) { return false; }
bool QQmlValueTypeProvider::create(int, int, const void *[], QVariant *) { return false; }
bool QQmlValueTypeProvider::createFromString(int, const QString &, void *, size_t) { return false; }
bool QQmlValueTypeProvider::createStringFrom(int, const void *, QString *) { return false; }
bool QQmlValueTypeProvider::variantFromString(const QString &, QVariant *) { return false; }
bool QQmlValueTypeProvider::variantFromString(int, const QString &, QVariant *) { return false; }
bool QQmlValueTypeProvider::variantFromJsObject(int, QQmlV4Handle, QV8Engine *, QVariant *) { return false; }
bool QQmlValueTypeProvider::equal(int, const void *, const void *, size_t) { return false; }
bool QQmlValueTypeProvider::store(int, const void *, void *, size_t) { return false; }
bool QQmlValueTypeProvider::read(int, const void *, size_t, int, void *) { return false; }
bool QQmlValueTypeProvider::write(int, const void *, void *, size_t) { return false; }

Q_GLOBAL_STATIC(QQmlValueTypeProvider, nullValueTypeProvider)
static QQmlValueTypeProvider *valueTypeProvider = 0;

static QQmlValueTypeProvider **getValueTypeProvider(void)
{
    if (valueTypeProvider == 0) {
        valueTypeProvider = nullValueTypeProvider;
    }

    return &valueTypeProvider;
}

Q_QML_PRIVATE_EXPORT void QQml_addValueTypeProvider(QQmlValueTypeProvider *newProvider)
{
    static QQmlValueTypeProvider **providerPtr = getValueTypeProvider();
    newProvider->next = *providerPtr;
    *providerPtr = newProvider;
}

Q_QML_PRIVATE_EXPORT void QQml_removeValueTypeProvider(QQmlValueTypeProvider *oldProvider)
{
    if (oldProvider == nullValueTypeProvider) {
        // don't remove the null provider
        // we get here when the QtQml library is being unloaded
        return;
    }

    // the only entry with next = 0 is the null provider
    Q_ASSERT(oldProvider->next);

    QQmlValueTypeProvider *prev = valueTypeProvider;
    if (prev == oldProvider) {
        valueTypeProvider = oldProvider->next;
        return;
    }

    // singly-linked list removal
    for ( ; prev; prev = prev->next) {
        if (prev->next != oldProvider)
            continue;               // this is not the provider you're looking for
        prev->next = oldProvider->next;
        return;
    }

    qWarning("QQml_removeValueTypeProvider: was asked to remove provider %p but it was not found", oldProvider);
}

Q_AUTOTEST_EXPORT QQmlValueTypeProvider *QQml_valueTypeProvider(void)
{
    static QQmlValueTypeProvider **providerPtr = getValueTypeProvider();
    return *providerPtr;
}

QQmlColorProvider::~QQmlColorProvider() {}
QVariant QQmlColorProvider::colorFromString(const QString &, bool *ok) { if (ok) *ok = false; return QVariant(); }
unsigned QQmlColorProvider::rgbaFromString(const QString &, bool *ok) { if (ok) *ok = false; return 0; }
QVariant QQmlColorProvider::fromRgbF(double, double, double, double) { return QVariant(); }
QVariant QQmlColorProvider::fromHslF(double, double, double, double) { return QVariant(); }
QVariant QQmlColorProvider::lighter(const QVariant &, qreal) { return QVariant(); }
QVariant QQmlColorProvider::darker(const QVariant &, qreal) { return QVariant(); }
QVariant QQmlColorProvider::tint(const QVariant &, const QVariant &) { return QVariant(); }

static QQmlColorProvider *colorProvider = 0;

Q_QML_PRIVATE_EXPORT QQmlColorProvider *QQml_setColorProvider(QQmlColorProvider *newProvider)
{
    QQmlColorProvider *old = colorProvider;
    colorProvider = newProvider;
    return old;
}

static QQmlColorProvider **getColorProvider(void)
{
    if (colorProvider == 0) {
        qWarning() << "Warning: QQml_colorProvider: no color provider has been set!";
        static QQmlColorProvider nullColorProvider;
        colorProvider = &nullColorProvider;
    }

    return &colorProvider;
}

Q_AUTOTEST_EXPORT QQmlColorProvider *QQml_colorProvider(void)
{
    static QQmlColorProvider **providerPtr = getColorProvider();
    return *providerPtr;
}


QQmlGuiProvider::~QQmlGuiProvider() {}
QObject *QQmlGuiProvider::application(QObject *) { return new QQmlApplication(); }
QStringList QQmlGuiProvider::fontFamilies() { return QStringList(); }
bool QQmlGuiProvider::openUrlExternally(QUrl &) { return false; }

#ifndef QT_NO_IM
QObject *QQmlGuiProvider::inputMethod()
{
    // We don't have any input method code by default
    QObject *o = new QObject();
    o->setObjectName(QString::fromLatin1("No inputMethod available"));
    return o;
}
#endif

static QQmlGuiProvider *guiProvider = 0;

Q_QML_PRIVATE_EXPORT QQmlGuiProvider *QQml_setGuiProvider(QQmlGuiProvider *newProvider)
{
    QQmlGuiProvider *old = guiProvider;
    guiProvider = newProvider;
    return old;
}

static QQmlGuiProvider **getGuiProvider(void)
{
    if (guiProvider == 0) {
        static QQmlGuiProvider nullGuiProvider; //Still provides an application with no GUI support
        guiProvider = &nullGuiProvider;
    }

    return &guiProvider;
}

Q_AUTOTEST_EXPORT QQmlGuiProvider *QQml_guiProvider(void)
{
    static QQmlGuiProvider **providerPtr = getGuiProvider();
    return *providerPtr;
}

//Docs in qqmlengine.cpp
QQmlApplication::QQmlApplication(QObject *parent)
    : QObject(*(new QQmlApplicationPrivate),parent)
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
            this, SIGNAL(aboutToQuit()));
    connect(QCoreApplication::instance(), SIGNAL(applicationNameChanged()),
            this, SIGNAL(nameChanged()));
    connect(QCoreApplication::instance(), SIGNAL(applicationVersionChanged()),
            this, SIGNAL(versionChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationNameChanged()),
            this, SIGNAL(organizationChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationDomainChanged()),
            this, SIGNAL(domainChanged()));
}

QQmlApplication::QQmlApplication(QQmlApplicationPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
            this, SIGNAL(aboutToQuit()));
    connect(QCoreApplication::instance(), SIGNAL(applicationNameChanged()),
            this, SIGNAL(nameChanged()));
    connect(QCoreApplication::instance(), SIGNAL(applicationVersionChanged()),
            this, SIGNAL(versionChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationNameChanged()),
            this, SIGNAL(organizationChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationDomainChanged()),
            this, SIGNAL(domainChanged()));
}

QStringList QQmlApplication::args()
{
    Q_D(QQmlApplication);
    if (!d->argsInit) {
        d->argsInit = true;
        d->args = QCoreApplication::arguments();
    }
    return d->args;
}

QString QQmlApplication::name() const
{
    return QCoreApplication::instance()->applicationName();
}

QString QQmlApplication::version() const
{
    return QCoreApplication::instance()->applicationVersion();
}

QString QQmlApplication::organization() const
{
    return QCoreApplication::instance()->organizationName();
}

QString QQmlApplication::domain() const
{
    return QCoreApplication::instance()->organizationDomain();
}

void QQmlApplication::setName(const QString &arg)
{
    QCoreApplication::instance()->setApplicationName(arg);
}

void QQmlApplication::setVersion(const QString &arg)
{
    QCoreApplication::instance()->setApplicationVersion(arg);
}

void QQmlApplication::setOrganization(const QString &arg)
{
    QCoreApplication::instance()->setOrganizationName(arg);
}

void QQmlApplication::setDomain(const QString &arg)
{
    QCoreApplication::instance()->setOrganizationDomain(arg);
}

QT_END_NAMESPACE
