
from . import parser
from . import baseparser

from dataclasses_json import dataclass_json

from dataclasses import dataclass, field
from typing import List, Tuple, Union
from collections import deque, OrderedDict

from .parser import *

class AnalyzeError(Exception):
    pass

class TreeListener:
    def process_toplevel(self, node): yield
    def process_typedef(self, node): yield
    def process_struct(self, node): yield
    def process_ns(self, node): yield
    def process_enum(self, node): yield
    def process_basetype(self, node): yield
    def process_field(self, node): yield
    def process_typeref(self, node): yield
    def process_intliteral(self, node): yield
    def process_enum_entry(self, node): yield

def walk(listener, node):
    if isinstance(node, parser.Toplevel):
        f = listener.process_toplevel
        subnodes = node.decls
    elif isinstance(node, parser.Typedef):
        f = listener.process_typedef
        subnodes = [node.tref]
    elif isinstance(node, parser.Struct):
        f = listener.process_struct
        subnodes = node.fields
    elif isinstance(node, parser.Namespace):
        f = listener.process_ns
        subnodes = node.decls
    elif isinstance(node, parser.EnumClass):
        f = listener.process_enum
        subnodes = node.entries
    elif isinstance(node, parser.BaseType):
        f = listener.process_basetype
        subnodes = []
    elif isinstance(node, parser.Field):
        f = listener.process_field
        subnodes = [node.tref]
    elif isinstance(node, parser.Typeref):
        f = listener.process_typeref
        subnodes = node.targs
        if subnodes is None:
            subnodes = []
    elif isinstance(node, parser.IntLiteral):
        f = listener.process_intliteral
        subnodes = []
    elif isinstance(node, parser.EnumEntry):
        f = listener.process_enum_entry
        subnodes = []
    else:
        raise AnalyzeError("Unknown node type")
    # Pre-node:  Run until yield statement
    it = f(node)
    next(it)
    for sn in subnodes:
        walk(listener, sn)

    # Post-node:  Continue, expect StopIteration
    try:
        next(it)
        raise AnalyzeError("Multiple yields while processing node")
    except StopIteration:
        pass

class NamespaceStacker(TreeListener):
    def __init__(self):
        super().__init__()
        self.namespace_stack = []

    def process_ns(self, node):
        self.namespace_stack.append(node.name)
        yield
        self.namespace_stack.pop()

class TypemapBuilder(NamespaceStacker):
    def __init__(self):
        super().__init__()
        self.typemap = OrderedDict()

    def add_to_typemap(self, name, node):
        name = tuple(name)
        if name in self.typemap:
             raise AnalyzeError("Multiple definition of {}".format(name))
        self.typemap[name] = node

    def process_basetype(self, node):
        self.add_to_typemap(node.name, node)
        yield

    def process_typedef(self, node):
        self.add_to_typemap(self.namespace_stack + [node.name], node)
        yield

    def process_struct(self, node):
        self.add_to_typemap(self.namespace_stack + [node.name], node)
        yield

    def process_enum(self, node):
        self.add_to_typemap(self.namespace_stack + [node.name], node)
        yield

class ReferenceChecker(NamespaceStacker):
    def __init__(self, typemap):
        super().__init__()
        self.typemap = typemap

    def search(self, name):
        # Search for a qualified name in each enclosing namespace
        ns = list(self.namespace_stack)
        while True:
            fqn = tuple(ns + name)
            if fqn in self.typemap:
                return fqn
            if len(ns) == 0:
                return None
            ns.pop()

    def process_typeref(self, node):
        fqn = self.search(node.name)
        if fqn is None:
            raise AnalyzeError("Unknown type {}", "::".join(node.name))
        node.name = fqn
        yield

# Immediate dependencies of a node
class FindDepsWalker(TreeListener):
    def __init__(self, typemap=None):
        super().__init__()
        self.typemap = typemap
        self.dep_set = set()
        self.dep_list = []

    def process_typeref(self, node):
        name = tuple(node.name)
        if name not in self.dep_set:
            self.dep_set.add(name)
            self.dep_list.append(name)
        yield

def find_immediate_deps(typemap, name):
    finder = FindDepsWalker(typemap)
    walk(finder, typemap[name])
    return finder.dep_list

class TopologicalSorter:
    def __init__(self, deps, max_depth):
        self.deps = deps
        self.max_depth = max_depth
        self.emitted_set = set()
        self.emitted_list = list()

    def process_item(self, item, depth=0):
        if item in self.emitted_set:
            return
        next_depth = depth+1
        for d in self.deps(item):
            if d in self.emitted_set:
                continue
            if next_depth >= self.max_depth:
                raise AnalyzeError("Topological sort depth exceeded, circular dependency on {}".format(item))
            self.process_item(d, next_depth)
            self.emit(d)
        self.emit(item)

    def emit(self, item):
        if item not in self.emitted_set:
            self.emitted_set.add(item)
            self.emitted_list.append(item)

def topological_sort(items, deps, max_depth=20):
    #
    # Topological sort:  Given a set of items, and a function deps(item)
    # which gives the dependencies of an item, sort the items such that an item's dependencies precede it.
    #
    sorter = TopologicalSorter(deps, max_depth)
    for item in items:
        sorter.process_item(item)
    return sorter.emitted_list

@dataclass_json
@dataclass
class Schema:
    decls: List[Tuple[List[str], parser.Decl]]
    info: OrderedDict = field(default_factory=parser.info_factory("Schema"))

class Schemanator:
    def __init__(self, tree=()):
        self.tree = tree
        self.typemap = OrderedDict()
        self.schema = None

    def build_typemap(self):
        builder = TypemapBuilder()
        walk(builder, self.tree)
        self.typemap = builder.typemap

    def check_references(self):
        checker = ReferenceChecker(self.typemap)
        walk(checker, self.tree)

    def find_types(self):
        # Not actually used, demo method to demonstrate how to process all typeref nodes
        listener = TreeListener()
        typerefs = []
        def process_typeref(node):
            typerefs.append(node.name)
            yield
        listener.process_typeref = process_typeref
        walk(listener, self.tree)
        print("typerefs:", typerefs)

    def sort_typemap(self):
        finder = FindDepsWalker(self.typemap)
        tuple_names = [tuple(name) for name in self.typemap.keys()]
        deps = lambda x : find_immediate_deps(self.typemap, x)
        sorted_tuple_names = topological_sort(tuple_names, deps)
        new_typemap = OrderedDict()
        for name in sorted_tuple_names:
            new_typemap[name] = self.typemap[name]
        self.typemap = new_typemap

    def create_schema(self):
        decls = []
        for k, v in self.typemap.items():
            decls.append((list(k), v))
        self.schema = Schema(decls=decls)

    def schemanate(self):
        self.build_typemap()
        self.check_references()
        self.sort_typemap()
        self.create_schema()
