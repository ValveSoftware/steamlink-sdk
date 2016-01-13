# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import datetime
import os.path
import sys

import code
import cpp_util
import model

try:
  import jinja2
except ImportError:
  sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..',
                               'third_party'))
  import jinja2


class _PpapiGeneratorBase(object):
  """A base class for ppapi generators.

  Implementations should set TEMPLATE_NAME to a string containing the name of
  the template file without its extension. The template will be rendered with
  the following symbols available:
    name: A string containing the name of the namespace.
    enums: A list of enums within the namespace.
    types: A list of types within the namespace, sorted such that no element
        depends on an earlier element.
    events: A dict of events within the namespace.
    functions: A dict of functions within the namespace.
    year: An int containing the current year.
    source_file: The name of the input file.
  """

  def __init__(self, namespace):
    self._namespace = namespace
    self._required_types = {}
    self._array_types = set()
    self._optional_types = set()
    self._optional_array_types = set()
    self._dependencies = collections.OrderedDict()
    self._types = []
    self._enums = []

    self.jinja_environment = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.join(os.path.dirname(__file__),
                                                    'templates', 'ppapi')))
    self._SetupFilters()
    self._ResolveTypeDependencies()

  def _SetupFilters(self):
    self.jinja_environment.filters.update({
      'ppapi_type': self.ToPpapiType,
      'classname': cpp_util.Classname,
      'enum_value': self.EnumValueName,
      'return_type': self.GetFunctionReturnType,
      'format_param_type': self.FormatParamType,
      'needs_optional': self.NeedsOptional,
      'needs_array': self.NeedsArray,
      'needs_optional_array': self.NeedsOptionalArray,
      'has_array_outs': self.HasArrayOuts,
    })

  def Render(self, template_name, values):
    generated_code = code.Code()
    template = self.jinja_environment.get_template(
        '%s.template' % template_name)
    generated_code.Append(template.render(values))
    return generated_code

  def Generate(self):
    """Generates a Code object for a single namespace."""
    return self.Render(self.TEMPLATE_NAME, {
      'name': self._namespace.name,
      'enums': self._enums,
      'types': self._types,
      'events': self._namespace.events,
      'functions': self._namespace.functions,
      # TODO(sammc): Don't change years when regenerating existing output files.
      'year': datetime.date.today().year,
      'source_file': self._namespace.source_file,
    })

  def _ResolveTypeDependencies(self):
    """Calculates the transitive closure of the types in _required_types.

    Returns a tuple containing the list of struct types and the list of enum
    types. The list of struct types is ordered such that no type depends on a
    type later in the list.

    """
    if self._namespace.functions:
      for function in self._namespace.functions.itervalues():
        self._FindFunctionDependencies(function)

    if self._namespace.events:
      for event in self._namespace.events.itervalues():
        self._FindFunctionDependencies(event)
    resolved_types = set()
    while resolved_types < set(self._required_types):
      for typename in sorted(set(self._required_types) - resolved_types):
        type_ = self._required_types[typename]
        self._dependencies.setdefault(typename, set())
        for member in type_.properties.itervalues():
          self._RegisterDependency(member, self._NameComponents(type_))
        resolved_types.add(typename)
    while self._dependencies:
      for name, deps in self._dependencies.items():
        if not deps:
          if (self._required_types[name].property_type ==
              model.PropertyType.ENUM):
            self._enums.append(self._required_types[name])
          else:
            self._types.append(self._required_types[name])
          for deps in self._dependencies.itervalues():
            deps.discard(name)
          del self._dependencies[name]
          break
      else:
        raise ValueError('Circular dependency %s' % self._dependencies)

  def _FindFunctionDependencies(self, function):
    for param in function.params:
      self._RegisterDependency(param, None)
    if function.callback:
      for param in function.callback.params:
        self._RegisterDependency(param, None)
    if function.returns:
      self._RegisterTypeDependency(function.returns, None, False, False)

  def _RegisterDependency(self, member, depender):
    self._RegisterTypeDependency(member.type_, depender, member.optional, False)

  def _RegisterTypeDependency(self, type_, depender, optional, array):
    if type_.property_type == model.PropertyType.ARRAY:
      self._RegisterTypeDependency(type_.item_type, depender, optional, True)
    elif type_.property_type == model.PropertyType.REF:
      self._RegisterTypeDependency(self._namespace.types[type_.ref_type],
                                   depender, optional, array)
    elif type_.property_type in (model.PropertyType.OBJECT,
                                 model.PropertyType.ENUM):
      name_components = self._NameComponents(type_)
      self._required_types[name_components] = type_
      if depender:
        self._dependencies.setdefault(depender, set()).add(
            name_components)
      if array:
        self._array_types.add(name_components)
        if optional:
          self._optional_array_types.add(name_components)
      elif optional:
        self._optional_types.add(name_components)

  @staticmethod
  def _NameComponents(entity):
    """Returns a tuple of the fully-qualified name of an entity."""
    names = []
    while entity:
      if (not isinstance(entity, model.Type) or
          entity.property_type != model.PropertyType.ARRAY):
        names.append(entity.name)
      entity = entity.parent
    return tuple(reversed(names[:-1]))

  def ToPpapiType(self, type_, array=False, optional=False):
    """Returns a string containing the name of the Pepper C type for |type_|.

    If array is True, returns the name of an array of |type_|. If optional is
    True, returns the name of an optional |type_|. If both array and optional
    are True, returns the name of an optional array of |type_|.
    """
    if isinstance(type_, model.Function) or type_.property_type in (
        model.PropertyType.OBJECT, model.PropertyType.ENUM):
      return self._FormatPpapiTypeName(
          array, optional, '_'.join(
              cpp_util.Classname(s) for s in self._NameComponents(type_)),
              namespace=cpp_util.Classname(self._namespace.name))
    elif type_.property_type == model.PropertyType.REF:
      return self.ToPpapiType(self._namespace.types[type_.ref_type],
                              optional=optional, array=array)
    elif type_.property_type == model.PropertyType.ARRAY:
      return self.ToPpapiType(type_.item_type, array=True,
                              optional=optional)
    elif type_.property_type == model.PropertyType.STRING and not array:
      return 'PP_Var'
    elif array or optional:
      if type_.property_type in self._PPAPI_COMPOUND_PRIMITIVE_TYPE_MAP:
        return self._FormatPpapiTypeName(
            array, optional,
            self._PPAPI_COMPOUND_PRIMITIVE_TYPE_MAP[type_.property_type], '')
    return self._PPAPI_PRIMITIVE_TYPE_MAP.get(type_.property_type, 'PP_Var')

  _PPAPI_PRIMITIVE_TYPE_MAP = {
    model.PropertyType.BOOLEAN: 'PP_Bool',
    model.PropertyType.DOUBLE: 'double_t',
    model.PropertyType.INT64: 'int64_t',
    model.PropertyType.INTEGER: 'int32_t',
  }
  _PPAPI_COMPOUND_PRIMITIVE_TYPE_MAP = {
    model.PropertyType.BOOLEAN: 'Bool',
    model.PropertyType.DOUBLE: 'Double',
    model.PropertyType.INT64: 'Int64',
    model.PropertyType.INTEGER: 'Int32',
    model.PropertyType.STRING: 'String',
  }

  @staticmethod
  def _FormatPpapiTypeName(array, optional, name, namespace=''):
    if namespace:
      namespace = '%s_' % namespace
    if array:
      if optional:
        return 'PP_%sOptional_%s_Array' % (namespace, name)
      return 'PP_%s%s_Array' % (namespace, name)
    if optional:
      return 'PP_%sOptional_%s' % (namespace, name)
    return 'PP_%s%s' % (namespace, name)

  def NeedsOptional(self, type_):
    """Returns True if an optional |type_| is required."""
    return self._NameComponents(type_) in self._optional_types

  def NeedsArray(self, type_):
    """Returns True if an array of |type_| is required."""
    return self._NameComponents(type_) in self._array_types

  def NeedsOptionalArray(self, type_):
    """Returns True if an optional array of |type_| is required."""
    return self._NameComponents(type_) in self._optional_array_types

  def FormatParamType(self, param):
    """Formats the type of a parameter or property."""
    return self.ToPpapiType(param.type_, optional=param.optional)

  @staticmethod
  def GetFunctionReturnType(function):
    return 'int32_t' if function.callback or function.returns else 'void'

  def EnumValueName(self, enum_value, enum_type):
    """Returns a string containing the name for an enum value."""
    return '%s_%s' % (self.ToPpapiType(enum_type).upper(),
                      enum_value.name.upper())

  def _ResolveType(self, type_):
    if type_.property_type == model.PropertyType.REF:
      return self._ResolveType(self._namespace.types[type_.ref_type])
    if type_.property_type == model.PropertyType.ARRAY:
      return self._ResolveType(type_.item_type)
    return type_

  def _IsOrContainsArray(self, type_):
    if type_.property_type == model.PropertyType.ARRAY:
      return True
    type_ = self._ResolveType(type_)
    if type_.property_type == model.PropertyType.OBJECT:
      return any(self._IsOrContainsArray(param.type_)
                 for param in type_.properties.itervalues())
    return False

  def HasArrayOuts(self, function):
    """Returns True if the function produces any arrays as outputs.

    This includes arrays that are properties of other objects.
    """
    if function.callback:
      for param in function.callback.params:
        if self._IsOrContainsArray(param.type_):
          return True
    return function.returns and self._IsOrContainsArray(function.returns)


class _IdlGenerator(_PpapiGeneratorBase):
  TEMPLATE_NAME = 'idl'


class _GeneratorWrapper(object):
  def __init__(self, generator_factory):
    self._generator_factory = generator_factory

  def Generate(self, namespace):
    return self._generator_factory(namespace).Generate()


class PpapiGenerator(object):
  def __init__(self):
    self.idl_generator = _GeneratorWrapper(_IdlGenerator)
