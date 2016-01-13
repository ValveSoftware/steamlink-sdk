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
import subprocess
import sys
import version_resolver as resolver

extra_os = ['android', 'mac', 'win']

def subprocessCall(args):
    print args
    return subprocess.call(args)

def subprocessCheckOutput(args):
    print args
    return subprocess.check_output(args)

class DEPSParser:
    def __init__(self):
        self.global_scope = {
          'Var': self.Lookup,
          'deps_os': {},
        }
        self.local_scope = {}

    def Lookup(self, var_name):
        return self.local_scope["vars"][var_name]

    def createSubmodulesFromScope(self, scope, os):
        submodules = []
        for dep in scope:
            if (type(scope[dep]) == str):
                repo_rev = scope[dep].split('@')
                repo = repo_rev[0]
                rev = repo_rev[1]
                subdir = dep
                if subdir.startswith('src/'):
                    subdir = subdir[4:]
                else:
                    # Ignore the information about chromium itself since we get that from git,
                    # also ignore anything outside src/ (e.g. depot_tools)
                    continue

                submodule = Submodule(subdir, repo)
                submodule.os = os

                if not submodule.matchesOS():
                    print '-- skipping ' + submodule.path + ' for this operating system. --'
                    continue

                if len(rev) == 40: # Length of a git shasum
                    submodule.ref = rev
                else:
                    sys.exit("Invalid shasum: " + str(rev))
                submodules.append(submodule)
        return submodules

    def parse(self, deps_content):
        exec(deps_content, self.global_scope, self.local_scope)

        submodules = []
        submodules.extend(self.createSubmodulesFromScope(self.local_scope['deps'], 'all'))
        for os_dep in self.local_scope['deps_os']:
            submodules.extend(self.createSubmodulesFromScope(self.local_scope['deps_os'][os_dep], os_dep))
        return submodules

class Submodule:
    def __init__(self, path='', url='', ref='', os=[]):
        self.path = path
        self.url = url
        self.ref = ref
        self.os = os

    def matchesOS(self):
        if not self.os:
            return True
        if 'all' in self.os:
            return True
        if sys.platform.startswith('linux') and 'unix' in self.os:
            return True
        if sys.platform.startswith('darwin') and ('unix' in self.os or 'mac' in self.os):
            return True
        if sys.platform.startswith('win32') or sys.platform.startswith('cygwin'):
            if 'win' in self.os:
                return True
            else:
                # Skipping all dependecies of the extra_os on Windows platform, because it caused confict.
                return False
        for os in extra_os:
            if os in self.os:
                return True
        return False

    def findShaAndCheckout(self):
        oldCwd = os.getcwd()
        os.chdir(self.path)

        # Fetch the shasum we parsed from the DEPS file.
        error = subprocessCall(['git', 'fetch', 'origin', self.ref])
        if error != 0:
            print('ERROR: Could not fetch ' + self.ref + ' from upstream origin.')
            return error

        error = subprocessCall(['git', 'checkout', 'FETCH_HEAD']);

        current_shasum = subprocessCheckOutput(['git', 'rev-parse', 'HEAD']).strip()
        current_tag = subprocessCheckOutput(['git', 'name-rev', '--tags', '--name-only', current_shasum]).strip()

        if current_tag == resolver.currentVersion():
            # We checked out a tagged version of chromium.
            self.ref = current_shasum

        if not self.ref:
            # No shasum could be deduced, use the submodule shasum.
            os.chdir(oldCwd)
            line = subprocessCheckOutput(['git', 'submodule', 'status', self.path])
            os.chdir(self.path)
            line = line.lstrip(' -')
            self.ref = line.split(' ')[0]

        if not self.ref.startswith(current_shasum):
            # In case HEAD differs check out the actual shasum we require.
            subprocessCall(['git', 'fetch'])
            error = subprocessCall(['git', 'checkout', self.ref])
        os.chdir(oldCwd)
        return error

    def findGitDir(self):
        try:
            return subprocessCheckOutput(['git', 'rev-parse', '--git-dir']).strip()
        except subprocess.CalledProcessError, e:
            sys.exit("git dir could not be determined! - Initialization failed! " + e.output)

    def reset(self):
        currentDir = os.getcwd()
        os.chdir(self.path)
        gitdir = self.findGitDir()
        if os.path.isdir(os.path.join(gitdir, 'rebase-merge')):
            if os.path.isfile(os.path.join(gitdir, 'MERGE_HEAD')):
                print 'merge in progress... aborting merge.'
                subprocessCall(['git', 'merge', '--abort'])
            else:
                print 'rebase in progress... aborting merge.'
                subprocessCall(['git', 'rebase', '--abort'])
        if os.path.isdir(os.path.join(gitdir, 'rebase-apply')):
            print 'am in progress... aborting am.'
            subprocessCall(['git', 'am', '--abort'])
        subprocessCall(['git', 'reset', '--hard'])
        os.chdir(currentDir)

    def initialize(self):
        if self.matchesOS():
            print '\n\n-- initializing ' + self.path + ' --'
            oldCwd = os.getcwd()
            if os.path.isdir(self.path):
                self.reset()

            if self.url:
                subprocessCall(['git', 'submodule', 'add', '-f', self.url, self.path])
            subprocessCall(['git', 'submodule', 'sync', '--', self.path])
            subprocessCall(['git', 'submodule', 'init', self.path])
            subprocessCall(['git', 'submodule', 'update', self.path])

            if '3rdparty_upstream' in os.path.abspath(self.path):
                if self.findShaAndCheckout() != 0:
                    sys.exit("!!! initialization failed !!!")

                # Add baseline commit for upstream repository to be able to reset.
                os.chdir(self.path)
                commit = subprocessCheckOutput(['git', 'rev-list', '--max-count=1', 'HEAD'])
                subprocessCall(['git', 'commit', '-a', '--allow-empty', '-m', '-- QtWebEngine baseline --\n\ncommit ' + commit])

            os.chdir(oldCwd)
        else:
            print '-- skipping ' + self.path + ' for this operating system. --'

    def listFiles(self):
        if self.matchesOS() and os.path.isdir(self.path):
            currentDir = os.getcwd()
            os.chdir(self.path)
            files = subprocessCheckOutput(['git', 'ls-files']).splitlines()
            os.chdir(currentDir)
            return files
        else:
            print '-- skipping ' + self.path + ' for this operating system. --'
            return []


    def readSubmodules(self):
        submodules = []
        if self.ref:
            submodules = resolver.readSubmodules()
            print 'DEPS file provides the following submodules:'
            for submodule in submodules:
                print '{:<80}'.format(submodule.path) + '{:<120}'.format(submodule.url) + submodule.ref
        else: # Try .gitmodules since no ref has been specified
            if not os.path.isfile('.gitmodules'):
                return []
            gitmodules_file = open('.gitmodules')
            gitmodules_lines = gitmodules_file.readlines()
            gitmodules_file.close()

            submodules = []
            currentSubmodule = None
            for line in gitmodules_lines:
                if line.find('[submodule') == 0:
                    if currentSubmodule:
                        submodules.append(currentSubmodule)
                    currentSubmodule = Submodule()
                tokens = line.split('=')
                if len(tokens) >= 2:
                    key = tokens[0].strip()
                    value = tokens[1].strip()
                    if key == 'path':
                        currentSubmodule.path = value
                    elif key == 'url':
                        currentSubmodule.url = value
                    elif key == 'os':
                        currentSubmodule.os = value.split(',')
            if currentSubmodule:
                submodules.append(currentSubmodule)
        return submodules

    def initSubmodules(self):
        oldCwd = os.getcwd()
        os.chdir(self.path)
        submodules = self.readSubmodules()
        for submodule in submodules:
            submodule.initialize()
        subprocessCall(['git', 'commit', '-a', '--amend', '--no-edit'])
        os.chdir(oldCwd)
