#!/usr/bin/env python

#############################################################################
#
# Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
# Contact: http://www.qt-project.org/legal
#
# This file is part of the QtWebEngine module of the Qt Toolkit.
#
# $QT_BEGIN_LICENSE:LGPL$
# Commercial License Usage
# Licensees holding valid commercial Qt licenses may use this file in
# accordance with the commercial license agreement provided with the
# Software or, alternatively, in accordance with the terms contained in
# a written agreement between you and Digia.  For licensing terms and
# conditions see http://qt.digia.com/licensing.  For further information
# use the contact form at http://qt.digia.com/contact-us.
#
# GNU Lesser General Public License Usage
# Alternatively, this file may be used under the terms of the GNU Lesser
# General Public License version 2.1 as published by the Free Software
# Foundation and appearing in the file LICENSE.LGPL included in the
# packaging of this file.  Please review the following information to
# ensure the GNU Lesser General Public License version 2.1 requirements
# will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# In addition, as a special exception, Digia gives you certain additional
# rights.  These rights are described in the Digia Qt LGPL Exception
# version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3.0 as published by the Free Software
# Foundation and appearing in the file LICENSE.GPL included in the
# packaging of this file.  Please review the following information to
# ensure the GNU General Public License version 3.0 requirements will be
# met: http://www.gnu.org/copyleft/gpl.html.
#
#
# $QT_END_LICENSE$
#
#############################################################################

import glob
import os
import re
import shutil
import subprocess
import sys

qtwebengine_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(qtwebengine_root, 'tools', 'scripts'))

import version_resolver as resolver

os.chdir(os.path.join(resolver.upstream_src_dir, 'chromium'))

if len(sys.argv) != 1:
    if "--reset" in sys.argv:
        resolver.resetUpstream()
        sys.exit()
    else:
        sys.exit('unknown option, try --reset or w/o options\n')

if not len(resolver.findSnapshotBaselineSha1()):
    sys.exit('\n-- missing chromium snapshot patches, try running init-repository.py w/o options first --')

patches = resolver.preparePatchesFromSnapshot()

for path in patches:
    leading = path.count('/') + 2
    target_dir = ""

    if path.startswith('chromium'):
        target_dir = os.path.join(resolver.upstream_src_dir, path)
    else:
        target_dir = os.path.join(resolver.upstream_src_dir, 'chromium', path)
        leading += 1

    if not os.path.isdir(target_dir):
        # Skip applying patches for non-existing submodules
        print('\n-- missing '+ target_dir + ', skipping --')
        continue

    os.chdir(target_dir)
    print('\n-- entering '+ os.getcwd() + ' --')

    # Sort the patches to be able to apply them in order
    patch_list = sorted(patches[path])
    for patch in patch_list:
        error = subprocess.call(['git', 'am', '-p' + str(leading), patch])
        if error != 0:
            sys.exit('-- git am ' + patch + ' failed in ' + os.getcwd() + ' --')

print('\n-- done --')

