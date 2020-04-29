
from .lexer import Lexer

from dataclasses_json import dataclass_json

from dataclasses import dataclass, field
from typing import List, Tuple, Union, Optional

from collections import OrderedDict

__all__ = [
   "Decl",
   "Namespace",
   "Targ",
   "IntLiteral",
   "Typeref",
   "Typedef",
   "Field",
   "Struct",
   "EnumImplicitValue",
   "EnumValue",
   "EnumEntry",
   "EnumClass",
   "BaseType",
   "Toplevel",
   "ParseError",
   "Parser",
   "parse",
]

Decl = Union["Typedef", "Struct", "Namespace", "EnumClass", "BaseType"]

def info_factory(name):
    def create_info():
        return OrderedDict([("type", name)])
    return create_info

@dataclass_json
@dataclass
class Namespace:
    name: str
    decls: List[Decl]
    doc: str
    info: OrderedDict = field(default_factory=info_factory("Namespace"))

Targ = Union["Typeref", "IntLiteral"]

@dataclass_json
@dataclass
class IntLiteral:
    value: int
    info: OrderedDict = field(default_factory=info_factory("IntLiteral"))

@dataclass_json
@dataclass
class Typeref:
    name: List[str]
    targs: Optional[List[Targ]]
    info: OrderedDict = field(default_factory=info_factory("Typeref"))

@dataclass_json
@dataclass
class Typedef:
    name: str
    tref: Typeref
    doc: str
    info: OrderedDict = field(default_factory=info_factory("Typedef"))

@dataclass_json
@dataclass
class Field:
    name: str
    tref: Typeref
    doc: str
    info: OrderedDict = field(default_factory=info_factory("Field"))

# TODO: support inheritance
@dataclass_json
@dataclass
class Struct:
    name: str
    fields: List[Field]
    doc: str
    info: OrderedDict = field(default_factory=info_factory("Struct"))

@dataclass_json
@dataclass
class EnumImplicitValue:
    value: int
    doc: str
    info: OrderedDict = field(default_factory=info_factory("EnumImplicitValue"))

EnumValue = Union["EnumImplicitValue", "IntLiteral"]

@dataclass_json
@dataclass
class EnumEntry:
    name: str
    value: EnumValue
    doc: str
    info: OrderedDict = field(default_factory=info_factory("EnumEntry"))

@dataclass_json
@dataclass
class EnumClass:
    name: str
    entries: List[EnumEntry]
    doc: str
    info: OrderedDict = field(default_factory=info_factory("EnumClass"))

@dataclass_json
@dataclass
class BaseType:
    name: List[str]
    doc: str
    info: OrderedDict = field(default_factory=info_factory("BaseType"))

@dataclass_json
@dataclass
class Toplevel:
    decls: List[Decl]
    info: OrderedDict = field(default_factory=info_factory("Toplevel"))

class ParseError(Exception):
    pass

class Parser:
    def __init__(self, data):
        self.lexer = Lexer(data)

    def peek(self):
        return self.lexer.peek()

    def expect(self, expected_ttype):
        tok = self.lexer.read()
        if tok.ttype != expected_ttype:
            raise ParseError("Parse error:  Expected {}, got {}".format(expected_ttype, tok.ttype))
        return tok

    def parse(self):
        return self.parse_toplevel()

    def parse_toplevel(self):
        decls = self.parse_decls()
        return Toplevel(decls=decls)

    def parse_decl(self):
        tok = self.peek()
        if tok.ttype == "KW_TYPEDEF":
            return self.parse_typedef()
        elif tok.ttype == "KW_STRUCT":
            return self.parse_struct()
        elif tok.ttype == "KW_NS":
            return self.parse_ns()
        elif tok.ttype == "KW_ENUM":
            return self.parse_enum()
        elif tok.ttype == "KW_KOINOS_BASETYPE":
            return self.parse_basetype()
        else:
            raise ParseError("Parse error:  Cannot parse decl, starts with {}".format(tok.ttype))

    def parse_decls(self):
        decls = []
        while True:
            tok = self.peek()
            if tok.ttype == "RBRACE":
                break
            elif tok.ttype == "EOF":
                break
            elif tok.ttype == "SEMI":
                self.expect("SEMI")
                continue
            decls.append(self.parse_decl())
        return decls

    def parse_ns(self):
        ns = self.expect("KW_NS")
        name = self.expect("ID")
        self.expect("LBRACE")
        decls = self.parse_decls()
        self.expect("RBRACE")
        return Namespace(name=name.text, decls=decls, doc=ns.doc)

    def parse_typedef(self):
        td = self.expect("KW_TYPEDEF")
        tref = self.parse_typeref()
        name = self.expect("ID")
        self.expect("SEMI")
        return Typedef(name=name.text, tref=tref, doc=td.doc)

    def parse_struct(self):
        struct = self.expect("KW_STRUCT")
        name = self.expect("ID")
        fields = []
        self.expect("LBRACE")
        while True:
            tok = self.peek()
            if tok.ttype == "RBRACE":
                break
            elif tok.ttype == "SEMI":
                self.expect("SEMI")
                continue
            tref = self.parse_typeref()
            fname = self.expect("ID")
            fsemi = self.expect("SEMI")
            fields.append(Field(name=fname.text, tref=tref, doc=fsemi.doc))
        self.expect("RBRACE")
        self.expect("SEMI")
        return Struct(name=name.text, fields=fields, doc=struct.doc)

    def parse_enum(self):
        self.expect("KW_ENUM")
        self.expect("KW_CLASS")
        name = self.expect("ID")
        self.expect("LBRACE")
        next_value = 0
        entries = []
        need_comma = False
        while True:
            tok = self.peek()
            if tok.ttype == "RBRACE":
                break
            if need_comma:
                self.expect("COMMA")
            ename = self.expect("ID")
            tok = self.peek()
            if tok.ttype == "ASSIGN":
                self.expect("ASSIGN")
                vlit = self.parse_int_literal()
                value = vlit.value
            else:
                value = next_value
            # TODO fix doc
            entries.append(EnumEntry(name=ename.text, value=value, doc=""))
            next_value = value+1
            need_comma = True
        self.expect("RBRACE")
        semi = self.expect("SEMI")
        return EnumClass(name=name.text, entries=entries, doc=semi.doc)

    def parse_basetype(self):
        basetype = self.expect("KW_KOINOS_BASETYPE")
        self.expect("LPAREN")
        name = self.parse_qualname()
        self.expect("RPAREN")
        return BaseType(name=name, doc=basetype.doc)

    def parse_qualname(self):
        result = []
        result.append(self.expect("ID").text)
        while self.peek().ttype == "QUAL":
            self.expect("QUAL")
            result.append(self.expect("ID").text)
        return result

    def parse_maybe_targs(self):
        if self.peek().ttype != "LT":
            return None
        self.expect("LT")
        result = []
        tok = self.peek()
        if tok.ttype == "GT":
            self.expect("GT")
            return result
        result.append(self.parse_targ())
        while True:
            tok = self.peek()
            if tok.ttype == "GT":
                break
            self.expect("COMMA")
            result.append(self.parse_targ())
        self.expect("GT")
        return result

    def parse_int_literal(self):
        tok = self.expect("UINT")
        text = tok.text.replace("'", "")
        text = text.lower()
        if tok.text == "0":
            value = 0
        elif tok.text.startswith("0b"):
            value = int(tok.text[2:], 2)
        elif tok.text.startswith("0x"):
            value = int(tok.text[2:], 16)
        elif tok.text.startswith("0"):
            value = int(tok.text[1:], 8)
        else:
            value = int(tok.text)
        return IntLiteral(value=value)

    def parse_targ(self):
        tok = self.peek()
        if tok.ttype == "UINT":
            return self.parse_int_literal()
        return self.parse_typeref()

    def parse_typeref(self):
        name = self.parse_qualname()
        maybe_targs = self.parse_maybe_targs()
        return Typeref(name=name, targs=maybe_targs)

def parse(data):
    return Parser(data).parse()

def main():
    import sys

    with open(sys.argv[1], "r") as f:
        program_text = f.read()

    parser = Parser(program_text)
    print(parser.parse().to_json(indent=1))

if __name__ == "__main__":
    main()
