# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja


_kind_to_cpp_type = {
  mojom.BOOL:         "bool",
  mojom.INT8:         "int8_t",
  mojom.UINT8:        "uint8_t",
  mojom.INT16:        "int16_t",
  mojom.UINT16:       "uint16_t",
  mojom.INT32:        "int32_t",
  mojom.UINT32:       "uint32_t",
  mojom.FLOAT:        "float",
  mojom.HANDLE:       "mojo::Handle",
  mojom.DCPIPE:       "mojo::DataPipeConsumerHandle",
  mojom.DPPIPE:       "mojo::DataPipeProducerHandle",
  mojom.MSGPIPE:      "mojo::MessagePipeHandle",
  mojom.SHAREDBUFFER: "mojo::SharedBufferHandle",
  mojom.INT64:        "int64_t",
  mojom.UINT64:       "uint64_t",
  mojom.DOUBLE:       "double",
}

def DefaultValue(field):
  if field.default:
    if isinstance(field.kind, mojom.Struct):
      assert field.default == "default"
      return "%s::New()" % GetNameForKind(field.kind)
    return ExpressionToText(field.default)
  return ""

def NamespaceToArray(namespace):
  return namespace.split('.') if namespace else []

def GetNameForKind(kind, internal = False):
  parts = []
  if kind.imported_from:
    parts.extend(NamespaceToArray(kind.imported_from["namespace"]))
  if internal:
    parts.append("internal")
  if kind.parent_kind:
    parts.append(kind.parent_kind.name)
  parts.append(kind.name)
  return "::".join(parts)

def GetCppType(kind):
  if isinstance(kind, mojom.Struct):
    return "%s_Data*" % GetNameForKind(kind, internal=True)
  if isinstance(kind, mojom.Array):
    return "mojo::internal::Array_Data<%s>*" % GetCppType(kind.kind)
  if isinstance(kind, mojom.Interface) or \
     isinstance(kind, mojom.InterfaceRequest):
    return "mojo::MessagePipeHandle"
  if isinstance(kind, mojom.Enum):
    return "int32_t"
  if kind.spec == 's':
    return "mojo::internal::String_Data*"
  return _kind_to_cpp_type[kind]

def GetCppPodType(kind):
  if kind.spec == 's':
    return "char*"
  return _kind_to_cpp_type[kind]

def GetCppArrayArgWrapperType(kind):
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Struct):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s> " % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    raise Exception("Arrays of interfaces not yet supported!")
  if isinstance(kind, mojom.InterfaceRequest):
    raise Exception("Arrays of interface requests not yet supported!")
  if kind.spec == 's':
    return "mojo::String"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppResultWrapperType(kind):
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Struct):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.InterfaceRequest):
    return "mojo::InterfaceRequest<%s>" % GetNameForKind(kind.kind)
  if kind.spec == 's':
    return "mojo::String"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppWrapperType(kind):
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Struct):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "mojo::ScopedMessagePipeHandle"
  if isinstance(kind, mojom.InterfaceRequest):
    raise Exception("InterfaceRequest fields not supported!")
  if kind.spec == 's':
    return "mojo::String"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppConstWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "%sPtr" % GetNameForKind(kind)
  if isinstance(kind, mojom.InterfaceRequest):
    return "mojo::InterfaceRequest<%s>" % GetNameForKind(kind.kind)
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if kind.spec == 's':
    return "const mojo::String&"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  if not kind in _kind_to_cpp_type:
    print "missing:", kind.spec
  return _kind_to_cpp_type[kind]

def GetCppFieldType(kind):
  if isinstance(kind, mojom.Struct):
    return ("mojo::internal::StructPointer<%s_Data>" %
        GetNameForKind(kind, internal=True))
  if isinstance(kind, mojom.Array):
    return "mojo::internal::ArrayPointer<%s>" % GetCppType(kind.kind)
  if isinstance(kind, mojom.Interface) or \
     isinstance(kind, mojom.InterfaceRequest):
    return "mojo::MessagePipeHandle"
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if kind.spec == 's':
    return "mojo::internal::StringPointer"
  return _kind_to_cpp_type[kind]

def IsStructWithHandles(struct):
  for pf in struct.packed.packed_fields:
    if generator.IsHandleKind(pf.field.kind):
      return True
  return False

def TranslateConstants(token):
  if isinstance(token, (mojom.NamedValue, mojom.EnumValue)):
    # Both variable and enum constants are constructed like:
    # Namespace::Struct::CONSTANT_NAME
    name = []
    if token.imported_from:
      name.extend(NamespaceToArray(token.namespace))
    if token.parent_kind:
      name.append(token.parent_kind.name)
    name.append(token.name)
    return "::".join(name)
  return token

def ExpressionToText(value):
  return TranslateConstants(value)

def HasCallbacks(interface):
  for method in interface.methods:
    if method.response_parameters != None:
      return True
  return False

def ShouldInlineStruct(struct):
  # TODO(darin): Base this on the size of the wrapper class.
  if len(struct.fields) > 4:
    return False
  for field in struct.fields:
    if generator.IsHandleKind(field.kind) or generator.IsObjectKind(field.kind):
      return False
  return True

_HEADER_SIZE = 8

class Generator(generator.Generator):

  cpp_filters = {
    "cpp_const_wrapper_type": GetCppConstWrapperType,
    "cpp_field_type": GetCppFieldType,
    "cpp_pod_type": GetCppPodType,
    "cpp_result_type": GetCppResultWrapperType,
    "cpp_type": GetCppType,
    "cpp_wrapper_type": GetCppWrapperType,
    "default_value": DefaultValue,
    "expression_to_text": ExpressionToText,
    "get_name_for_kind": GetNameForKind,
    "get_pad": pack.GetPad,
    "has_callbacks": HasCallbacks,
    "should_inline": ShouldInlineStruct,
    "is_enum_kind": generator.IsEnumKind,
    "is_move_only_kind": generator.IsMoveOnlyKind,
    "is_handle_kind": generator.IsHandleKind,
    "is_interface_kind": generator.IsInterfaceKind,
    "is_interface_request_kind": generator.IsInterfaceRequestKind,
    "is_object_kind": generator.IsObjectKind,
    "is_string_kind": generator.IsStringKind,
    "is_struct_with_handles": IsStructWithHandles,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "struct_from_method": generator.GetStructFromMethod,
    "response_struct_from_method": generator.GetResponseStructFromMethod,
    "stylize_method": generator.StudlyCapsToCamel,
  }

  def GetJinjaExports(self):
    return {
      "module": self.module,
      "namespace": self.module.namespace,
      "namespaces_as_array": NamespaceToArray(self.module.namespace),
      "imports": self.module.imports,
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module.h.tmpl", filters=cpp_filters)
  def GenerateModuleHeader(self):
    return self.GetJinjaExports()

  @UseJinja("cpp_templates/module-internal.h.tmpl", filters=cpp_filters)
  def GenerateModuleInternalHeader(self):
    return self.GetJinjaExports()

  @UseJinja("cpp_templates/module.cc.tmpl", filters=cpp_filters)
  def GenerateModuleSource(self):
    return self.GetJinjaExports()

  def GenerateFiles(self, args):
    self.Write(self.GenerateModuleHeader(), "%s.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s-internal.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.cc" % self.module.name)
