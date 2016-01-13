# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This module's classes provide an interface to mojo modules. Modules are
# collections of interfaces and structs to be used by mojo ipc clients and
# servers.
#
# A simple interface would be created this way:
# module = mojom.generate.module.Module('Foo')
# interface = module.AddInterface('Bar')
# method = interface.AddMethod('Tat', 0)
# method.AddParameter('baz', 0, mojom.INT32)
#

class Kind(object):
  def __init__(self, spec=None):
    self.spec = spec
    self.parent_kind = None

# Initialize the set of primitive types. These can be accessed by clients.
BOOL         = Kind('b')
INT8         = Kind('i8')
INT16        = Kind('i16')
INT32        = Kind('i32')
INT64        = Kind('i64')
UINT8        = Kind('u8')
UINT16       = Kind('u16')
UINT32       = Kind('u32')
UINT64       = Kind('u64')
FLOAT        = Kind('f')
DOUBLE       = Kind('d')
STRING       = Kind('s')
HANDLE       = Kind('h')
DCPIPE       = Kind('h:d:c')
DPPIPE       = Kind('h:d:p')
MSGPIPE      = Kind('h:m')
SHAREDBUFFER = Kind('h:s')


# Collection of all Primitive types
PRIMITIVES = (
  BOOL,
  INT8,
  INT16,
  INT32,
  INT64,
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  FLOAT,
  DOUBLE,
  STRING,
  HANDLE,
  DCPIPE,
  DPPIPE,
  MSGPIPE,
  SHAREDBUFFER
)


class NamedValue(object):
  def __init__(self, module, parent_kind, name):
    self.module = module
    self.namespace = module.namespace
    self.parent_kind = parent_kind
    self.name = name
    self.imported_from = None

  def GetSpec(self):
    return (self.namespace + '.' +
        (self.parent_kind and (self.parent_kind.name + '.') or "") +
        self.name)


class EnumValue(NamedValue):
  def __init__(self, module, enum, field):
    NamedValue.__init__(self, module, enum.parent_kind, field.name)
    self.enum_name = enum.name


class Constant(object):
  def __init__(self, name=None, kind=None, value=None):
    self.name = name
    self.kind = kind
    self.value = value


class Field(object):
  def __init__(self, name=None, kind=None, ordinal=None, default=None):
    self.name = name
    self.kind = kind
    self.ordinal = ordinal
    self.default = default


class Struct(Kind):
  def __init__(self, name=None, module=None):
    self.name = name
    self.module = module
    self.imported_from = None
    if name != None:
      spec = 'x:' + name
    else:
      spec = None
    Kind.__init__(self, spec)
    self.fields = []

  def AddField(self, name, kind, ordinal=None, default=None):
    field = Field(name, kind, ordinal, default)
    self.fields.append(field)
    return field


class Array(Kind):
  def __init__(self, kind=None):
    self.kind = kind
    if kind != None:
      Kind.__init__(self, 'a:' + kind.spec)
    else:
      Kind.__init__(self)


class InterfaceRequest(Kind):
  def __init__(self, kind=None):
    self.kind = kind
    if kind != None:
      Kind.__init__(self, 'r:' + kind.spec)
    else:
      Kind.__init__(self)


class Parameter(object):
  def __init__(self, name=None, kind=None, ordinal=None, default=None):
    self.name = name
    self.ordinal = ordinal
    self.kind = kind
    self.default = default


class Method(object):
  def __init__(self, interface, name, ordinal=None):
    self.interface = interface
    self.name = name
    self.ordinal = ordinal
    self.parameters = []
    self.response_parameters = None

  def AddParameter(self, name, kind, ordinal=None, default=None):
    parameter = Parameter(name, kind, ordinal, default)
    self.parameters.append(parameter)
    return parameter

  def AddResponseParameter(self, name, kind, ordinal=None, default=None):
    if self.response_parameters == None:
      self.response_parameters = []
    parameter = Parameter(name, kind, ordinal, default)
    self.response_parameters.append(parameter)
    return parameter


class Interface(Kind):
  def __init__(self, name=None, client=None, module=None):
    self.module = module
    self.name = name
    self.imported_from = None
    if name != None:
      spec = 'x:' + name
    else:
      spec = None
    Kind.__init__(self, spec)
    self.client = client
    self.methods = []

  def AddMethod(self, name, ordinal=None):
    method = Method(self, name, ordinal=ordinal)
    self.methods.append(method)
    return method


class EnumField(object):
  def __init__(self, name=None, value=None):
    self.name = name
    self.value = value


class Enum(Kind):
  def __init__(self, name=None, module=None):
    self.module = module
    self.name = name
    self.imported_from = None
    if name != None:
      spec = 'x:' + name
    else:
      spec = None
    Kind.__init__(self, spec)
    self.fields = []


class Module(object):
  def __init__(self, name=None, namespace=None):
    self.name = name
    self.path = name
    self.namespace = namespace
    self.structs = []
    self.interfaces = []

  def AddInterface(self, name):
    interface=Interface(name, module=self);
    self.interfaces.append(interface)
    return interface

  def AddStruct(self, name):
    struct=Struct(name, module=self)
    self.structs.append(struct)
    return struct
