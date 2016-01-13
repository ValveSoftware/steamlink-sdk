# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

import idl_schema
import json_schema
from model import Model, UnixName

class SchemaLoader(object):
  '''Resolves a type name into the namespace the type belongs to.

  Properties:
  - |display_path| path to the directory with the API header files, intended for
  use with the Model.
  - |real_path| path to the directory with the API header files, used for file
  access.
  '''
  def __init__(self, display_path, real_path):
    self._display_path = display_path
    self._real_path = real_path

  def ResolveType(self, full_name, default_namespace):
    name_parts = full_name.rsplit('.', 1)
    if len(name_parts) == 1:
      if full_name not in default_namespace.types:
        return None
      return default_namespace
    namespace_name, type_name = name_parts
    real_name = None
    # Try to find the file defining the namespace. Eg. for
    # nameSpace.sub_name_space.Type' the following heuristics looks for:
    # 1. name_space_sub_name_space.json,
    # 2. name_space_sub_name_space.idl.
    for ext in ['json', 'idl']:
      basename = UnixName(namespace_name)
      filename = '%s.%s' % (basename, ext)
      filepath = os.path.join(self._real_path, filename);
      if os.path.exists(filepath):
        real_name = filename
        break
    if real_name is None:
      return None
    namespace = Model().AddNamespace(
        self.LoadSchema(real_name)[0],
        os.path.join(self._display_path, real_name))
    if type_name not in namespace.types:
      return None
    return namespace

  def LoadSchema(self, schema):
    '''Load a schema definition. The schema parameter must be a file name
    without any path component - the file is loaded from the path defined by
    the real_path argument passed to the constructor.'''
    schema_filename, schema_extension = os.path.splitext(schema)

    schema_path = os.path.join(self._real_path, schema);
    if schema_extension == '.json':
      api_defs = json_schema.Load(schema_path)
    elif schema_extension == '.idl':
      api_defs = idl_schema.Load(schema_path)
    else:
      sys.exit('Did not recognize file extension %s for schema %s' %
               (schema_extension, schema))

    return api_defs
