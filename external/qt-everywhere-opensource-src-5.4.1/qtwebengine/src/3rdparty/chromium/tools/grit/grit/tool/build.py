#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit build' tool along with integration for this tool with the
SCons build system.
'''

import filecmp
import getopt
import os
import shutil
import sys

from grit import grd_reader
from grit import util
from grit.tool import interface
from grit import shortcuts


# It would be cleaner to have each module register itself, but that would
# require importing all of them on every run of GRIT.
'''Map from <output> node types to modules under grit.format.'''
_format_modules = {
  'android':                  'android_xml',
  'c_format':                 'c_format',
  'chrome_messages_json':     'chrome_messages_json',
  'data_package':             'data_pack',
  'js_map_format':            'js_map_format',
  'rc_all':                   'rc',
  'rc_translateable':         'rc',
  'rc_nontranslateable':      'rc',
  'rc_header':                'rc_header',
  'resource_map_header':      'resource_map',
  'resource_map_source':      'resource_map',
  'resource_file_map_source': 'resource_map',
}
_format_modules.update(
    (type, 'policy_templates.template_formatter') for type in
        [ 'adm', 'admx', 'adml', 'reg', 'doc', 'json',
          'plist', 'plist_strings', 'ios_plist' ])


def GetFormatter(type):
  modulename = 'grit.format.' + _format_modules[type]
  __import__(modulename)
  module = sys.modules[modulename]
  try:
    return module.Format
  except AttributeError:
    return module.GetFormatter(type)


class RcBuilder(interface.Tool):
  '''A tool that builds RC files and resource header files for compilation.

Usage:  grit build [-o OUTPUTDIR] [-D NAME[=VAL]]*

All output options for this tool are specified in the input file (see
'grit help' for details on how to specify the input file - it is a global
option).

Options:

  -o OUTPUTDIR      Specify what directory output paths are relative to.
                    Defaults to the current directory.

  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

  -E NAME=VALUE     Set environment variable NAME to VALUE (within grit).

  -f FIRSTIDSFILE   Path to a python file that specifies the first id of
                    value to use for resources.  A non-empty value here will
                    override the value specified in the <grit> node's
                    first_ids_file.

  -w WHITELISTFILE  Path to a file containing the string names of the
                    resources to include.  Anything not listed is dropped.

  -t PLATFORM       Specifies the platform the build is targeting; defaults
                    to the value of sys.platform. The value provided via this
                    flag should match what sys.platform would report for your
                    target platform; see grit.node.base.EvaluateCondition.

  -h HEADERFORMAT   Custom format string to use for generating rc header files.
                    The string should have two placeholders: {textual_id}
                    and {numeric_id}. E.g. "#define {textual_id} {numeric_id}"
                    Otherwise it will use the default "#define SYMBOL 1234"

Conditional inclusion of resources only affects the output of files which
control which resources get linked into a binary, e.g. it affects .rc files
meant for compilation but it does not affect resource header files (that define
IDs).  This helps ensure that values of IDs stay the same, that all messages
are exported to translation interchange files (e.g. XMB files), etc.
'''

  def ShortDescription(self):
    return 'A tool that builds RC files for compilation.'

  def Run(self, opts, args):
    self.output_directory = '.'
    first_ids_file = None
    whitelist_filenames = []
    target_platform = None
    depfile = None
    depdir = None
    rc_header_format = None
    (own_opts, args) = getopt.getopt(args, 'o:D:E:f:w:t:h:', ('depdir=','depfile='))
    for (key, val) in own_opts:
      if key == '-o':
        self.output_directory = val
      elif key == '-D':
        name, val = util.ParseDefine(val)
        self.defines[name] = val
      elif key == '-E':
        (env_name, env_value) = val.split('=', 1)
        os.environ[env_name] = env_value
      elif key == '-f':
        # TODO(joi@chromium.org): Remove this override once change
        # lands in WebKit.grd to specify the first_ids_file in the
        # .grd itself.
        first_ids_file = val
      elif key == '-w':
        whitelist_filenames.append(val)
      elif key == '-t':
        target_platform = val
      elif key == '-h':
        rc_header_format = val
      elif key == '--depdir':
        depdir = val
      elif key == '--depfile':
        depfile = val

    if len(args):
      print 'This tool takes no tool-specific arguments.'
      return 2
    self.SetOptions(opts)
    if self.scons_targets:
      self.VerboseOut('Using SCons targets to identify files to output.\n')
    else:
      self.VerboseOut('Output directory: %s (absolute path: %s)\n' %
                      (self.output_directory,
                       os.path.abspath(self.output_directory)))

    if whitelist_filenames:
      self.whitelist_names = set()
      for whitelist_filename in whitelist_filenames:
        self.VerboseOut('Using whitelist: %s\n' % whitelist_filename);
        whitelist_contents = util.ReadFile(whitelist_filename, util.RAW_TEXT)
        self.whitelist_names.update(whitelist_contents.strip().split('\n'))

    self.res = grd_reader.Parse(opts.input,
                                debug=opts.extra_verbose,
                                first_ids_file=first_ids_file,
                                defines=self.defines,
                                target_platform=target_platform)
    # Set an output context so that conditionals can use defines during the
    # gathering stage; we use a dummy language here since we are not outputting
    # a specific language.
    self.res.SetOutputLanguage('en')
    if rc_header_format:
      self.res.AssignRcHeaderFormat(rc_header_format)
    self.res.RunGatherers()
    self.Process()

    if depfile and depdir:
      self.GenerateDepfile(opts.input, depfile, depdir)

    return 0

  def __init__(self, defines=None):
    # Default file-creation function is built-in open().  Only done to allow
    # overriding by unit test.
    self.fo_create = open

    # key/value pairs of C-preprocessor like defines that are used for
    # conditional output of resources
    self.defines = defines or {}

    # self.res is a fully-populated resource tree if Run()
    # has been called, otherwise None.
    self.res = None

    # Set to a list of filenames for the output nodes that are relative
    # to the current working directory.  They are in the same order as the
    # output nodes in the file.
    self.scons_targets = None

    # The set of names that are whitelisted to actually be included in the
    # output.
    self.whitelist_names = None

  @staticmethod
  def AddWhitelistTags(start_node, whitelist_names):
    # Walk the tree of nodes added attributes for the nodes that shouldn't
    # be written into the target files (skip markers).
    from grit.node import include
    from grit.node import message
    for node in start_node:
      # Same trick data_pack.py uses to see what nodes actually result in
      # real items.
      if (isinstance(node, include.IncludeNode) or
          isinstance(node, message.MessageNode)):
        text_ids = node.GetTextualIds()
        # Mark the item to be skipped if it wasn't in the whitelist.
        if text_ids and text_ids[0] not in whitelist_names:
          node.SetWhitelistMarkedAsSkip(True)

  @staticmethod
  def ProcessNode(node, output_node, outfile):
    '''Processes a node in-order, calling its formatter before and after
    recursing to its children.

    Args:
      node: grit.node.base.Node subclass
      output_node: grit.node.io.OutputNode
      outfile: open filehandle
    '''
    base_dir = util.dirname(output_node.GetOutputFilename())

    formatter = GetFormatter(output_node.GetType())
    formatted = formatter(node, output_node.GetLanguage(), output_dir=base_dir)
    outfile.writelines(formatted)


  def Process(self):
    # Update filenames with those provided by SCons if we're being invoked
    # from SCons.  The list of SCons targets also includes all <structure>
    # node outputs, but it starts with our output files, in the order they
    # occur in the .grd
    if self.scons_targets:
      assert len(self.scons_targets) >= len(self.res.GetOutputFiles())
      outfiles = self.res.GetOutputFiles()
      for ix in range(len(outfiles)):
        outfiles[ix].output_filename = os.path.abspath(
          self.scons_targets[ix])
    else:
      for output in self.res.GetOutputFiles():
        output.output_filename = os.path.abspath(os.path.join(
          self.output_directory, output.GetFilename()))

    # If there are whitelisted names, tag the tree once up front, this way
    # while looping through the actual output, it is just an attribute check.
    if self.whitelist_names:
      self.AddWhitelistTags(self.res, self.whitelist_names)

    for output in self.res.GetOutputFiles():
      self.VerboseOut('Creating %s...' % output.GetFilename())

      # Microsoft's RC compiler can only deal with single-byte or double-byte
      # files (no UTF-8), so we make all RC files UTF-16 to support all
      # character sets.
      if output.GetType() in ('rc_header', 'resource_map_header',
          'resource_map_source', 'resource_file_map_source'):
        encoding = 'cp1252'
      elif output.GetType() in ('android', 'c_format', 'js_map_format', 'plist',
                                'plist_strings', 'doc', 'json'):
        encoding = 'utf_8'
      elif output.GetType() in ('chrome_messages_json'):
        # Chrome Web Store currently expects BOM for UTF-8 files :-(
        encoding = 'utf-8-sig'
      else:
        # TODO(gfeher) modify here to set utf-8 encoding for admx/adml
        encoding = 'utf_16'

      # Set the context, for conditional inclusion of resources
      self.res.SetOutputLanguage(output.GetLanguage())
      self.res.SetOutputContext(output.GetContext())
      self.res.SetDefines(self.defines)

      # Make the output directory if it doesn't exist.
      self.MakeDirectoriesTo(output.GetOutputFilename())

      # Write the results to a temporary file and only overwrite the original
      # if the file changed.  This avoids unnecessary rebuilds.
      outfile = self.fo_create(output.GetOutputFilename() + '.tmp', 'wb')

      if output.GetType() != 'data_package':
        outfile = util.WrapOutputStream(outfile, encoding)

      # Iterate in-order through entire resource tree, calling formatters on
      # the entry into a node and on exit out of it.
      with outfile:
        self.ProcessNode(self.res, output, outfile)

      # Now copy from the temp file back to the real output, but on Windows,
      # only if the real output doesn't exist or the contents of the file
      # changed.  This prevents identical headers from being written and .cc
      # files from recompiling (which is painful on Windows).
      if not os.path.exists(output.GetOutputFilename()):
        os.rename(output.GetOutputFilename() + '.tmp',
                  output.GetOutputFilename())
      else:
        # CHROMIUM SPECIFIC CHANGE.
        # This clashes with gyp + vstudio, which expect the output timestamp
        # to change on a rebuild, even if nothing has changed.
        #files_match = filecmp.cmp(output.GetOutputFilename(),
        #    output.GetOutputFilename() + '.tmp')
        #if (output.GetType() != 'rc_header' or not files_match
        #    or sys.platform != 'win32'):
        shutil.copy2(output.GetOutputFilename() + '.tmp',
                     output.GetOutputFilename())
        os.remove(output.GetOutputFilename() + '.tmp')

      self.VerboseOut(' done.\n')

    # Print warnings if there are any duplicate shortcuts.
    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(
        self.res.UberClique(), self.res.GetTcProject())
    if warnings:
      print '\n'.join(warnings)

    # Print out any fallback warnings, and missing translation errors, and
    # exit with an error code if there are missing translations in a non-pseudo
    # and non-official build.
    warnings = (self.res.UberClique().MissingTranslationsReport().
        encode('ascii', 'replace'))
    if warnings:
      self.VerboseOut(warnings)
    if self.res.UberClique().HasMissingTranslations():
      print self.res.UberClique().missing_translations_
      sys.exit(-1)

  def GenerateDepfile(self, input_filename, depfile, depdir):
    '''Generate a depfile that contains the imlicit dependencies of the input
    grd. The depfile will be in the same format as a makefile, and will contain
    references to files relative to |depdir|. It will be put in |depfile|.

    For example, supposing we have three files in a directory src/

    src/
      blah.grd    <- depends on input{1,2}.xtb
      input1.xtb
      input2.xtb

    and we run

      grit -i blah.grd -o ../out/gen --depdir ../out --depfile ../out/gen/blah.rd.d

    from the directory src/ we will generate a depfile ../out/gen/blah.grd.d
    that has the contents

      gen/blah.grd.d: ../src/input1.xtb ../src/input2.xtb

    Note that all paths in the depfile are relative to ../out, the depdir.
    '''
    depfile = os.path.abspath(depfile)
    depdir = os.path.abspath(depdir)
    # The path prefix to prepend to dependencies in the depfile.
    prefix = os.path.relpath(os.getcwd(), depdir)
    # The path that the depfile refers to itself by.
    self_ref_depfile = os.path.relpath(depfile, depdir)
    infiles = self.res.GetInputFiles()
    deps_text = ' '.join([os.path.join(prefix, i) for i in infiles])
    depfile_contents = self_ref_depfile + ': ' + deps_text
    self.MakeDirectoriesTo(depfile)
    outfile = self.fo_create(depfile, 'wb')
    outfile.writelines(depfile_contents)

  @staticmethod
  def MakeDirectoriesTo(file):
    '''Creates directories necessary to contain |file|.'''
    dir = os.path.split(file)[0]
    if not os.path.exists(dir):
      os.makedirs(dir)
