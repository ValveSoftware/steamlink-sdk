# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Node objects for the AST for a Mojo IDL file."""

# Note: For convenience of testing, you probably want to define __eq__() methods
# for all node types; it's okay to be slightly lax (e.g., not compare filename
# and lineno).


class BaseNode(object):
  def __init__(self, filename=None, lineno=None):
    self.filename = filename
    self.lineno = lineno


class Ordinal(BaseNode):
  """Represents an ordinal value labeling, e.g., a struct field."""
  def __init__(self, value, **kwargs):
    BaseNode.__init__(self, **kwargs)
    self.value = value

  def __eq__(self, other):
    return self.value == other.value
