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

#ifndef Patternist_PushBaseliner_h
#define Patternist_PushBaseliner_h

#include <QTextStream>
#include <QtXmlPatterns/QAbstractXmlReceiver>
#include <QtXmlPatterns/QXmlNamePool>
#include <QtXmlPatterns/QXmlNamePool>

class PushBaseliner : public QAbstractXmlReceiver
{
public:
    PushBaseliner(QTextStream &out,
                  const QXmlNamePool &namePool) : m_out(out)
                                                , m_namePool(namePool)
    {
    }

    bool isValid() const;
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

private:
    QTextStream &   m_out;
    const QXmlNamePool m_namePool;
};

bool PushBaseliner::isValid() const
{
    return m_out.codec();
}

void PushBaseliner::startElement(const QXmlName &name)
{
    m_out << "startElement(" << name.toClarkName(m_namePool) << ')'<< endl;
}

void PushBaseliner::endElement()
{
    m_out << "endElement()" << endl;
}

void PushBaseliner::attribute(const QXmlName &name, const QStringRef &value)
{
    m_out << "attribute(" << name.toClarkName(m_namePool) << ", " << value.toString() << ')'<< endl;
}

void PushBaseliner::comment(const QString &value)
{
    m_out << "comment(" << value << ')' << endl;
}

void PushBaseliner::characters(const QStringRef &value)
{
    m_out << "characters(" << value.toString() << ')' << endl;
}

void PushBaseliner::startDocument()
{
    m_out << "startDocument()" << endl;
}

void PushBaseliner::endDocument()
{
    m_out << "endDocument()" << endl;
}

void PushBaseliner::processingInstruction(const QXmlName &name, const QString &data)
{
    m_out << "processingInstruction(" << name.toClarkName(m_namePool) << ", " << data << ')' << endl;
}

void PushBaseliner::atomicValue(const QVariant &val)
{
    m_out << "atomicValue(" << val.toString() << ')' << endl;
}

void PushBaseliner::namespaceBinding(const QXmlName &name)
{
    m_out << "namespaceBinding(" << name.toClarkName(m_namePool) << ')' << endl;
}

void PushBaseliner::startOfSequence()
{
    m_out << "startOfSequence()" << endl;
}

void PushBaseliner::endOfSequence()
{
    m_out << "endOfSequence()" << endl;
}

#endif
