/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <iterator>

#include "dependencyhandler_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

namespace {
    template <template <typename T> class Op>
    struct ByDepender {
        typedef bool result_type;

        bool operator()(const RunnableInterface *lhs, const RunnableInterface *rhs) const Q_DECL_NOTHROW
        { return Op<const RunnableInterface *>()(lhs, rhs); }

        bool operator()(const RunnableInterface *lhs, const Dependency &rhs) const Q_DECL_NOTHROW
        { return operator()(lhs, rhs.depender); }

        bool operator()(const Dependency &lhs, const RunnableInterface *rhs) const Q_DECL_NOTHROW
        { return operator()(lhs.depender, rhs); }

        bool operator()(const Dependency &lhs, const Dependency &rhs) const Q_DECL_NOTHROW
        { return operator()(lhs.depender, rhs.depender); }
    };

    struct DependeeEquals : std::unary_function<Dependency, bool>
    {
        const RunnableInterface *dependee;
        QVector<RunnableInterface *> *freedList;
        explicit DependeeEquals(const RunnableInterface *dependee, QVector<RunnableInterface *> *freedList)
            : dependee(qMove(dependee)), freedList(qMove(freedList)) {}
        bool operator()(const Dependency &candidate) const
        {
            if (dependee == candidate.dependee) {
                if (!candidate.depender->reserved())
                    freedList->append(candidate.depender);
                return true;
            }
            return false;
        }
    };

    struct DependerEquals : std::unary_function<Dependency, bool>
    {
        const RunnableInterface *depender;
        explicit DependerEquals(const RunnableInterface *depender)
            : depender(qMove(depender)) {}
        bool operator()(const Dependency &candidate) const
        {
            return depender == candidate.depender;
        }
    };

    struct ByDependerThenDependee : std::binary_function<Dependency, Dependency, bool>
    {
        // Defines a lexicographical order (depender first).
        bool operator()(const Dependency &lhs, const Dependency &rhs)
        {
            if (lhs.depender < rhs.depender) return true;
            if (rhs.depender < lhs.depender) return false;
            return lhs.dependee < rhs.dependee;
        }
    };
}

DependencyHandler::DependencyHandler()
{
}

void DependencyHandler::addDependencies(QVector<Dependency> dependencies)
{
    std::sort(dependencies.begin(), dependencies.end(), ByDependerThenDependee());

    const QMutexLocker locker(m_mutex);

    QVector<Dependency> newDependencyMap;
    newDependencyMap.reserve(dependencies.size() + m_dependencyMap.size());
    std::set_union(m_dependencyMap.begin(), m_dependencyMap.end(),
                   dependencies.begin(), dependencies.end(),
                   std::back_inserter(newDependencyMap), ByDependerThenDependee());
    m_dependencyMap.swap(newDependencyMap); // commit
}

bool DependencyHandler::hasDependency(const RunnableInterface *depender)
{
    // The caller has to set the mutex, which is QThreadPooler::enqueueTasks

    return std::binary_search(m_dependencyMap.begin(), m_dependencyMap.end(),
                              depender, ByDepender<std::less>());
}

/*
 * Removes all the entries on the m_dependencyMap that have given task as a dependee,
 * i.e. entries where the dependency is on the given task.
 */
QVector<RunnableInterface *> DependencyHandler::freeDependencies(const RunnableInterface *task)
{
    // The caller has to set the mutex, which is QThreadPooler::taskFinished

    m_dependencyMap.erase(std::remove_if(m_dependencyMap.begin(),
                                         m_dependencyMap.end(),
                                         DependerEquals(task)),
                          m_dependencyMap.end());

    QVector<RunnableInterface *> freedList;
    m_dependencyMap.erase(std::remove_if(m_dependencyMap.begin(),
                                         m_dependencyMap.end(),
                                         DependeeEquals(task, &freedList)),
                          m_dependencyMap.end());

    return freedList;
}

} // namespace Qt3DCore

QT_END_NAMESPACE
