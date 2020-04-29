
from .lexer import Lexer

from dataclasses_json import dataclass_json

from dataclasses import dataclass, field
from typing import List, Tuple, Union

# Parser of .base files
# Syntax is semicolon separated list of qualified types

@dataclass_json
@dataclass
class BasefileType:
    name: List[str]
    doc: str

@dataclass_json
@dataclass
class BasefileTop:
    types: List[BasefileType]
    doc: str

class BasefileParser:
    def __init__(self, data):
        self.lexer = Lexer(data)

    def parse(self):
        return self.parse_toplevel()

    def parse_typelist(self):
        result = []
        while True:
            tok = self.peek()
            if tok.ttype == "EOF":
                break
            elif tok.ttype == "SEMI":
                self.expect("SEMI")
                continue
            name = self.parse_qualname()
            semi = self.expect("SEMI")
            result.append(BasefileType(name=name, doc=semi.doc))
        return result

    def parse_qualname(self):
        result = []
        result.append(self.expect("ID").text)
        while self.peek().ttype == "QUAL":
            self.expect("QUAL")
            result.append(self.expect("ID").text)
        return result

def parse(data):
    return BasefileParser(data).parse()
