# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a syntax tree from a Mojo IDL file."""

import imp
import os.path
import sys

# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

try:
  imp.find_module("ply")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("mojo"), "third_party"))
from ply import lex
from ply import yacc

from ..error import Error
import ast
from lexer import Lexer


_MAX_ORDINAL_VALUE = 0xffffffff


def _ListFromConcat(*items):
  """Generate list by concatenating inputs (note: only concatenates lists, not
  tuples or other iterables)."""
  itemsout = []
  for item in items:
    if item is None:
      continue
    if type(item) is not type([]):
      itemsout.append(item)
    else:
      itemsout.extend(item)
  return itemsout


# Disable lint check for exceptions deriving from Exception:
# pylint: disable=W0710
class ParseError(Error):
  """Class for errors from the parser."""

  def __init__(self, filename, message, lineno=None, snippet=None):
    Error.__init__(self, filename, message, lineno=lineno,
                   addenda=([snippet] if snippet else None))


# We have methods which look like they could be functions:
# pylint: disable=R0201
class Parser(object):

  def __init__(self, lexer, source, filename):
    self.tokens = lexer.tokens
    self.source = source
    self.filename = filename

  def p_root(self, p):
    """root : import root
            | module
            | definitions"""
    if len(p) > 2:
      p[0] = _ListFromConcat(p[1], p[2])
    else:
      # Generator expects a module. If one wasn't specified insert one with an
      # empty name.
      if p[1][0] != 'MODULE':
        p[0] = [('MODULE', '', None, p[1])]
      else:
        p[0] = [p[1]]

  def p_import(self, p):
    """import : IMPORT STRING_LITERAL"""
    # 'eval' the literal to strip the quotes.
    p[0] = ('IMPORT', eval(p[2]))

  def p_module(self, p):
    """module : attribute_section MODULE identifier LBRACE definitions RBRACE"""
    p[0] = ('MODULE', p[3], p[1], p[5])

  def p_definitions(self, p):
    """definitions : definition definitions
                   | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

  def p_definition(self, p):
    """definition : struct
                  | interface
                  | enum
                  | const"""
    p[0] = p[1]

  def p_attribute_section(self, p):
    """attribute_section : LBRACKET attributes RBRACKET
                         | """
    if len(p) > 3:
      p[0] = p[2]

  def p_attributes(self, p):
    """attributes : attribute
                  | attribute COMMA attributes
                  | """
    if len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_attribute(self, p):
    """attribute : NAME EQUALS evaled_literal
                 | NAME EQUALS NAME"""
    p[0] = ('ATTRIBUTE', p[1], p[3])

  def p_evaled_literal(self, p):
    """evaled_literal : literal"""
    # 'eval' the literal to strip the quotes.
    p[0] = eval(p[1])

  def p_struct(self, p):
    """struct : attribute_section STRUCT NAME LBRACE struct_body RBRACE SEMI"""
    p[0] = ('STRUCT', p[3], p[1], p[5])

  def p_struct_body(self, p):
    """struct_body : field struct_body
                   | enum struct_body
                   | const struct_body
                   | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

  def p_field(self, p):
    """field : typename NAME ordinal default SEMI"""
    p[0] = ('FIELD', p[1], p[2], p[3], p[4])

  def p_default(self, p):
    """default : EQUALS constant
               | """
    if len(p) > 2:
      p[0] = p[2]

  def p_interface(self, p):
    """interface : attribute_section INTERFACE NAME LBRACE interface_body \
                       RBRACE SEMI"""
    p[0] = ('INTERFACE', p[3], p[1], p[5])

  def p_interface_body(self, p):
    """interface_body : method interface_body
                      | enum interface_body
                      | const interface_body
                      | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

  def p_response(self, p):
    """response : RESPONSE LPAREN parameters RPAREN
                | """
    if len(p) > 3:
      p[0] = p[3]

  def p_method(self, p):
    """method : NAME ordinal LPAREN parameters RPAREN response SEMI"""
    p[0] = ('METHOD', p[1], p[4], p[2], p[6])

  def p_parameters(self, p):
    """parameters : parameter
                  | parameter COMMA parameters
                  | """
    if len(p) == 1:
      p[0] = []
    elif len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_parameter(self, p):
    """parameter : typename NAME ordinal"""
    p[0] = ('PARAM', p[1], p[2], p[3])

  def p_typename(self, p):
    """typename : basictypename
                | array
                | interfacerequest"""
    p[0] = p[1]

  def p_basictypename(self, p):
    """basictypename : identifier
                     | handletype"""
    p[0] = p[1]

  def p_handletype(self, p):
    """handletype : HANDLE
                  | HANDLE LANGLE NAME RANGLE"""
    if len(p) == 2:
      p[0] = p[1]
    else:
      if p[3] not in ('data_pipe_consumer',
                      'data_pipe_producer',
                      'message_pipe',
                      'shared_buffer'):
        # Note: We don't enable tracking of line numbers for everything, so we
        # can't use |p.lineno(3)|.
        raise ParseError(self.filename, "Invalid handle type %r:" % p[3],
                         lineno=p.lineno(1),
                         snippet=self._GetSnippet(p.lineno(1)))
      p[0] = "handle<" + p[3] + ">"

  def p_array(self, p):
    """array : typename LBRACKET RBRACKET"""
    p[0] = p[1] + "[]"

  def p_interfacerequest(self, p):
    """interfacerequest : identifier AMP"""
    p[0] = p[1] + "&"

  def p_ordinal(self, p):
    """ordinal : ORDINAL
               | """
    if len(p) > 1:
      value = int(p[1][1:])
      if value > _MAX_ORDINAL_VALUE:
        raise ParseError(self.filename, "Ordinal value %d too large:" % value,
                         lineno=p.lineno(1),
                         snippet=self._GetSnippet(p.lineno(1)))
      p[0] = ast.Ordinal(value, filename=self.filename, lineno=p.lineno(1))
    else:
      p[0] = ast.Ordinal(None)

  def p_enum(self, p):
    """enum : ENUM NAME LBRACE enum_fields RBRACE SEMI"""
    p[0] = ('ENUM', p[2], p[4])

  def p_enum_fields(self, p):
    """enum_fields : enum_field
                   | enum_field COMMA enum_fields
                   | """
    if len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_enum_field(self, p):
    """enum_field : NAME
                  | NAME EQUALS constant"""
    if len(p) == 2:
      p[0] = ('ENUM_FIELD', p[1], None)
    else:
      p[0] = ('ENUM_FIELD', p[1], p[3])

  def p_const(self, p):
    """const : CONST typename NAME EQUALS constant SEMI"""
    p[0] = ('CONST', p[2], p[3], p[5])

  def p_constant(self, p):
    """constant : literal
                | identifier_wrapped"""
    p[0] = p[1]

  def p_identifier_wrapped(self, p):
    """identifier_wrapped : identifier"""
    p[0] = ('IDENTIFIER', p[1])

  def p_identifier(self, p):
    """identifier : NAME
                  | NAME DOT identifier"""
    p[0] = ''.join(p[1:])

  def p_literal(self, p):
    """literal : number
               | CHAR_CONST
               | TRUE
               | FALSE
               | DEFAULT
               | STRING_LITERAL"""
    p[0] = p[1]

  def p_number(self, p):
    """number : digits
              | PLUS digits
              | MINUS digits"""
    p[0] = ''.join(p[1:])

  def p_digits(self, p):
    """digits : INT_CONST_DEC
              | INT_CONST_HEX
              | FLOAT_CONST"""
    p[0] = p[1]

  def p_error(self, e):
    if e is None:
      # Unexpected EOF.
      # TODO(vtl): Can we figure out what's missing?
      raise ParseError(self.filename, "Unexpected end of file")

    raise ParseError(self.filename, "Unexpected %r:" % e.value, lineno=e.lineno,
                     snippet=self._GetSnippet(e.lineno))

  def _GetSnippet(self, lineno):
    return self.source.split('\n')[lineno - 1]


def Parse(source, filename):
  lexer = Lexer(filename)
  parser = Parser(lexer, source, filename)

  lex.lex(object=lexer)
  yacc.yacc(module=parser, debug=0, write_tables=0)

  tree = yacc.parse(source)
  return tree
