#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from model import Model
import ppapi_generator
from schema_loader import SchemaLoader


def _LoadNamespace(filename):
  filename = os.path.join(os.path.dirname(__file__), filename)
  schema_loader = SchemaLoader(
      os.path.dirname(os.path.relpath(os.path.normpath(filename),
                                      os.path.dirname(filename))),
      os.path.dirname(filename))
  schema = os.path.normpath(filename)
  api_def = schema_loader.LoadSchema(os.path.split(schema)[1])[0]

  api_model = Model()
  relpath = os.path.relpath(os.path.normpath(filename),
                            os.path.dirname(filename))
  namespace = api_model.AddNamespace(api_def, relpath)
  generator = ppapi_generator._PpapiGeneratorBase(namespace)
  return namespace, generator


class PpapiGeneratorIdlStructsTest(unittest.TestCase):
  def setUp(self):
    self.namespace, self.generator = _LoadNamespace(
        os.path.join('test', 'idl_pepper.idl'))

  def testTypesOrder(self):
    typename_to_index = dict(
        (type_.name, i) for i, type_ in enumerate(self.generator._types))
    self.assertLess(typename_to_index['MyType1'], typename_to_index['MyType2'])
    self.assertLess(typename_to_index['MyType1'], typename_to_index['MyType3'])
    self.assertLess(typename_to_index['MyType1'], typename_to_index['MyType4'])
    self.assertLess(typename_to_index['MyType1'], typename_to_index['MyType5'])
    self.assertLess(typename_to_index['MyType3'], typename_to_index['MyType5'])
    self.assertLess(typename_to_index['MyType4'], typename_to_index['MyType5'])
    self.assertLess(typename_to_index['MyType5'], typename_to_index['MyType6'])

  def testNeedsArray(self):
    self.assertFalse(self.generator.NeedsArray(self.namespace.types['MyType1']))
    self.assertTrue(self.generator.NeedsArray(self.namespace.types['MyType2']))
    self.assertTrue(self.generator.NeedsArray(self.namespace.types['MyType3']))
    self.assertFalse(self.generator.NeedsArray(self.namespace.types['MyType4']))
    self.assertFalse(self.generator.NeedsArray(self.namespace.types['MyType5']))
    self.assertTrue(self.generator.NeedsArray(self.namespace.types['EnumType']))

  def testNeedsOptional(self):
    self.assertFalse(
        self.generator.NeedsOptional(self.namespace.types['MyType1']))
    self.assertFalse(
        self.generator.NeedsOptional(self.namespace.types['MyType2']))
    self.assertTrue(
        self.generator.NeedsOptional(self.namespace.types['MyType3']))
    self.assertTrue(
        self.generator.NeedsOptional(self.namespace.types['MyType4']))
    self.assertFalse(
        self.generator.NeedsOptional(self.namespace.types['MyType5']))
    self.assertTrue(
        self.generator.NeedsOptional(self.namespace.types['EnumType']))

  def testNeedsOptionalArray(self):
    self.assertFalse(
        self.generator.NeedsOptionalArray(self.namespace.types['MyType1']))
    self.assertTrue(
        self.generator.NeedsOptionalArray(self.namespace.types['MyType2']))
    self.assertFalse(
        self.generator.NeedsOptionalArray(self.namespace.types['MyType3']))
    self.assertFalse(
        self.generator.NeedsOptionalArray(self.namespace.types['MyType4']))
    self.assertFalse(
        self.generator.NeedsOptionalArray(self.namespace.types['MyType5']))
    self.assertTrue(
        self.generator.NeedsOptionalArray(self.namespace.types['EnumType']))

  def testFormatParamTypePrimitive(self):
    self.assertEqual('int32_t', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['int_single']))
    self.assertEqual('PP_Int32_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['int_array']))
    self.assertEqual('PP_Optional_Int32', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_int']))
    self.assertEqual('PP_Optional_Int32_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_int_array']))
    self.assertEqual('double_t', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['double_single']))
    self.assertEqual('PP_Double_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['double_array']))
    self.assertEqual('PP_Optional_Double', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_double']))
    self.assertEqual('PP_Optional_Double_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_double_array']))
    self.assertEqual('PP_Var', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['string']))
    self.assertEqual('PP_String_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['string_array']))
    self.assertEqual('PP_Var', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_string']))
    self.assertEqual('PP_Optional_String_Array', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['optional_string_array']))

  def testFormatParamTypeStruct(self):
    self.assertEqual('PP_IdlPepper_MyType0', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['struct_single']))
    self.assertEqual(
        'PP_IdlPepper_MyType0_Array', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties['struct_array']))
    self.assertEqual(
        'PP_IdlPepper_Optional_MyType0', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties['optional_struct']))
    self.assertEqual(
        'PP_IdlPepper_Optional_MyType0_Array', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties[
                'optional_struct_array']))

  def testFormatParamTypeEnum(self):
    self.assertEqual('PP_IdlPepper_EnumType', self.generator.FormatParamType(
        self.namespace.types['MyType'].properties['enum_single']))
    self.assertEqual(
        'PP_IdlPepper_EnumType_Array', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties['enum_array']))
    self.assertEqual(
        'PP_IdlPepper_Optional_EnumType', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties['optional_enum']))
    self.assertEqual(
        'PP_IdlPepper_Optional_EnumType_Array', self.generator.FormatParamType(
            self.namespace.types['MyType'].properties['optional_enum_array']))

  def testEnumValueName(self):
    enum_type = self.namespace.types['EnumType']
    self.assertEqual('PP_IDLPEPPER_ENUMTYPE_NAME1',
                     self.generator.EnumValueName(enum_type.enum_values[0],
                                                  enum_type))
    self.assertEqual('PP_IDLPEPPER_ENUMTYPE_NAME2',
                     self.generator.EnumValueName(enum_type.enum_values[1],
                                                  enum_type))
    enum_type = self.namespace.types['AnotherEnumType']
    self.assertEqual('PP_IDLPEPPER_ANOTHERENUMTYPE_NAME1',
                     self.generator.EnumValueName(enum_type.enum_values[0],
                                                  enum_type))
    self.assertEqual('PP_IDLPEPPER_ANOTHERENUMTYPE_NAME2',
                     self.generator.EnumValueName(enum_type.enum_values[1],
                                                  enum_type))

  def testReturnTypes(self):
    self.assertEqual('void', self.generator.GetFunctionReturnType(
        self.namespace.functions['function1']))
    self.assertEqual('void', self.generator.GetFunctionReturnType(
        self.namespace.functions['function2']))
    self.assertEqual('int32_t', self.generator.GetFunctionReturnType(
        self.namespace.functions['function3']))
    self.assertEqual('int32_t', self.generator.GetFunctionReturnType(
        self.namespace.functions['function4']))
    self.assertEqual('int32_t', self.generator.GetFunctionReturnType(
        self.namespace.functions['function5']))
    self.assertEqual('int32_t', self.generator.GetFunctionReturnType(
        self.namespace.functions['function6']))

  def testHasOutArray(self):
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function1']))
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function2']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function3']))
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function4']))
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function5']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function6']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function7']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function8']))
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function9']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function10']))
    self.assertTrue(self.generator.HasArrayOuts(
        self.namespace.functions['function11']))
    self.assertFalse(self.generator.HasArrayOuts(
        self.namespace.functions['function12']))

if __name__ == '__main__':
  unittest.main()
