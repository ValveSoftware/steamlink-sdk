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

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QtDebug>

#include "XQTSTestCase.h"

using namespace QPatternistSDK;
using namespace QPatternist;

XQTSTestCase::XQTSTestCase(const Scenario scen,
                           TreeItem *p,
                           const QXmlQuery::QueryLanguage lang) : m_isXPath(false)
                                                                , m_scenario(scen)
                                                                , m_parent(p)
                                                                , m_lang(lang)
{
}

XQTSTestCase::~XQTSTestCase()
{
    qDeleteAll(m_baseLines);
}

QVariant XQTSTestCase::data(const Qt::ItemDataRole role, int column) const
{
    if(role == Qt::DisplayRole)
    {
        if(column == 0)
            return title();

        const TestResult *const tr = testResult();
        if(!tr)
        {
            if(column == 1)
                return TestResult::displayName(TestResult::NotTested);
            else
                return QString();
        }
        const TestResult::Status status = tr->status();

        switch(column)
        {
            case 1:
                return status == TestResult::Pass ? QString(QChar::fromLatin1('1'))
                                                  : QString(QChar::fromLatin1('0'));
            case 2:
                return status == TestResult::Fail ? QString(QChar::fromLatin1('1'))
                                                  : QString(QChar::fromLatin1('0'));
            default:
                return QString();
        }
    }

    if(role != Qt::BackgroundRole)
        return QVariant();

    const TestResult *const tr = testResult();

    if(!tr)
    {
        if(column == 0)
            return QColor(Qt::yellow);
        else
            return QVariant();
    }

    const TestResult::Status status = tr->status();

    if(status == TestResult::NotTested || status == TestResult::Unknown)
        return QColor(Qt::yellow);

    switch(column)
    {
        case 1:
            return status == TestResult::Pass ? QColor(Qt::green) : QVariant();
        case 2:
            return status == TestResult::Fail ? QColor(Qt::red) : QVariant();
        default:
            return QVariant();
    }
}

QString XQTSTestCase::sourceCode(bool &ok) const
{
    QFile file(m_queryPath.toLocalFile());

    QString err;

    if(!file.exists())
        err = QString::fromLatin1("Error: %1 does not exist.").arg(file.fileName());
    else if(!QFileInfo(file.fileName()).isFile())
        err = QString::fromLatin1("Error: %1 is not a file, cannot display it.").arg(file.fileName());
    else if(!file.open(QIODevice::ReadOnly))
        err = QString::fromLatin1("Error: Could not open %1. Likely a permission error.")
                                  .arg(file.fileName());

    if(err.isNull()) /* No errors. */
    {
        ok = true;
        /* Scary, we assume the query is stored in UTF-8. */
        return QString::fromUtf8(file.readAll());
    }
    else
    {
        ok = false;
        return err;
    }
}

int XQTSTestCase::columnCount() const
{
    return 2;
}

void XQTSTestCase::addBaseLine(TestBaseLine *line)
{
    m_baseLines.append(line);
}

QString XQTSTestCase::name() const
{
    return m_name;
}

QString XQTSTestCase::creator() const
{
    return m_creator;
}

QString XQTSTestCase::description() const
{
    return m_description;
}

QDate XQTSTestCase::lastModified() const
{
    return m_lastModified;
}

bool XQTSTestCase::isXPath() const
{
    return m_isXPath;
}

TestCase::Scenario XQTSTestCase::scenario() const
{
    return m_scenario;
}

void XQTSTestCase::setName(const QString &n)
{
    m_name = n;
}

void XQTSTestCase::setCreator(const QString &ctor)
{
    m_creator = ctor;
}

void XQTSTestCase::setDescription(const QString &descriptionP)
{
    m_description = descriptionP;
}

void XQTSTestCase::setLastModified(const QDate &date)
{
    m_lastModified = date;
}

void XQTSTestCase::setIsXPath(const bool isXPathP)
{
    m_isXPath = isXPathP;
}

void XQTSTestCase::setQueryPath(const QUrl &uri)
{
    m_queryPath = uri;
}

TreeItem *XQTSTestCase::parent() const
{
    return m_parent;
}

QString XQTSTestCase::title() const
{
    return m_name;
}

TestBaseLine::List XQTSTestCase::baseLines() const
{
    Q_ASSERT_X(!m_baseLines.isEmpty(), Q_FUNC_INFO,
               qPrintable(QString::fromLatin1("The test %1 has no base lines, it should have at least one.").arg(name())));
    return m_baseLines;
}

QUrl XQTSTestCase::testCasePath() const
{
    return m_queryPath;
}

void XQTSTestCase::setExternalVariableLoader(const QPatternist::ExternalVariableLoader::Ptr &loader)
{
    m_externalVariableLoader = loader;
}

QPatternist::ExternalVariableLoader::Ptr XQTSTestCase::externalVariableLoader() const
{
    return m_externalVariableLoader;
}

void XQTSTestCase::setContextItemSource(const QUrl &uri)
{
    m_contextItemSource = uri;
}

QUrl XQTSTestCase::contextItemSource() const
{
    return m_contextItemSource;
}

QXmlQuery::QueryLanguage XQTSTestCase::language() const
{
    return m_lang;
}

void XQTSTestCase::setParent(TreeItem *const p)
{
    m_parent = p;
}

void XQTSTestCase::setInitialTemplateName(const QXmlName &name)
{
    m_initialTemplateName = name;
}

QXmlName XQTSTestCase::initialTemplateName() const
{
    return m_initialTemplateName;
}

// vim: et:ts=4:sw=4:sts=4

