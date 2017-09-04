# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja


_kind_to_cpp_type = {
  mojom.BOOL:                  "bool",
  mojom.INT8:                  "int8_t",
  mojom.UINT8:                 "uint8_t",
  mojom.INT16:                 "int16_t",
  mojom.UINT16:                "uint16_t",
  mojom.INT32:                 "int32_t",
  mojom.UINT32:                "uint32_t",
  mojom.FLOAT:                 "float",
  mojom.INT64:                 "int64_t",
  mojom.UINT64:                "uint64_t",
  mojom.DOUBLE:                "double",
}

_kind_to_cpp_literal_suffix = {
  mojom.UINT8:        "U",
  mojom.UINT16:       "U",
  mojom.UINT32:       "U",
  mojom.FLOAT:        "f",
  mojom.UINT64:       "ULL",
}

# TODO(rockot): Get rid of these globals. This requires some refactoring of the
# generator library code so that filters can use the generator as context.
_current_typemap = {}
_for_blink = False
_use_new_wrapper_types = False
# TODO(rockot, yzshen): The variant handling is kind of a hack currently. Make
# it right.
_variant = None


class _NameFormatter(object):
  """A formatter for the names of kinds or values."""

  def __init__(self, token, variant):
    self._token = token
    self._variant = variant

  def Format(self, separator, prefixed=False, internal=False,
             include_variant=False, add_same_module_namespaces=False,
             flatten_nested_kind=False):
    """Formats the name according to the given configuration.

    Args:
      separator: Separator between different parts of the name.
      prefixed: Whether a leading separator should be added.
      internal: Returns the name in the "internal" namespace.
      include_variant: Whether to include variant as namespace. If |internal| is
          True, then this flag is ignored and variant is not included.
      add_same_module_namespaces: Includes all namespaces even if the token is
          from the same module as the current mojom file.
      flatten_nested_kind: It is allowed to define enums inside structs and
          interfaces. If this flag is set to True, this method concatenates the
          parent kind and the nested kind with '_', instead of treating the
          parent kind as a scope."""

    parts = []
    if self._ShouldIncludeNamespace(add_same_module_namespaces):
      if prefixed:
        parts.append("")
      parts.extend(self._GetNamespace())
      if include_variant and self._variant and not internal:
        parts.append(self._variant)
    parts.extend(self._GetName(internal, flatten_nested_kind))
    return separator.join(parts)

  def FormatForCpp(self, add_same_module_namespaces=False, internal=False,
                   flatten_nested_kind=False):
    return self.Format(
        "::", prefixed=True,
        add_same_module_namespaces=add_same_module_namespaces,
        internal=internal, include_variant=True,
        flatten_nested_kind=flatten_nested_kind)

  def FormatForMojom(self):
    return self.Format(".", add_same_module_namespaces=True)

  def _MapKindName(self, token, internal):
    if not internal:
      return token.name
    if (mojom.IsStructKind(token) or mojom.IsUnionKind(token) or
        mojom.IsEnumKind(token)):
      return token.name + "_Data"
    return token.name

  def _GetName(self, internal, flatten_nested_kind):
    if isinstance(self._token, mojom.EnumValue):
      name_parts = _NameFormatter(self._token.enum, self._variant)._GetName(
          internal, flatten_nested_kind)
      name_parts.append(self._token.name)
      return name_parts

    name_parts = []
    if internal:
      name_parts.append("internal")

    if (flatten_nested_kind and mojom.IsEnumKind(self._token) and
        self._token.parent_kind):
      name = "%s_%s" % (self._token.parent_kind.name,
                        self._MapKindName(self._token, internal))
      name_parts.append(name)
      return name_parts

    if self._token.parent_kind:
      name_parts.append(self._MapKindName(self._token.parent_kind, internal))
    name_parts.append(self._MapKindName(self._token, internal))
    return name_parts

  def _ShouldIncludeNamespace(self, add_same_module_namespaces):
    return add_same_module_namespaces or self._token.imported_from

  def _GetNamespace(self):
    if self._token.imported_from:
      return NamespaceToArray(self._token.imported_from["namespace"])
    elif hasattr(self._token, "module"):
      return NamespaceToArray(self._token.module.namespace)
    return []


def ConstantValue(constant):
  return ExpressionToText(constant.value, kind=constant.kind)

# TODO(yzshen): Revisit the default value feature. It was designed prior to
# custom type mapping.
def DefaultValue(field):
  if field.default:
    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      if not IsTypemappedKind(field.kind):
        return "%s::New()" % GetNameForKind(field.kind)
    return ExpressionToText(field.default, kind=field.kind)
  if not _use_new_wrapper_types:
    if mojom.IsArrayKind(field.kind) or mojom.IsMapKind(field.kind):
      return "nullptr";
    if mojom.IsStringKind(field.kind):
      return "" if _for_blink else "nullptr"
  return ""

def NamespaceToArray(namespace):
  return namespace.split(".") if namespace else []

def GetNameForKind(kind, internal=False, flatten_nested_kind=False,
                   add_same_module_namespaces=False):
  return _NameFormatter(kind, _variant).FormatForCpp(
      internal=internal, flatten_nested_kind=flatten_nested_kind,
      add_same_module_namespaces=add_same_module_namespaces)

def GetQualifiedNameForKind(kind, internal=False, flatten_nested_kind=False):
  return _NameFormatter(kind, _variant).FormatForCpp(
      internal=internal, add_same_module_namespaces=True,
      flatten_nested_kind=flatten_nested_kind)

def GetFullMojomNameForKind(kind):
  return _NameFormatter(kind, _variant).FormatForMojom()

def IsTypemappedKind(kind):
  return hasattr(kind, "name") and \
      GetFullMojomNameForKind(kind) in _current_typemap

def IsNativeOnlyKind(kind):
  return (mojom.IsStructKind(kind) or mojom.IsEnumKind(kind)) and \
      kind.native_only


def IsHashableKind(kind):
  """Check if the kind can be hashed.

  Args:
    kind: {Kind} The kind to check.

  Returns:
    {bool} True if a value of this kind can be hashed.
  """
  checked = set()
  def Check(kind):
    if kind.spec in checked:
      return True
    checked.add(kind.spec)
    if mojom.IsNullableKind(kind):
      return False
    elif mojom.IsStructKind(kind):
      if (IsTypemappedKind(kind) and
          not _current_typemap[GetFullMojomNameForKind(kind)]["hashable"]):
        return False
      return all(Check(field.kind) for field in kind.fields)
    elif mojom.IsEnumKind(kind):
      if (IsTypemappedKind(kind) and
          not _current_typemap[GetFullMojomNameForKind(kind)]["hashable"]):
        return False
      return True
    elif mojom.IsUnionKind(kind):
      return all(Check(field.kind) for field in kind.fields)
    elif mojom.IsAnyHandleKind(kind):
      return False
    elif mojom.IsAnyInterfaceKind(kind):
      return False
    # TODO(tibell): Arrays and maps could be made hashable. We just don't have a
    # use case yet.
    elif mojom.IsArrayKind(kind):
      return False
    elif mojom.IsMapKind(kind):
      return False
    else:
      return True
  return Check(kind)


def GetNativeTypeName(typemapped_kind):
  return _current_typemap[GetFullMojomNameForKind(typemapped_kind)]["typename"]

def GetCppPodType(kind):
  if mojom.IsStringKind(kind):
    return "char*"
  return _kind_to_cpp_type[kind]

def GetCppWrapperType(kind, add_same_module_namespaces=False):
  def _AddOptional(type_name):
    pattern = "WTF::Optional<%s>" if _for_blink else "base::Optional<%s>"
    return pattern % type_name

  if IsTypemappedKind(kind):
    type_name = GetNativeTypeName(kind)
    if (mojom.IsNullableKind(kind) and
        not _current_typemap[GetFullMojomNameForKind(kind)][
           "nullable_is_same_type"]):
      type_name = _AddOptional(type_name)
    return type_name
  if mojom.IsEnumKind(kind):
    return GetNameForKind(
        kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
    return "%sPtr" % GetNameForKind(
        kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsArrayKind(kind):
    pattern = None
    if _use_new_wrapper_types:
      pattern = "WTF::Vector<%s>" if _for_blink else "std::vector<%s>"
      if mojom.IsNullableKind(kind):
        pattern = _AddOptional(pattern)
    else:
      pattern = "mojo::WTFArray<%s>" if _for_blink else "mojo::Array<%s>"
    return pattern % GetCppWrapperType(
        kind.kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsMapKind(kind):
    pattern = None
    if _use_new_wrapper_types:
      pattern = ("WTF::HashMap<%s, %s>" if _for_blink else
                 "std::unordered_map<%s, %s>")
      if mojom.IsNullableKind(kind):
        pattern = _AddOptional(pattern)
    else:
      pattern = "mojo::WTFMap<%s, %s>" if _for_blink else "mojo::Map<%s, %s>"
    return pattern % (
        GetCppWrapperType(
            kind.key_kind,
            add_same_module_namespaces=add_same_module_namespaces),
        GetCppWrapperType(
            kind.value_kind,
            add_same_module_namespaces=add_same_module_namespaces))
  if mojom.IsInterfaceKind(kind):
    return "%sPtr" % GetNameForKind(
        kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsInterfaceRequestKind(kind):
    return "%sRequest" % GetNameForKind(
        kind.kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsAssociatedInterfaceKind(kind):
    return "%sAssociatedPtrInfo" % GetNameForKind(
        kind.kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsAssociatedInterfaceRequestKind(kind):
    return "%sAssociatedRequest" % GetNameForKind(
        kind.kind, add_same_module_namespaces=add_same_module_namespaces)
  if mojom.IsStringKind(kind):
    if _for_blink:
      return "WTF::String"
    if not _use_new_wrapper_types:
      return "mojo::String"
    type_name = "std::string"
    return _AddOptional(type_name) if mojom.IsNullableKind(kind) else type_name
  if mojom.IsGenericHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  if not kind in _kind_to_cpp_type:
    raise Exception("Unrecognized kind %s" % kind.spec)
  return _kind_to_cpp_type[kind]

def IsMoveOnlyKind(kind):
  if IsTypemappedKind(kind):
    if mojom.IsEnumKind(kind):
      return False
    return _current_typemap[GetFullMojomNameForKind(kind)]["move_only"]
  if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
    return True
  if mojom.IsArrayKind(kind):
    return IsMoveOnlyKind(kind.kind) if _use_new_wrapper_types else True
  if mojom.IsMapKind(kind):
    return IsMoveOnlyKind(kind.value_kind) if _use_new_wrapper_types else True
  if mojom.IsAnyHandleOrInterfaceKind(kind):
    return True
  return False

def IsCopyablePassByValue(kind):
  if not IsTypemappedKind(kind):
    return False
  return _current_typemap[GetFullMojomNameForKind(kind)][
      "copyable_pass_by_value"]

def ShouldPassParamByValue(kind):
  return ((not mojom.IsReferenceKind(kind)) or IsMoveOnlyKind(kind) or
      IsCopyablePassByValue(kind))

def GetCppWrapperParamType(kind):
  cpp_wrapper_type = GetCppWrapperType(kind)
  return (cpp_wrapper_type if ShouldPassParamByValue(kind)
                           else "const %s&" % cpp_wrapper_type)

def GetCppFieldType(kind):
  if mojom.IsStructKind(kind):
    return ("mojo::internal::Pointer<%s>" %
        GetNameForKind(kind, internal=True))
  if mojom.IsUnionKind(kind):
    return "%s" % GetNameForKind(kind, internal=True)
  if mojom.IsArrayKind(kind):
    return ("mojo::internal::Pointer<mojo::internal::Array_Data<%s>>" %
            GetCppFieldType(kind.kind))
  if mojom.IsMapKind(kind):
    return ("mojo::internal::Pointer<mojo::internal::Map_Data<%s, %s>>" %
            (GetCppFieldType(kind.key_kind), GetCppFieldType(kind.value_kind)))
  if mojom.IsInterfaceKind(kind):
    return "mojo::internal::Interface_Data"
  if mojom.IsInterfaceRequestKind(kind):
    return "mojo::internal::Handle_Data"
  if mojom.IsAssociatedInterfaceKind(kind):
    return "mojo::internal::AssociatedInterface_Data"
  if mojom.IsAssociatedInterfaceRequestKind(kind):
    return "mojo::internal::AssociatedInterfaceRequest_Data"
  if mojom.IsEnumKind(kind):
    return "int32_t"
  if mojom.IsStringKind(kind):
    return "mojo::internal::Pointer<mojo::internal::String_Data>"
  if mojom.IsAnyHandleKind(kind):
    return "mojo::internal::Handle_Data"
  return _kind_to_cpp_type[kind]

def GetCppUnionFieldType(kind):
  if mojom.IsUnionKind(kind):
    return ("mojo::internal::Pointer<%s>" % GetNameForKind(kind, internal=True))
  return GetCppFieldType(kind)

def GetUnionGetterReturnType(kind):
  if mojom.IsReferenceKind(kind):
    return "%s&" % GetCppWrapperType(kind)
  return GetCppWrapperType(kind)

def GetUnionTraitGetterReturnType(kind):
  """Get field type used in UnionTraits template specialization.

  The type may be qualified as UnionTraits specializations live outside the
  namespace where e.g. structs are defined.

  Args:
    kind: {Kind} The type of the field.

  Returns:
    {str} The C++ type to use for the field.
  """
  if mojom.IsReferenceKind(kind):
    return "%s&" % GetCppWrapperType(kind, add_same_module_namespaces=True)
  return GetCppWrapperType(kind, add_same_module_namespaces=True)

def GetCppDataViewType(kind, qualified=False):
  def _GetName(input_kind):
    return _NameFormatter(input_kind, None).FormatForCpp(
        add_same_module_namespaces=qualified, flatten_nested_kind=True)

  if mojom.IsEnumKind(kind):
    return _GetName(kind)
  if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
    return "%sDataView" % _GetName(kind)
  if mojom.IsArrayKind(kind):
    return "mojo::ArrayDataView<%s>" % GetCppDataViewType(kind.kind, qualified)
  if mojom.IsMapKind(kind):
    return ("mojo::MapDataView<%s, %s>" % (
        GetCppDataViewType(kind.key_kind, qualified),
        GetCppDataViewType(kind.value_kind, qualified)))
  if mojom.IsStringKind(kind):
    return "mojo::StringDataView"
  if mojom.IsInterfaceKind(kind):
    return "%sPtrDataView" % _GetName(kind)
  if mojom.IsInterfaceRequestKind(kind):
    return "%sRequestDataView" % _GetName(kind.kind)
  if mojom.IsAssociatedInterfaceKind(kind):
    return "%sAssociatedPtrInfoDataView" % _GetName(kind.kind)
  if mojom.IsAssociatedInterfaceRequestKind(kind):
    return "%sAssociatedRequestDataView" % _GetName(kind.kind)
  if mojom.IsGenericHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetUnmappedTypeForSerializer(kind):
  return GetCppDataViewType(kind, qualified=True)

def TranslateConstants(token, kind):
  if isinstance(token, mojom.NamedValue):
    return _NameFormatter(token, _variant).FormatForCpp(
        flatten_nested_kind=True)

  if isinstance(token, mojom.BuiltinValue):
    if token.value == "double.INFINITY" or token.value == "float.INFINITY":
      return "INFINITY";
    if token.value == "double.NEGATIVE_INFINITY" or \
       token.value == "float.NEGATIVE_INFINITY":
      return "-INFINITY";
    if token.value == "double.NAN" or token.value == "float.NAN":
      return "NAN";

  if (kind is not None and mojom.IsFloatKind(kind)):
      return token if token.isdigit() else token + "f";

  # Per C++11, 2.14.2, the type of an integer literal is the first of the
  # corresponding list in Table 6 in which its value can be represented. In this
  # case, the list for decimal constants with no suffix is:
  #   int, long int, long long int
  # The standard considers a program ill-formed if it contains an integer
  # literal that cannot be represented by any of the allowed types.
  #
  # As it turns out, MSVC doesn't bother trying to fall back to long long int,
  # so the integral constant -2147483648 causes it grief: it decides to
  # represent 2147483648 as an unsigned integer, and then warns that the unary
  # minus operator doesn't make sense on unsigned types. Doh!
  if kind == mojom.INT32 and token == "-2147483648":
    return "(-%d - 1) /* %s */" % (
        2**31 - 1, "Workaround for MSVC bug; see https://crbug.com/445618")

  return "%s%s" % (token, _kind_to_cpp_literal_suffix.get(kind, ""))

def ExpressionToText(value, kind=None):
  return TranslateConstants(value, kind)

def RequiresContextForDataView(kind):
  for field in kind.fields:
    if mojom.IsReferenceKind(field.kind):
      return True
  return False

def ShouldInlineStruct(struct):
  # TODO(darin): Base this on the size of the wrapper class.
  if len(struct.fields) > 4:
    return False
  for field in struct.fields:
    if mojom.IsReferenceKind(field.kind) and not mojom.IsStringKind(field.kind):
      return False
  return True

def ContainsMoveOnlyMembers(struct):
  for field in struct.fields:
    if IsMoveOnlyKind(field.kind):
      return True
  return False

def ShouldInlineUnion(union):
  return not any(
      mojom.IsReferenceKind(field.kind) and not mojom.IsStringKind(field.kind)
           for field in union.fields)

def GetContainerValidateParamsCtorArgs(kind):
  if mojom.IsStringKind(kind):
    expected_num_elements = 0
    element_is_nullable = False
    key_validate_params = "nullptr"
    element_validate_params = "nullptr"
    enum_validate_func = "nullptr"
  elif mojom.IsMapKind(kind):
    expected_num_elements = 0
    element_is_nullable = False
    key_validate_params = GetNewContainerValidateParams(mojom.Array(
        kind=kind.key_kind))
    element_validate_params = GetNewContainerValidateParams(mojom.Array(
        kind=kind.value_kind))
    enum_validate_func = "nullptr"
  else:  # mojom.IsArrayKind(kind)
    expected_num_elements = generator.ExpectedArraySize(kind) or 0
    element_is_nullable = mojom.IsNullableKind(kind.kind)
    key_validate_params = "nullptr"
    element_validate_params = GetNewContainerValidateParams(kind.kind)
    if mojom.IsEnumKind(kind.kind):
      enum_validate_func = ("%s::Validate" %
                            GetQualifiedNameForKind(kind.kind, internal=True,
                                                    flatten_nested_kind=True))
    else:
      enum_validate_func = "nullptr"

  if enum_validate_func == "nullptr":
    if key_validate_params == "nullptr":
      return "%d, %s, %s" % (expected_num_elements,
                             "true" if element_is_nullable else "false",
                             element_validate_params)
    else:
      return "%s, %s" % (key_validate_params, element_validate_params)
  else:
    return "%d, %s" % (expected_num_elements, enum_validate_func)

def GetNewContainerValidateParams(kind):
  if (not mojom.IsArrayKind(kind) and not mojom.IsMapKind(kind) and
      not mojom.IsStringKind(kind)):
    return "nullptr"

  return "new mojo::internal::ContainerValidateParams(%s)" % (
      GetContainerValidateParamsCtorArgs(kind))

class Generator(generator.Generator):

  cpp_filters = {
    "constant_value": ConstantValue,
    "contains_handles_or_interfaces": mojom.ContainsHandlesOrInterfaces,
    "contains_move_only_members": ContainsMoveOnlyMembers,
    "cpp_wrapper_param_type": GetCppWrapperParamType,
    "cpp_data_view_type": GetCppDataViewType,
    "cpp_field_type": GetCppFieldType,
    "cpp_union_field_type": GetCppUnionFieldType,
    "cpp_pod_type": GetCppPodType,
    "cpp_union_getter_return_type": GetUnionGetterReturnType,
    "cpp_union_trait_getter_return_type": GetUnionTraitGetterReturnType,
    "cpp_wrapper_type": GetCppWrapperType,
    "default_value": DefaultValue,
    "expression_to_text": ExpressionToText,
    "get_container_validate_params_ctor_args":
        GetContainerValidateParamsCtorArgs,
    "get_name_for_kind": GetNameForKind,
    "get_pad": pack.GetPad,
    "get_qualified_name_for_kind": GetQualifiedNameForKind,
    "has_callbacks": mojom.HasCallbacks,
    "has_sync_methods": mojom.HasSyncMethods,
    "requires_context_for_data_view": RequiresContextForDataView,
    "should_inline": ShouldInlineStruct,
    "should_inline_union": ShouldInlineUnion,
    "is_array_kind": mojom.IsArrayKind,
    "is_enum_kind": mojom.IsEnumKind,
    "is_integral_kind": mojom.IsIntegralKind,
    "is_native_only_kind": IsNativeOnlyKind,
    "is_any_handle_kind": mojom.IsAnyHandleKind,
    "is_any_interface_kind": mojom.IsAnyInterfaceKind,
    "is_any_handle_or_interface_kind": mojom.IsAnyHandleOrInterfaceKind,
    "is_associated_kind": mojom.IsAssociatedKind,
    "is_hashable": IsHashableKind,
    "is_map_kind": mojom.IsMapKind,
    "is_nullable_kind": mojom.IsNullableKind,
    "is_object_kind": mojom.IsObjectKind,
    "is_reference_kind": mojom.IsReferenceKind,
    "is_string_kind": mojom.IsStringKind,
    "is_struct_kind": mojom.IsStructKind,
    "is_typemapped_kind": IsTypemappedKind,
    "is_union_kind": mojom.IsUnionKind,
    "passes_associated_kinds": mojom.PassesAssociatedKinds,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "stylize_method": generator.StudlyCapsToCamel,
    "under_to_camel": generator.UnderToCamel,
    "unmapped_type_for_serializer": GetUnmappedTypeForSerializer,
  }

  def GetExtraTraitsHeaders(self):
    extra_headers = set()
    for entry in self.typemap.itervalues():
      extra_headers.update(entry.get("traits_headers", []))
    return list(extra_headers)

  def GetExtraPublicHeaders(self):
    extra_headers = set()
    for entry in self.typemap.itervalues():
      extra_headers.update(entry.get("public_headers", []))
    return list(extra_headers)

  def GetJinjaExports(self):
    structs = self.GetStructs()
    interfaces = self.GetInterfaces()
    all_enums = list(self.module.enums)
    for struct in structs:
      all_enums.extend(struct.enums)
    for interface in interfaces:
      all_enums.extend(interface.enums)

    return {
      "module": self.module,
      "namespace": self.module.namespace,
      "namespaces_as_array": NamespaceToArray(self.module.namespace),
      "imports": self.module.imports,
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "all_enums": all_enums,
      "structs": structs,
      "unions": self.GetUnions(),
      "interfaces": interfaces,
      "variant": self.variant,
      "extra_traits_headers": self.GetExtraTraitsHeaders(),
      "extra_public_headers": self.GetExtraPublicHeaders(),
      "for_blink": self.for_blink,
      "use_new_wrapper_types": self.use_new_wrapper_types,
      "export_attribute": self.export_attribute,
      "export_header": self.export_header,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "cpp_templates"

  @classmethod
  def GetFilters(cls):
    return cls.cpp_filters

  @UseJinja("module.h.tmpl")
  def GenerateModuleHeader(self):
    return self.GetJinjaExports()

  @UseJinja("module.cc.tmpl")
  def GenerateModuleSource(self):
    return self.GetJinjaExports()

  @UseJinja("module-shared.h.tmpl")
  def GenerateModuleSharedHeader(self):
    return self.GetJinjaExports()

  @UseJinja("module-shared-internal.h.tmpl")
  def GenerateModuleSharedInternalHeader(self):
    return self.GetJinjaExports()

  @UseJinja("module-shared.cc.tmpl")
  def GenerateModuleSharedSource(self):
    return self.GetJinjaExports()

  def GenerateFiles(self, args):
    if self.generate_non_variant_code:
      self.Write(self.GenerateModuleSharedHeader(),
                 self.MatchMojomFilePath("%s-shared.h" % self.module.name))
      self.Write(
          self.GenerateModuleSharedInternalHeader(),
          self.MatchMojomFilePath("%s-shared-internal.h" % self.module.name))
      self.Write(self.GenerateModuleSharedSource(),
                 self.MatchMojomFilePath("%s-shared.cc" % self.module.name))
    else:
      global _current_typemap
      _current_typemap = self.typemap
      global _for_blink
      _for_blink = self.for_blink
      global _use_new_wrapper_types
      _use_new_wrapper_types = self.use_new_wrapper_types
      global _variant
      _variant = self.variant
      suffix = "-%s" % self.variant if self.variant else ""
      self.Write(self.GenerateModuleHeader(),
                 self.MatchMojomFilePath("%s%s.h" % (self.module.name, suffix)))
      self.Write(
          self.GenerateModuleSource(),
          self.MatchMojomFilePath("%s%s.cc" % (self.module.name, suffix)))
