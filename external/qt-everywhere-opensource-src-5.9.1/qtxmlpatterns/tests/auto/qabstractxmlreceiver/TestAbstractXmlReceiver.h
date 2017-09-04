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


//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef Patternist_TestAbstractXmlReceiver_h
#define Patternist_TestAbstractXmlReceiver_h

#include <QtXmlPatterns/QAbstractXmlReceiver>

class TestAbstractXmlReceiver : public QAbstractXmlReceiver
{
public:
    using QAbstractXmlReceiver::attribute;
    using QAbstractXmlReceiver::characters;

    virtual void startElement(const QXmlName&);
    virtual void endElement();
    virtual void attribute(const QXmlName&, const QStringRef&);
    virtual void comment(const QString&);
    virtual void characters(const QStringRef&);
    virtual void startDocument();
    virtual void endDocument();
    virtual void processingInstruction(const QXmlName&, const QString&);
    virtual void atomicValue(const QVariant&);
    virtual void namespaceBinding(const QXmlName&);
    virtual void startOfSequence();
    virtual void endOfSequence();

    QString receivedFromCharacters;
    QString receivedFromAttribute;
};

void TestAbstractXmlReceiver::startElement(const QXmlName &name)
{
    Q_UNUSED(name);
}

void TestAbstractXmlReceiver::endElement()
{
}

void TestAbstractXmlReceiver::attribute(const QXmlName &name, const QStringRef &value)
{
    Q_UNUSED(name);
    receivedFromAttribute = value.toString();
}

void TestAbstractXmlReceiver::comment(const QString &value)
{
    Q_UNUSED(value);
}

void TestAbstractXmlReceiver::characters(const QStringRef &value)
{
    receivedFromCharacters = value.toString();
}

void TestAbstractXmlReceiver::startDocument()
{
}

void TestAbstractXmlReceiver::endDocument()
{
}

void TestAbstractXmlReceiver::processingInstruction(const QXmlName &name, const QString &data)
{
    Q_UNUSED(name);
    Q_UNUSED(data);
}

void TestAbstractXmlReceiver::atomicValue(const QVariant &val)
{
    Q_UNUSED(val);
}

void TestAbstractXmlReceiver::namespaceBinding(const QXmlName &name)
{
    Q_UNUSED(name);
}

void TestAbstractXmlReceiver::startOfSequence()
{
}

void TestAbstractXmlReceiver::endOfSequence()
{
}

#endif
