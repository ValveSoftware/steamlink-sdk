# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja

_kind_to_javascript_default_value = {
  mojom.BOOL:         "false",
  mojom.INT8:         "0",
  mojom.UINT8:        "0",
  mojom.INT16:        "0",
  mojom.UINT16:       "0",
  mojom.INT32:        "0",
  mojom.UINT32:       "0",
  mojom.FLOAT:        "0",
  mojom.HANDLE:       "null",
  mojom.DCPIPE:       "null",
  mojom.DPPIPE:       "null",
  mojom.MSGPIPE:      "null",
  mojom.SHAREDBUFFER: "null",
  mojom.INT64:        "0",
  mojom.UINT64:       "0",
  mojom.DOUBLE:       "0",
  mojom.STRING:       '""',
}


def JavaScriptDefaultValue(field):
  if field.default:
    if isinstance(field.kind, mojom.Struct):
      assert field.default == "default"
      return "new %s()" % JavascriptType(field.kind)
    return ExpressionToText(field.default)
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_javascript_default_value[field.kind]
  if isinstance(field.kind, mojom.Struct):
    return "null"
  if isinstance(field.kind, mojom.Array):
    return "[]"
  if isinstance(field.kind, mojom.Interface) or \
     isinstance(field.kind, mojom.InterfaceRequest):
    return _kind_to_javascript_default_value[mojom.MSGPIPE]
  if isinstance(field.kind, mojom.Enum):
    return "0"


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = pack.GetPad(offset, 8)
  return offset + pad


_kind_to_codec_type = {
  mojom.BOOL:         "codec.Uint8",
  mojom.INT8:         "codec.Int8",
  mojom.UINT8:        "codec.Uint8",
  mojom.INT16:        "codec.Int16",
  mojom.UINT16:       "codec.Uint16",
  mojom.INT32:        "codec.Int32",
  mojom.UINT32:       "codec.Uint32",
  mojom.FLOAT:        "codec.Float",
  mojom.HANDLE:       "codec.Handle",
  mojom.DCPIPE:       "codec.Handle",
  mojom.DPPIPE:       "codec.Handle",
  mojom.MSGPIPE:      "codec.Handle",
  mojom.SHAREDBUFFER: "codec.Handle",
  mojom.INT64:        "codec.Int64",
  mojom.UINT64:       "codec.Uint64",
  mojom.DOUBLE:       "codec.Double",
  mojom.STRING:       "codec.String",
}


def CodecType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_codec_type[kind]
  if isinstance(kind, mojom.Struct):
    return "new codec.PointerTo(%s)" % CodecType(kind.name)
  if isinstance(kind, mojom.Array):
    return "new codec.ArrayOf(%s)" % CodecType(kind.kind)
  if isinstance(kind, mojom.Interface) or \
     isinstance(kind, mojom.InterfaceRequest):
    return CodecType(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return _kind_to_codec_type[mojom.INT32]
  return kind


def JavaScriptDecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "decodeStruct(%s)" % CodecType(kind)
  if isinstance(kind, mojom.Struct):
    return "decodeStructPointer(%s)" % CodecType(kind.name)
  if isinstance(kind, mojom.Array):
    return "decodeArrayPointer(%s)" % CodecType(kind.kind)
  if isinstance(kind, mojom.Interface) or \
     isinstance(kind, mojom.InterfaceRequest):
    return JavaScriptDecodeSnippet(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return JavaScriptDecodeSnippet(mojom.INT32)


def JavaScriptEncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "encodeStruct(%s, " % CodecType(kind)
  if isinstance(kind, mojom.Struct):
    return "encodeStructPointer(%s, " % CodecType(kind.name)
  if isinstance(kind, mojom.Array):
    return "encodeArrayPointer(%s, " % CodecType(kind.kind)
  if isinstance(kind, mojom.Interface) or \
     isinstance(kind, mojom.InterfaceRequest):
    return JavaScriptEncodeSnippet(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return JavaScriptEncodeSnippet(mojom.INT32)


def TranslateConstants(token):
  if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
    # Both variable and enum constants are constructed like:
    # NamespaceUid.Struct[.Enum].CONSTANT_NAME
    name = []
    if token.imported_from:
      name.append(token.imported_from["unique_name"])
    if token.parent_kind:
      name.append(token.parent_kind.name)
    if isinstance(token, mojom.EnumValue):
      name.append(token.enum_name)
    name.append(token.name)
    return ".".join(name)
  return token


def ExpressionToText(value):
  return TranslateConstants(value)


def JavascriptType(kind):
  if kind.imported_from:
    return kind.imported_from["unique_name"] + "." + kind.name
  return kind.name


class Generator(generator.Generator):

  js_filters = {
    "default_value": JavaScriptDefaultValue,
    "payload_size": JavaScriptPayloadSize,
    "decode_snippet": JavaScriptDecodeSnippet,
    "encode_snippet": JavaScriptEncodeSnippet,
    "expression_to_text": ExpressionToText,
    "js_type": JavascriptType,
    "stylize_method": generator.StudlyCapsToCamel,
  }

  @UseJinja("js_templates/module.js.tmpl", filters=js_filters)
  def GenerateJsModule(self):
    return {
      "namespace": self.module.namespace,
      "imports": self.GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
    }

  def GenerateFiles(self, args):
    self.Write(self.GenerateJsModule(), "%s.js" % self.module.name)

  def GetImports(self):
    # Since each import is assigned a variable in JS, they need to have unique
    # names.
    counter = 1
    for each in self.module.imports:
      each["unique_name"] = "import" + str(counter)
      counter += 1
    return self.module.imports
