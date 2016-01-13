# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
import os
import optparse

from ui import spy_project
from tvcm import generate

def Main(args):
  parser = optparse.OptionParser()
  parser.add_option('--output-file', '-o')
  options,args = parser.parse_args(args)

  if options.output_file:
    ofile = open(options.output_file, 'w')
  else:
    ofile = sys.stdout
  GenerateHTML(ofile)
  if ofile != sys.stdout:
    ofile.close()

def GenerateHTML(ofile):
  project = spy_project.SpyProject()
  load_sequence = project.CalcLoadSequenceForModuleNames(
    ['ui.spy_shell'])
  bootstrap_js = """

  document.addEventListener('DOMContentLoaded', function() {
    document.body.appendChild(new ui.SpyShell('ws://127.0.0.1:42424'));

  });
"""
  bootstrap_script = generate.ExtraScript(text_content=bootstrap_js)
  generate.GenerateStandaloneHTMLToFile(
    ofile, load_sequence,
    title='Mojo spy',
    extra_scripts=[bootstrap_script])
