#!/usr/bin/python3

import sys

def main():
    if len(sys.argv) < 2:
        sys.exit("Usage: hardcode_wasm.py <wasm_file> [vector_name]")
    fname = sys.argv[1]
    vname = sys.argv[2] if len(sys.argv) > 2 else "CONTRACT"
    with open(fname, "rb") as f:
        data = ','.join(str(-(128-(x-128)) if x > 127 else x) for x in list(f.read()))
        print(f"const std::vector<char> {vname} {{ {data} }};")
        #print(list(data))
        #print(','.join(data))
    return

if __name__ == "__main__":
    main()
