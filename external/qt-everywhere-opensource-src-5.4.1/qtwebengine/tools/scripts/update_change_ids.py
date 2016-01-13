#!/usr/bin/env python

#############################################################################
#
# Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
import subprocess
import sys
import tempfile
import os
import git_submodule as submodule

if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
    sys.exit('\nUsage: ' + sys.argv[0] + ' /path/to/patches/dir\n')

def findGitDir():
    try:
        return subprocess.check_output(['git', 'rev-parse', '--git-dir']).strip()
    except subprocess.CalledProcessError, e:
        sys.exit('git dir could not be determined! ' + e.output)

change_id_script = os.path.join(findGitDir(), 'hooks', 'commit-msg')

if not os.path.isfile(change_id_script) or not os.access(change_id_script, os.X_OK):
    sys.exit('-- failed to find gerrit git hooks script: ' + change_id_script + ' --')

patches = glob.glob(os.path.join(sys.argv[1], '00*.patch'))
for patch in patches:
    message = ''
    with open(patch, 'r') as pfile:
        msg = tempfile.NamedTemporaryFile(delete=False)
        done = False
        for line in pfile:
            if 'Change-Id' in line:
                continue
            if not done and line.startswith('---'):
                msg.write(message)
                msg.close()
                error = subprocess.call([change_id_script, msg.name]);
                if error != 0:
                    sys.exit('-- updating commit message failed --')
                with open(msg.name, 'r') as msg:
                    message = msg.read()
                os.unlink(msg.name)
                done = True
            message += line
    with open(patch, "w") as pfile:
        pfile.write(message)
