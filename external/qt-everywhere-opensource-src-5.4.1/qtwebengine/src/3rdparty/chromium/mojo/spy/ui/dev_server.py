# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import tvcm

from ui import spy_project


def Main(port, args):
  parser = optparse.OptionParser()
  _, args = parser.parse_args(args)

  project = spy_project.SpyProject()
  server = tvcm.DevServer(
      port=port, project=project)

  def IsTestModuleResourcePartOfSpy(module_resource):
    return module_resource.absolute_path.startswith(project.spy_path)

  server.test_module_resource_filter = IsTestModuleResourcePartOfSpy
  return server.serve_forever()
