/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qdocumentprojector_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

DocumentProjector::DocumentProjector(const ProjectedExpression::Vector &paths,
                                     QAbstractXmlReceiver *const receiver) : m_paths(paths)
                                                                           , m_pathCount(paths.count())
                                                                           , m_action(ProjectedExpression::Move)
                                                                           , m_nodesInProcess(0)
                                                                           , m_receiver(receiver)
{
    Q_ASSERT_X(paths.count() > 0, Q_FUNC_INFO,
               "Using DocumentProjector with no paths is an "
               "overhead and has also undefined behavior.");
    Q_ASSERT(m_receiver);
}

void DocumentProjector::startElement(const QXmlName &name)
{
    Q_UNUSED(name);

    switch(m_action)
    {
        case ProjectedExpression::KeepSubtree:
        {
            m_receiver->startElement(name);
            /* Fallthrough. */
        }
        case ProjectedExpression::Skip:
        {
            ++m_nodesInProcess;
            return;
        }
        default:
        {
            Q_ASSERT_X(m_action == ProjectedExpression::Move, Q_FUNC_INFO,
                       "We're not supposed to receive Keep here, because "
                       "endElement() should always end that state.");

            for(int i = 0; i < m_pathCount; ++i)
            {
                m_action = m_paths.at(i)->actionForElement(name, m_paths[i]);

                switch(m_action)
                {
                    case ProjectedExpression::Keep:
                    {
                        m_action = ProjectedExpression::Keep;
                        continue;
                    }
                    case ProjectedExpression::KeepSubtree:
                    {
                        /* Ok, at least one path wanted this node. Pass it on,
                         * and exit. */
                        m_receiver->startElement(name);
                        ++m_nodesInProcess;
                        return;
                    }
                    case ProjectedExpression::Skip:
                    {
                        /* This particular path doesn't need it, but
                         * some other path might, so continue looping. */
                        continue;
                    }
                    case ProjectedExpression::Move:
                        Q_ASSERT_X(false, Q_FUNC_INFO, "The action functions can never return Move.");
                }
            }

            ++m_nodesInProcess;

            if(m_action == ProjectedExpression::Keep)
                m_receiver->startElement(name);
            else
            {
                Q_ASSERT(m_action == ProjectedExpression::Skip);
            }
        }
    }
}

void DocumentProjector::endElement()
{
    if(m_action == ProjectedExpression::Keep)
    {
        Q_ASSERT(m_nodesInProcess == 1);

        m_receiver->endElement();

        /* We have now kept the single node, and now wants to skip
         * all its children. */
        m_action = ProjectedExpression::Skip;
        m_nodesInProcess = 0;
    }
    else if(m_action == ProjectedExpression::KeepSubtree)
    {
        m_receiver->endElement();
        --m_nodesInProcess;

        if(m_nodesInProcess == 0)
        {
            /* We have now skipped all the children, let's do
             * a new path analysis. */
            m_action = ProjectedExpression::Move;
        }
    }
    else
    {
        Q_ASSERT_X(m_action == ProjectedExpression::Skip, Q_FUNC_INFO,
                   "We're not supposed to be in a Move action here.");
        /* We skip calling m_receiver's endElement() here since we're
         * skipping. */
        Q_ASSERT(m_nodesInProcess > 0);
        --m_nodesInProcess;

        if(m_nodesInProcess == 0)
        {
            /* Ok, we've skipped them all, let's do something
             * new -- let's Move on to the next path! */
            m_action = ProjectedExpression::Move;
        }
    }
}

void DocumentProjector::attribute(const QXmlName &name,
                                  const QStringRef &value)
{
    Q_UNUSED(name);
    Q_UNUSED(value);
}

void DocumentProjector::namespaceBinding(const QXmlName &nb)
{
    Q_UNUSED(nb);
}

void DocumentProjector::comment(const QString &value)
{
    Q_ASSERT_X(!value.contains(QLatin1String("--")), Q_FUNC_INFO,
               "Invalid input; it's the caller's responsibility to ensure the input is correct.");
    Q_UNUSED(value);
}

void DocumentProjector::characters(const QStringRef &value)
{
    Q_UNUSED(value);
}

void DocumentProjector::processingInstruction(const QXmlName &name,
                                              const QString &value)
{
    Q_ASSERT_X(!value.contains(QLatin1String("?>")), Q_FUNC_INFO,
               "Invalid input; it's the caller's responsibility to ensure the input is correct.");
    Q_UNUSED(name);
    Q_UNUSED(value);
}

void DocumentProjector::item(const Item &outputItem)
{
    Q_UNUSED(outputItem);
}

void DocumentProjector::startDocument()
{
}

void DocumentProjector::endDocument()
{
}

QT_END_NAMESPACE
