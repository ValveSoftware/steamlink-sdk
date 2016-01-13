# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(vtl): "data" is a pretty vague name. Rename it?

import copy

import module as mojom

# This module provides a mechanism to turn mojom Modules to dictionaries and
# back again. This can be used to persist a mojom Module created progromatically
# or to read a dictionary from code or a file.
# Example:
# test_dict = {
#   'name': 'test',
#   'namespace': 'testspace',
#   'structs': [{
#     'name': 'teststruct',
#     'fields': [
#       {'name': 'testfield1', 'kind': 'i32'},
#       {'name': 'testfield2', 'kind': 'a:i32', 'ordinal': 42}]}],
#   'interfaces': [{
#     'name': 'Server',
#     'methods': [{
#       'name': 'Foo',
#       'parameters': [{
#         'name': 'foo', 'kind': 'i32'},
#         {'name': 'bar', 'kind': 'a:x:teststruct'}],
#     'ordinal': 42}]}]
# }
# test_module = data.ModuleFromData(test_dict)

# Used to create a subclass of str that supports sorting by index, to make
# pretty printing maintain the order.
def istr(index, string):
  class IndexedString(str):
    def __lt__(self, other):
      return self.__index__ < other.__index__

  istr = IndexedString(string)
  istr.__index__ = index
  return istr

def LookupKind(kinds, spec, scope):
  """Tries to find which Kind a spec refers to, given the scope in which its
  referenced. Starts checking from the narrowest scope to most general. For
  example, given a struct field like
    Foo.Bar x;
  Foo.Bar could refer to the type 'Bar' in the 'Foo' namespace, or an inner
  type 'Bar' in the struct 'Foo' in the current namespace.

  |scope| is a tuple that looks like (namespace, struct/interface), referring
  to the location where the type is referenced."""
  if spec.startswith('x:'):
    name = spec[2:]
    for i in xrange(len(scope), -1, -1):
      test_spec = 'x:'
      if i > 0:
        test_spec += '.'.join(scope[:i]) + '.'
      test_spec += name
      kind = kinds.get(test_spec)
      if kind:
        return kind

  return kinds.get(spec)

def LookupValue(values, name, scope):
  """Like LookupKind, but for constant values."""
  for i in xrange(len(scope), -1, -1):
    if i > 0:
      test_spec = '.'.join(scope[:i]) + '.'
    test_spec += name
    value = values.get(test_spec)
    if value:
      return value

  return values.get(name)

def FixupExpression(module, value, scope):
  """Translates an IDENTIFIER into a structured Value object."""
  if isinstance(value, tuple) and value[0] == 'IDENTIFIER':
    result = LookupValue(module.values, value[1], scope)
    if result:
      return result
  return value

def KindToData(kind):
  return kind.spec

def KindFromData(kinds, data, scope):
  kind = LookupKind(kinds, data, scope)
  if kind:
    return kind
  if data.startswith('a:'):
    kind = mojom.Array()
    kind.kind = KindFromData(kinds, data[2:], scope)
  elif data.startswith('r:'):
    kind = mojom.InterfaceRequest()
    kind.kind = KindFromData(kinds, data[2:], scope)
  else:
    kind = mojom.Kind()
  kind.spec = data
  kinds[data] = kind
  return kind

def KindFromImport(original_kind, imported_from):
  """Used with 'import module' - clones the kind imported from the given
  module's namespace. Only used with Structs, Interfaces and Enums."""
  kind = copy.deepcopy(original_kind)
  kind.imported_from = imported_from
  return kind

def ImportFromData(module, data):
  import_module = data['module']

  import_item = {}
  import_item['module_name'] = import_module.name
  import_item['namespace'] = import_module.namespace
  import_item['module'] = import_module

  # Copy the struct kinds from our imports into the current module.
  for kind in import_module.kinds.itervalues():
    if (isinstance(kind, (mojom.Struct, mojom.Enum, mojom.Interface)) and
        kind.imported_from is None):
      kind = KindFromImport(kind, import_item)
      module.kinds[kind.spec] = kind
  # Ditto for values.
  for value in import_module.values.itervalues():
    if value.imported_from is None:
      value = copy.deepcopy(value)
      value.imported_from = import_item
      module.values[value.GetSpec()] = value

  return import_item

def StructToData(struct):
  return {
    istr(0, 'name'): struct.name,
    istr(1, 'fields'): map(FieldToData, struct.fields)
  }

def StructFromData(module, data):
  struct = mojom.Struct(module=module)
  struct.name = data['name']
  struct.attributes = data['attributes']
  struct.spec = 'x:' + module.namespace + '.' + struct.name
  module.kinds[struct.spec] = struct
  struct.enums = map(lambda enum:
      EnumFromData(module, enum, struct), data['enums'])
  struct.constants = map(lambda constant:
      ConstantFromData(module, constant, struct), data['constants'])
  # Stash fields data here temporarily.
  struct.fields_data = data['fields']
  return struct

def FieldToData(field):
  data = {
    istr(0, 'name'): field.name,
    istr(1, 'kind'): KindToData(field.kind)
  }
  if field.ordinal != None:
    data[istr(2, 'ordinal')] = field.ordinal
  if field.default != None:
    data[istr(3, 'default')] = field.default
  return data

def FieldFromData(module, data, struct):
  field = mojom.Field()
  field.name = data['name']
  field.kind = KindFromData(
      module.kinds, data['kind'], (module.namespace, struct.name))
  field.ordinal = data.get('ordinal')
  field.default = FixupExpression(
      module, data.get('default'), (module.namespace, struct.name))
  return field

def ParameterToData(parameter):
  data = {
    istr(0, 'name'): parameter.name,
    istr(1, 'kind'): parameter.kind.spec
  }
  if parameter.ordinal != None:
    data[istr(2, 'ordinal')] = parameter.ordinal
  if parameter.default != None:
    data[istr(3, 'default')] = parameter.default
  return data

def ParameterFromData(module, data, interface):
  parameter = mojom.Parameter()
  parameter.name = data['name']
  parameter.kind = KindFromData(
      module.kinds, data['kind'], (module.namespace, interface.name))
  parameter.ordinal = data.get('ordinal')
  parameter.default = data.get('default')
  return parameter

def MethodToData(method):
  data = {
    istr(0, 'name'):       method.name,
    istr(1, 'parameters'): map(ParameterToData, method.parameters)
  }
  if method.ordinal != None:
    data[istr(2, 'ordinal')] = method.ordinal
  if method.response_parameters != None:
    data[istr(3, 'response_parameters')] = map(
        ParameterToData, method.response_parameters)
  return data

def MethodFromData(module, data, interface):
  method = mojom.Method(interface, data['name'], ordinal=data.get('ordinal'))
  method.default = data.get('default')
  method.parameters = map(lambda parameter:
      ParameterFromData(module, parameter, interface), data['parameters'])
  if data.has_key('response_parameters'):
    method.response_parameters = map(
        lambda parameter: ParameterFromData(module, parameter, interface),
                          data['response_parameters'])
  return method

def InterfaceToData(interface):
  return {
    istr(0, 'name'):    interface.name,
    istr(1, 'client'):  interface.client,
    istr(2, 'methods'): map(MethodToData, interface.methods)
  }

def InterfaceFromData(module, data):
  interface = mojom.Interface(module=module)
  interface.name = data['name']
  interface.spec = 'x:' + module.namespace + '.' + interface.name
  interface.client = data['client'] if data.has_key('client') else None
  module.kinds[interface.spec] = interface
  interface.enums = map(lambda enum:
      EnumFromData(module, enum, interface), data['enums'])
  interface.constants = map(lambda constant:
      ConstantFromData(module, constant, interface), data['constants'])
  # Stash methods data here temporarily.
  interface.methods_data = data['methods']
  return interface

def EnumFieldFromData(module, enum, data, parent_kind):
  field = mojom.EnumField()
  field.name = data['name']
  # TODO(mpcomplete): FixupExpression should be done in the second pass,
  # so constants and enums can refer to each other.
  # TODO(mpcomplete): But then, what if constants are initialized to an enum? Or
  # vice versa?
  if parent_kind:
    field.value = FixupExpression(
        module, data['value'], (module.namespace, parent_kind.name))
  else:
    field.value = FixupExpression(
        module, data['value'], (module.namespace, ))
  value = mojom.EnumValue(module, enum, field)
  module.values[value.GetSpec()] = value
  return field

def EnumFromData(module, data, parent_kind):
  enum = mojom.Enum(module=module)
  enum.name = data['name']
  name = enum.name
  if parent_kind:
    name = parent_kind.name + '.' + name
  enum.spec = 'x:%s.%s' % (module.namespace, name)
  enum.parent_kind = parent_kind

  enum.fields = map(
      lambda field: EnumFieldFromData(module, enum, field, parent_kind),
      data['fields'])
  module.kinds[enum.spec] = enum
  return enum

def ConstantFromData(module, data, parent_kind):
  constant = mojom.Constant()
  constant.name = data['name']
  if parent_kind:
    scope = (module.namespace, parent_kind.name)
  else:
    scope = (module.namespace, )
  # TODO(mpcomplete): maybe we should only support POD kinds.
  constant.kind = KindFromData(module.kinds, data['kind'], scope)
  constant.value = FixupExpression(module, data.get('value'), scope)

  value = mojom.NamedValue(module, parent_kind, constant.name)
  module.values[value.GetSpec()] = value
  return constant

def ModuleToData(module):
  return {
    istr(0, 'name'):       module.name,
    istr(1, 'namespace'):  module.namespace,
    istr(2, 'structs'):    map(StructToData, module.structs),
    istr(3, 'interfaces'): map(InterfaceToData, module.interfaces)
  }

def ModuleFromData(data):
  module = mojom.Module()
  module.kinds = {}
  for kind in mojom.PRIMITIVES:
    module.kinds[kind.spec] = kind

  module.values = {}

  module.name = data['name']
  module.namespace = data['namespace']
  module.attributes = data['attributes']
  # Imports must come first, because they add to module.kinds which is used
  # by by the others.
  module.imports = map(
      lambda import_data: ImportFromData(module, import_data),
      data['imports'])

  # First pass collects kinds.
  module.enums = map(
      lambda enum: EnumFromData(module, enum, None), data['enums'])
  module.structs = map(
      lambda struct: StructFromData(module, struct), data['structs'])
  module.interfaces = map(
      lambda interface: InterfaceFromData(module, interface),
      data['interfaces'])
  module.constants = map(
      lambda constant: ConstantFromData(module, constant, None),
      data['constants'])

  # Second pass expands fields and methods. This allows fields and parameters
  # to refer to kinds defined anywhere in the mojom.
  for struct in module.structs:
    struct.fields = map(lambda field:
        FieldFromData(module, field, struct), struct.fields_data)
    del struct.fields_data
  for interface in module.interfaces:
    interface.methods = map(lambda method:
        MethodFromData(module, method, interface), interface.methods_data)
    del interface.methods_data

  return module

def OrderedModuleFromData(data):
  module = ModuleFromData(data)
  next_interface_ordinal = 0
  for interface in module.interfaces:
    next_ordinal = 0
    for method in interface.methods:
      if method.ordinal is None:
        method.ordinal = next_ordinal
      next_ordinal = method.ordinal + 1
  return module
