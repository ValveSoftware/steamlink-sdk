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

#include <qtest.h>

#include <QFile>
#include <QSvgRenderer>

class tst_QSvgRenderer : public QObject
{
    Q_OBJECT

public:
    tst_QSvgRenderer();
    virtual ~tst_QSvgRenderer();

public slots:
    void init();
    void cleanup();

private slots:
    void construct();
    void load();
};

tst_QSvgRenderer::tst_QSvgRenderer()
{
}

tst_QSvgRenderer::~tst_QSvgRenderer()
{
}

void tst_QSvgRenderer::init()
{
}

void tst_QSvgRenderer::cleanup()
{
}

void tst_QSvgRenderer::construct()
{
    QBENCHMARK {
        QSvgRenderer renderer;
    }
}

void tst_QSvgRenderer::load()
{
    QFile file(":/data/tiger.svg");
    if (!file.open(QFile::ReadOnly))
        QFAIL("Can not open tiger.svg");
    QByteArray data = file.readAll();
    QSvgRenderer renderer;

    QBENCHMARK {
        renderer.load(data);
    }
}

QTEST_MAIN(tst_QSvgRenderer)
#include "tst_qsvgrenderer.moc"
