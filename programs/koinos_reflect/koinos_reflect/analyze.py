
import argparse
import json
import sys

from . import lexer
from . import parser
from . import schema

def read_files(filenames):
    result = []
    if filenames is None:
        return result
    for fn in filenames:
        with open(fn, "r") as f:
            content = f.read()
            result.append(content)
            if not content.endswith("\n"):
                result.append("\n")
    return "".join(result)

def main(argv):
    argparser = argparse.ArgumentParser(description="Analyze files")

    argparser.add_argument("typefiles",   metavar="FILES", nargs="*", type=str, help="Type file(s)")
    argparser.add_argument("-l", "--lex",    action="store_true", help="Lex input file(s)")
    argparser.add_argument("-p", "--parse",  action="store_true", help="Parse input file(s)")
    argparser.add_argument("-s", "--schema", action="store_true", help="Generate schema")
    argparser.add_argument("-o", "--output", default="-", type=str, help="Output file (default=stdout)")
    args = argparser.parse_args(argv)

    content = read_files(args.typefiles)

    if args.output == "-":
        out = sys.stdout
        close_out = False
    else:
        out = open(args.output, "w")
        close_out = True
    try:
        if args.lex:
            for tok in lexer.lex(content):
                out.write(tok.to_json(separators=(",", ":"))+"\n")
        if args.parse:
            out.write(parser.parse(content).to_json(separators=(",", ":")))
            out.write("\n")
        if args.schema:
            parsed_content = parser.parse(content)
            schemanator = schema.Schemanator(parsed_content)
            schemanator.schemanate()
            schema_json = schemanator.schema.to_json(separators=(",", ":"))
            # Make sure the JSON we produce is loadable with from_json()
            schema2 = schema.Schema.from_json(schema_json)
            out.write(schema_json)
            out.write("\n")
    finally:
        if close_out:
            out.close()
        else:
            out.flush()

if __name__ == "__main__":
    main(sys.argv[1:])
