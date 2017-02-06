#!/usr/bin/env python

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtWebEngine module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import os
import subprocess
import sys

def getChromiumSrcDir():

  saved_cwd = os.getcwd()
  qtwebengine_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))

  os.chdir(qtwebengine_root)
  try:
    chrome_src = subprocess.check_output("git config qtwebengine.chromiumsrcdir", shell=True).strip()
  except subprocess.CalledProcessError:
    chrome_src = None
  os.chdir(saved_cwd)

  if chrome_src:
    chrome_src = os.path.join(qtwebengine_root, chrome_src)
    print('Using external chromium sources specified in git config qtwebengine.chromiumsrcdir: ' + chrome_src)
  if not chrome_src or not os.path.isdir(chrome_src):
    chrome_src = os.path.normpath(os.path.join(qtwebengine_root, 'src/3rdparty/chromium'))
  return os.path.normcase(chrome_src)

