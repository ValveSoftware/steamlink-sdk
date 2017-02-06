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

#include <QApplication>
#include <QtQuick>

#include "testmodel.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  QQuickView viewer;
  viewer.setResizeMode(QQuickView::SizeRootObjectToView);

  QObjectList model_qobjectlist;
  model_qobjectlist << new TestObject(0);
  model_qobjectlist << new TestObject(1);
  model_qobjectlist << new TestObject(2);
  viewer.rootContext()->setContextProperty("model_qobjectlist", QVariant::fromValue(model_qobjectlist));

  TestObject *model_qobject = new TestObject(0);
  viewer.rootContext()->setContextProperty("model_qobject", QVariant::fromValue(model_qobject));

  QStringList model_qstringlist;
  model_qstringlist << QStringLiteral("A");
  model_qstringlist << QStringLiteral("B");
  model_qstringlist << QStringLiteral("C");
  viewer.rootContext()->setContextProperty("model_qstringlist", model_qstringlist);

  TestModel *model_qaim = new TestModel;
  viewer.rootContext()->setContextProperty("model_qaim", model_qaim);

  viewer.setSource(QStringLiteral("qrc:/qml/main.qml"));
  viewer.show();

  return app.exec();
}
