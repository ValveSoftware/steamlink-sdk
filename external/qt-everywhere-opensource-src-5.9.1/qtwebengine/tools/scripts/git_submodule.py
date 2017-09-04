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

import glob
import os
import re
import subprocess
import sys
import version_resolver as resolver

extra_os = ['mac', 'win']

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
        self.topmost_supermodule_path_prefix = ''

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
                # Don't skip submodules that have a supermodule path prefix set (at the moment these
                # are 2nd level deep submodules).
                elif not self.topmost_supermodule_path_prefix:
                    # Ignore the information about chromium itself since we get that from git,
                    # also ignore anything outside src/ (e.g. depot_tools)
                    continue

                submodule = Submodule(subdir, repo, sp=self.topmost_supermodule_path_prefix)
                submodule.os = os

                if not submodule.matchesOS():
                    print '-- skipping ' + submodule.pathRelativeToTopMostSupermodule() + ' for this operating system. --'
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
        if 'deps_os' in self.local_scope:
            for os_dep in self.local_scope['deps_os']:
                submodules.extend(self.createSubmodulesFromScope(self.local_scope['deps_os'][os_dep], os_dep))
        return submodules

# Strips suffix from end of text.
def strip_end(text, suffix):
    if not text.endswith(suffix):
        return text
    return text[:len(text)-len(suffix)]

# Given supermodule_path = /chromium
#      current directory = /chromium/buildtools
#         submodule_path = third_party/foo/bar
# returns                = buildtools
def computeRelativePathPrefixToTopMostSupermodule(submodule_path, supermodule_path):
    relpath = os.path.relpath(submodule_path, supermodule_path)
    topmost_supermodule_path_prefix = strip_end(relpath, submodule_path)
    return topmost_supermodule_path_prefix

class Submodule:
    def __init__(self, path='', url='', ref='', os=[], sp=''):
        self.path = path
        self.url = url
        self.ref = ref
        self.os = os
        self.topmost_supermodule_path_prefix = sp

    def pathRelativeToTopMostSupermodule(self):
        return os.path.normpath(os.path.join(self.topmost_supermodule_path_prefix, self.path))

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
            print '\n\n-- initializing ' + self.pathRelativeToTopMostSupermodule() + ' --'
            oldCwd = os.getcwd()

            # The submodule operations should be done relative to the current submodule's
            # supermodule.
            if self.topmost_supermodule_path_prefix:
                os.chdir(self.topmost_supermodule_path_prefix)

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
        if self.matchesOS() and os.path.isdir(self.pathRelativeToTopMostSupermodule()):
            currentDir = os.getcwd()
            os.chdir(self.pathRelativeToTopMostSupermodule())
            files = subprocessCheckOutput(['git', 'ls-files']).splitlines()
            os.chdir(currentDir)
            return files
        else:
            print '-- skipping ' + self.path + ' for this operating system. --'
            return []

    def parseGitModulesFileContents(self, gitmodules_lines):
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

    # Return a flattened list of submodules starting from module, and recursively collecting child
    # submodules.
    def readSubmodulesFromGitModules(self, module, gitmodules_file_name, top_level_path):
        flattened_submodules = []
        oldCwd = os.getcwd()
        os.chdir(module.path)

        if os.path.isfile(gitmodules_file_name):
            gitmodules_file = open(gitmodules_file_name)
            gitmodules_lines = gitmodules_file.readlines()
            gitmodules_file.close()
            submodules = self.parseGitModulesFileContents(gitmodules_lines)

            # When inside a 2nd level submodule or deeper, store the path relative to the topmost
            # module.
            for submodule in submodules:
                submodule.topmost_supermodule_path_prefix = computeRelativePathPrefixToTopMostSupermodule(submodule.path, top_level_path)

            flattened_submodules.extend(submodules)

            # Recurse into deeper submodules.
            for submodule in submodules:
                flattened_submodules.extend(self.readSubmodulesFromGitModules(submodule, gitmodules_file_name, top_level_path))

        os.chdir(oldCwd)
        return flattened_submodules

    def readSubmodules(self):
        submodules = []
        if self.ref:
            submodules = resolver.readSubmodules()
            print 'DEPS file provides the following submodules:'
            for submodule in submodules:
                print '{:<80}'.format(submodule.pathRelativeToTopMostSupermodule()) + '{:<120}'.format(submodule.url) + submodule.ref
        else: # Try .gitmodules since no ref has been specified
            gitmodules_file_name = '.gitmodules'
            submodules = self.readSubmodulesFromGitModules(self, gitmodules_file_name, self.path)
        return submodules

    def initSubmodules(self):
        oldCwd = os.getcwd()
        os.chdir(self.path)
        submodules = self.readSubmodules()
        for submodule in submodules:
            submodule.initialize()
        subprocessCall(['git', 'commit', '-a', '--amend', '--no-edit'])
        os.chdir(oldCwd)
