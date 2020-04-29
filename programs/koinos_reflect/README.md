
### KOINOS reflection

The KOINOS reflection system is loosely based on FC reflection.

- Types are defined in `.type` files.  The format of `.type` is a subset of C++.
- A Python module for parsing `.type` files is implemented.
- The schemagen program combines `.type` files with `.base` files to create a `.schema` file.
- Schema is a JSON based format which contains all the information necessary to serialize / deserialize objects in binary, JSON, and CBOR.
- The `.base` file lists base types, which need to have serializers manually implemented in each target language.
- The `.type` file lists derived types, including structs and template instantiations.
- The code generator uses a Jinja2 file to emit serializers and deserializers for your favorite language.

### Deps

```
pip install dataclasses-json Jinja2 importlib_resources
```

### Architecture

Input `.type` files are read by a hand-built lexer and simple LL(1) recursive descent parser.
The schema is created by walking the parse tree.

### Running the example

Example code is included in three files:

- `block.hpp` is a work-in-progress development of what blocks may look like.
- `types.hpp` defines structs that are used by `block.hpp`.
- `block.bt` is a list of *basetypes* whose serialization needs to be explicitly defined, like `uint64` or `vector`.

A `block.schema` file containing all the information necessary to understand how to serialize the types can be created with the command:

```
python3 -m koinos_reflect.analyze block.bt types.hpp block.hpp -s -o block.schema
```

The analyzer only allows a small subset of C++.  Basically you can define types, but that's it.
This forces us to keep any C++ functionality other than what's needed to define serializable types out of the header files.
It may sound like a harsh restriction, but this will help minimize the amount of C++isms from sneaking into `protocol` and adding
work to language runtimes.

If you want to see the lexer / parser output, replace `-s` with `-l` or `-p` in the above command line.

### Running the code generator

The code generator can be run as follows:

```
python3 -m koinos_codegen.codegen --target-path lang --target python -o build -p koinos_protocol block.schema
```

For convenience, there's also a `build.sh` script that runs the analyzer, followed by the code generator.

### Code generator design notes

The code generator is completely separate from the analyzer.  A bit about its architecture:

- The code generator for Python is in [lang/koinos_codegen_python](lang/koinos_codegen_python), other language parsers will be added when ready.
- Most of the Python code generator is implemented as Jinja2 templates in [lang/koinos_codegen_python/templates](lang/koinos_codegen_python/templates).
- A few parts of the code generator are target-language-specific, but easier to implement in a general-purpose programming language rather than the Jinja2 template language.  For the Python target, this kind of functionality is implemented in [lang/koinos_codegen_python/genpy.py](lang/koinos_codegen_python/genpy.py).
- The generated code needs some runtime support, this is in `lang/koinos_codegen_python/rt`.  Basically the `rt` directory is copied into the output, while the `templates` directory is interpreted with Jinja2.

To run the code generator, you can use the `koinos_codegen` module command line interface.  The `koinos_codegen` command line tool is a very simple frontend which lets the user specify a target language and `.schema` file via command line, then runs the selected frontend from the `lang` directory.

This means that the directory organization of having `templates` and `rt`, and even the choice to use Jinja2 at all, is up to a particular backend.  All of the code that relies on these choices is in `genpy.py`.  In my opinion, these are sound architectural choices that maximize developer productivity and minimize unnecessary complexity of backend implementations, but other backends are free to organize themselves differently.

### Interface

### Template limitations

- `vector`, `variant`, `optional` are supported out of the box.
- Templates with integer parameters are supported.  The integer parameter value(s) are supplied to the basetype serialization method.
- If you have another template with type parameters, you have two options:
- (a) A typedef template with type parameters is treated as if it was a basetype.
- (b) Patch the code generator for each language to implement support for your type, similar to `vector` / `variant` / `optional`.

### Naming limitations

- The parser is careful about namespaces, the code generator is not.
- If you use identically-named types in different namespaces, everything should lex / parse just fine.
- If you use identically-named types in different namespaces, be prepared to encounter many issues in the code generator, possibly requiring substantial re-writes.
