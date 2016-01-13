# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates java source files from a mojom.Module."""

import argparse
import os
import re

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja


GENERATOR_PREFIX = 'java'

_spec_to_java_type = {
  'b':     'boolean',
  'd':     'double',
  'f':     'float',
  'h:d:c': 'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  'h:d:p': 'org.chromium.mojo.system.DataPipe.ProducerHandle',
  'h:m':   'org.chromium.mojo.system.MessagePipeHandle',
  'h':     'org.chromium.mojo.system.UntypedHandle',
  'h:s':   'org.chromium.mojo.system.SharedBufferHandle',
  'i16':   'short',
  'i32':   'int',
  'i64':   'long',
  'i8':    'byte',
  's':     'String',
  'u16':   'short',
  'u32':   'int',
  'u64':   'long',
  'u8':    'byte',
}


def NameToComponent(name):
  # insert '_' between anything and a Title name (e.g, HTTPEntry2FooBar ->
  # HTTP_Entry2_FooBar)
  name = re.sub('([^_])([A-Z][^A-Z_]+)', r'\1_\2', name)
  # insert '_' between non upper and start of upper blocks (e.g.,
  # HTTP_Entry2_FooBar -> HTTP_Entry2_Foo_Bar)
  name = re.sub('([^A-Z_])([A-Z])', r'\1_\2', name)
  return [x.lower() for x in name.split('_')]

def CapitalizeFirst(string):
  return string[0].upper() + string[1:]

def UpperCamelCase(name):
  return ''.join([CapitalizeFirst(x) for x in NameToComponent(name)])

def CamelCase(name):
  uccc = UpperCamelCase(name)
  return uccc[0].lower() + uccc[1:]

def ConstantStyle(name):
  components = NameToComponent(name)
  if components[0] == 'k':
    components = components[1:]
  return '_'.join([x.upper() for x in components])

def GetNameForElement(element):
  if (isinstance(element, mojom.Enum) or
      isinstance(element, mojom.Interface) or
      isinstance(element, mojom.Struct)):
    return UpperCamelCase(element.name)
  if (isinstance(element, mojom.Method) or
      isinstance(element, mojom.Parameter) or
      isinstance(element, mojom.Field)):
    return CamelCase(element.name)
  if isinstance(element,  mojom.EnumValue):
    return (UpperCamelCase(element.enum_name) + '.' +
            ConstantStyle(element.name))
  if (isinstance(element, mojom.NamedValue) or
      isinstance(element, mojom.Constant)):
    return ConstantStyle(element.name)
  raise Exception("Unexpected element: " % element)

def ParseStringAttribute(attribute):
  assert isinstance(attribute, basestring)
  return attribute

def GetPackage(module):
  if 'JavaPackage' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaPackage'])
  # Default package.
  return "org.chromium.mojom." + module.namespace

def GetNameForKind(kind):
  def _GetNameHierachy(kind):
    hierachy = []
    if kind.parent_kind:
      hierachy = _GetNameHierachy(kind.parent_kind)
    hierachy.append(kind.name)
    return hierachy

  elements = [GetPackage(kind.module)]
  elements += _GetNameHierachy(kind)
  return '.'.join(elements)

def GetJavaType(kind):
  if isinstance(kind, (mojom.Struct, mojom.Interface)):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "%s[]" % GetJavaType(kind.kind)
  if isinstance(kind, mojom.Enum):
    return "int"
  return _spec_to_java_type[kind.spec]

def ExpressionToText(token):
  def _TranslateNamedValue(named_value):
    entity_name = GetNameForElement(named_value)
    if named_value.parent_kind:
      return GetJavaType(named_value.parent_kind) + '.' + entity_name
    # Handle the case where named_value is a module level constant:
    if not isinstance(named_value, mojom.EnumValue):
      entity_name = (GetConstantsMainEntityName(named_value.module) + '.' +
                      entity_name)
    return GetPackage(named_value.module) + '.' + entity_name

  if isinstance(token, mojom.NamedValue):
    return _TranslateNamedValue(token)
  # Add Long suffix to all number literals.
  if re.match('^[0-9]+$', token):
    return token + 'L'
  return token

def GetConstantsMainEntityName(module):
  if 'JavaConstantsClassName' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaConstantsClassName'])
  # This constructs the name of the embedding classes for module level constants
  # by extracting the mojom's filename and prepending it to Constants.
  return (UpperCamelCase(module.path.split('/')[-1].rsplit('.', 1)[0]) +
          'Constants')

class Generator(generator.Generator):

  java_filters = {
    "expression_to_text": ExpressionToText,
    "java_type": GetJavaType,
    "name": GetNameForElement,
  }

  def GetJinjaExports(self):
    return {
      "module": self.module,
      "package": GetPackage(self.module),
    }

  @UseJinja("java_templates/enum.java.tmpl", filters=java_filters,
            lstrip_blocks=True, trim_blocks=True)
  def GenerateEnumSource(self, enum):
    exports = self.GetJinjaExports()
    exports.update({"enum": enum})
    return exports

  @UseJinja("java_templates/constants.java.tmpl", filters=java_filters,
            lstrip_blocks=True, trim_blocks=True)
  def GenerateConstantsSource(self, module):
    exports = self.GetJinjaExports()
    exports.update({"main_entity": GetConstantsMainEntityName(module),
                    "constants": module.constants})
    return exports

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--java_output_directory", dest="java_output_directory")
    args = parser.parse_args(unparsed_args)
    if self.output_dir and args.java_output_directory:
      self.output_dir = os.path.join(args.java_output_directory,
                                     GetPackage(self.module).replace('.', '/'))
      if not os.path.exists(self.output_dir):
        try:
          os.makedirs(self.output_dir)
        except:
          # Ignore errors on directory creation.
          pass

    for enum in self.module.enums:
      self.Write(self.GenerateEnumSource(enum),
                 "%s.java" % GetNameForElement(enum))

    if self.module.constants:
      self.Write(self.GenerateConstantsSource(self.module),
                 "%s.java" % GetConstantsMainEntityName(self.module))
