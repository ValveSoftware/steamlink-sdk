# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import module as mojom

# This module provides a mechanism for determining the packed order and offsets
# of a mojom.Struct.
#
# ps = pack.PackedStruct(struct)
# ps.packed_fields will access a list of PackedField objects, each of which
# will have an offset, a size and a bit (for mojom.BOOLs).

class PackedField(object):
  kind_to_size = {
    mojom.BOOL:         1,
    mojom.INT8:         1,
    mojom.UINT8:        1,
    mojom.INT16:        2,
    mojom.UINT16:       2,
    mojom.INT32:        4,
    mojom.UINT32:       4,
    mojom.FLOAT:        4,
    mojom.HANDLE:       4,
    mojom.MSGPIPE:      4,
    mojom.SHAREDBUFFER: 4,
    mojom.DCPIPE:       4,
    mojom.DPPIPE:       4,
    mojom.INT64:        8,
    mojom.UINT64:       8,
    mojom.DOUBLE:       8,
    mojom.STRING:       8
  }

  @classmethod
  def GetSizeForKind(cls, kind):
    if isinstance(kind, mojom.Array) or isinstance(kind, mojom.Struct):
      return 8
    if isinstance(kind, mojom.Interface) or \
       isinstance(kind, mojom.InterfaceRequest):
      kind = mojom.MSGPIPE
    if isinstance(kind, mojom.Enum):
      # TODO(mpcomplete): what about big enums?
      return cls.kind_to_size[mojom.INT32]
    if not kind in cls.kind_to_size:
      raise Exception("Invalid kind: %s" % kind.spec)
    return cls.kind_to_size[kind]

  def __init__(self, field, ordinal):
    self.field = field
    self.ordinal = ordinal
    self.size = self.GetSizeForKind(field.kind)
    self.offset = None
    self.bit = None


# Returns the pad necessary to reserve space for alignment of |size|.
def GetPad(offset, size):
  return (size - (offset % size)) % size


# Returns a 2-tuple of the field offset and bit (for BOOLs)
def GetFieldOffset(field, last_field):
  if field.field.kind == mojom.BOOL and \
      last_field.field.kind == mojom.BOOL and \
      last_field.bit < 7:
    return (last_field.offset, last_field.bit + 1)

  offset = last_field.offset + last_field.size
  pad = GetPad(offset, field.size)
  return (offset + pad, 0)


class PackedStruct(object):
  def __init__(self, struct):
    self.struct = struct
    self.packed_fields = []

    # No fields.
    if (len(struct.fields) == 0):
      return

    # Start by sorting by ordinal.
    src_fields = []
    ordinal = 0
    for field in struct.fields:
      if field.ordinal is not None:
        ordinal = field.ordinal
      src_fields.append(PackedField(field, ordinal))
      ordinal += 1
    src_fields.sort(key=lambda field: field.ordinal)

    src_field = src_fields[0]
    src_field.offset = 0
    src_field.bit = 0
    # dst_fields will contain each of the fields, in increasing offset order.
    dst_fields = self.packed_fields
    dst_fields.append(src_field)

    # Then find first slot that each field will fit.
    for src_field in src_fields[1:]:
      last_field = dst_fields[0]
      for i in xrange(1, len(dst_fields)):
        next_field = dst_fields[i]
        offset, bit = GetFieldOffset(src_field, last_field)
        if offset + src_field.size <= next_field.offset:
          # Found hole.
          src_field.offset = offset
          src_field.bit = bit
          dst_fields.insert(i, src_field)
          break
        last_field = next_field
      if src_field.offset is None:
        # Add to end
        src_field.offset, src_field.bit = GetFieldOffset(src_field, last_field)
        dst_fields.append(src_field)

  def GetTotalSize(self):
    if not self.packed_fields:
      return 0
    last_field = self.packed_fields[-1]
    offset = last_field.offset + last_field.size
    pad = GetPad(offset, 8)
    return offset + pad


class ByteInfo(object):
  def __init__(self):
    self.is_padding = False
    self.packed_fields = []


def GetByteLayout(packed_struct):
  bytes = [ByteInfo() for i in xrange(packed_struct.GetTotalSize())]

  limit_of_previous_field = 0
  for packed_field in packed_struct.packed_fields:
    for i in xrange(limit_of_previous_field, packed_field.offset):
      bytes[i].is_padding = True
    bytes[packed_field.offset].packed_fields.append(packed_field)
    limit_of_previous_field = packed_field.offset + packed_field.size

  for i in xrange(limit_of_previous_field, len(bytes)):
    bytes[i].is_padding = True

  for byte in bytes:
    # A given byte cannot both be padding and have a fields packed into it.
    assert not (byte.is_padding and byte.packed_fields)

  return bytes
