
import collections
import itertools
import re

from dataclasses_json import dataclass_json

from dataclasses import dataclass, field

def define_regexes():
    CPP_COMMENT = r"[/][/].*?\x0d?\x0a"
    DOC_COMMENT = r"[/][*][*](?:.|\x0d?\x0a)*?[*][/]"
    C_COMMENT   = r"[/][*](?:.|\x0d?\x0a)*?[*][/]"

    WS = r"(?:\s)+".format(**locals())

    KW_TYPEDEF = r"typedef"
    KW_STRUCT  = r"struct"
    KW_NS      = r"namespace"
    KW_ENUM    = r"enum"
    KW_CLASS   = r"class"
    KW_KOINOS_BASETYPE = r"KOINOS_BASETYPE"

    ID = r"[_a-zA-Z][_a-zA-Z0-9]*"
    LT = r"[<]"
    GT = r"[>]"
    LBRACE  = r"[{]"
    RBRACE  = r"[}]"
    SEMI = r"[;]"
    COMMA = r"[,]"
    QUAL = r"[:][:]"
    LPAREN = r"[(]"
    RPAREN = r"[)]"
    ASSIGN = r"[=]"

    BIN_UINT = r"0[bB][01](?:[']?[01])*"
    OCT_UINT = r"0(?:[']?[0-7])*"
    DEC_UINT = r"[1-9](?:[']?[0-9])*"
    HEX_UINT = r"0[xX][0-9a-fA-F](?:[']?[0-9a-fA-F])*"

    UINT = r"(?:{BIN_UINT}|{OCT_UINT}|{DEC_UINT}|{HEX_UINT})".format(**locals())

    tokens = ["WS", "DOC_COMMENT", "CPP_COMMENT", "C_COMMENT",
       "KW_TYPEDEF", "KW_STRUCT", "KW_NS", "KW_ENUM", "KW_CLASS", "KW_KOINOS_BASETYPE",
       "ID", "LT", "GT", "LBRACE", "RBRACE", "SEMI", "COMMA", "QUAL", "LPAREN", "RPAREN", "ASSIGN",
       "UINT"]
    result = []
    for name in tokens:
        result.append((name, re.compile(locals()[name])))
    return result

regexes = define_regexes()

class LexError(Exception):
    pass

@dataclass_json
@dataclass
class Token:
    ttype: str
    text: str
    doc: str

def lex(data):
    # This is a pretty inefficient lex algorithm, O(n^2)
    while len(data) > 0:
        for name, r in regexes:
            m = r.match(data)
            if m is not None:
                g = m.group(0)
                yield Token(ttype=name, text=g, doc="")
                data = data[len(g):]
                break
        else:
            raise LexError("Could not parse at position")
    yield Token(ttype="EOF", text="", doc="")

def lex_filter(it):
    # (1) filter whitespace / comments, (2) apply doc comment to semicolon of the following declaration.
    doc_comment = ""
    skip_types = frozenset(["WS", "CPP_COMMENT", "C_COMMENT"])
    docable_types = frozenset(["SEMI", "KW_NS", "KW_STRUCT", "KW_TYPEDEF", "KW_ENUM", "KW_KOINOS_BASETYPE"])
    for tok in it:
        if tok.ttype == "DOC_COMMENT":
            doc_comment = tok.text
            continue
        if tok.ttype in skip_types:
            continue
        if tok.ttype in docable_types:
            tok.doc = doc_comment
            doc_comment = ""
        yield tok

class Lexer:
    def __init__(self, data):
        self.it = itertools.chain( lex_filter(lex(data)), itertools.repeat([Token(ttype="EOF", text="", doc="")]) )
        self.peekbuf = collections.deque()

    def _fill_peek(self, n):
        while len(self.peekbuf) < n:
            self.peekbuf.append( next(self.it) )

    def peek(self, offset=0):
        self._fill_peek(offset+1)
        return self.peekbuf[offset]

    def read(self):
        self._fill_peek(1)
        return self.peekbuf.popleft()

def main():
    import sys

    with open(sys.argv[1], "r") as f:
        program_text = f.read()

    for tok in lex(program_text):
        print(tok)

if __name__ == "__main__":
    main()
