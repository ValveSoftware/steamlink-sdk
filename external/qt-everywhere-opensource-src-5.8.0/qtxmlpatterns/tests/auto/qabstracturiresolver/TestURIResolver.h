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


#ifndef TestURIResolver_h
#define TestURIResolver_h

#include <QtXmlPatterns/QAbstractUriResolver>

class TestURIResolver : public QAbstractUriResolver
{
public:
    TestURIResolver(const QUrl &result = QUrl(QLatin1String("http://example.com/")));

    virtual QUrl resolve(const QUrl &relative, const QUrl &baseURI) const;

private:
    const QUrl m_result;
};

TestURIResolver::TestURIResolver(const QUrl &result) : m_result(result)
{
}

QUrl TestURIResolver::resolve(const QUrl &relative, const QUrl &baseURI) const
{
    Q_UNUSED(relative);
    Q_UNUSED(baseURI);
    return baseURI.resolved(m_result);
}

#endif
