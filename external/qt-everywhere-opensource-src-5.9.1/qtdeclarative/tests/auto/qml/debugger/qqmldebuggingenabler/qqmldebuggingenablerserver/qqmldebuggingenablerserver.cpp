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

#include <QtCore/qcoreapplication.h>
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qdebug.h>
#include <QtQml/qqmldebug.h>
#include <QtQml/qqmlengine.h>

int main(int argc, char *argv[])
{
      QQmlDebuggingEnabler::StartMode block = QQmlDebuggingEnabler::DoNotWaitForClient;
      int portFrom = 0;
      int portTo = 0;

      QCoreApplication app(argc, argv);
      QStringList arguments = app.arguments();
      arguments.removeFirst();
      QString connector = QLatin1String("QQmlDebugServer");

      if (arguments.size() && arguments.first() == QLatin1String("-block")) {
          block = QQmlDebuggingEnabler::WaitForClient;
          arguments.removeFirst();
      }

      if (arguments.size() >= 2 && arguments.first() == QLatin1String("-connector")) {
          arguments.removeFirst();
          connector = arguments.takeFirst();
      }

      if (arguments.size() >= 2) {
          portFrom = arguments.takeFirst().toInt();
          portTo = arguments.takeFirst().toInt();
      }

      if (arguments.size() && arguments.takeFirst() == QLatin1String("-services"))
          QQmlDebuggingEnabler::setServices(arguments);

      if (connector == QLatin1String("QQmlDebugServer")) {
          if (!portFrom || !portTo)
              qFatal("Port range has to be specified.");

          while (portFrom <= portTo)
              QQmlDebuggingEnabler::startTcpDebugServer(portFrom++, block);
      } else if (connector == QLatin1String("QQmlNativeDebugConnector")) {
          QVariantHash configuration;
          configuration[QLatin1String("block")] = (block == QQmlDebuggingEnabler::WaitForClient);
          QQmlDebuggingEnabler::startDebugConnector(connector, configuration);
      }

      QQmlEngine engine;
      qDebug() << "QQmlEngine created\n";
      Q_UNUSED(engine);
      return app.exec();
}

