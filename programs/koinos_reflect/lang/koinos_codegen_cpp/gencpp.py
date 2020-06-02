
try:
    import importlib.resources as pkg_resources
except ImportError:
    # Try backported to PY<37 `importlib_resources`.
    import importlib_resources as pkg_resources

from . import templates  # relative-import the *package* containing the templates

import jinja2

import collections
import os

class RenderError(Exception):
    pass

def classname_case_gen(name):
    it = iter(name)
    for c in it:
        if c != "_":
            yield c.lower()
            break
    for c in it:
        yield c.lower()

def classname_case(name):
    return "".join(classname_case_gen(name))

def simple_name(fqn):
    if isinstance(fqn, list):
        return fqn[-1]
    else:
        return fqn.split("::")[-1]

def fq_name(name):
    return "::".join(name)

def typeref_has_type_parameters(tref):
    # Typedef with type parameters is a base typedef.
    if tref["targs"] is None:
        return False
    for targ in tref["targs"]:
        if "value" not in targ:
            return True
    return False

def template_raise(cause):
    # Helper function to allow templates to raise exceptions
    # See https://stackoverflow.com/questions/21778252/how-to-raise-an-exception-in-a-jinja2-macro
    raise RenderError(cause)

def generate_cpp(schema):
    env = jinja2.Environment(
            loader=jinja2.PackageLoader(__package__, "templates"),
        )
    env.filters["classname_case"] = classname_case
    env.filters["simple_name"] = simple_name
    env.filters["typeref_has_type_parameters"] = typeref_has_type_parameters
    env.filters["fq_name"] = fq_name
    env.filters["tuple"] = tuple
    decls_by_name = collections.OrderedDict(((fq_name(name), decl) for name, decl in schema["decls"]))

    env.globals["raise"] = template_raise

    ctx = {"schema" : schema,
           "decls_by_name" : decls_by_name,
          }
    #for name, val in ctx["decls_by_name"].items():
    #    print(name)
    #    import json
    #    print(json.dumps(val))
    classes_j2 = env.get_template("classes.j2")
    # json_j2 = env.get_template("json.j2")
    # import json
    # print(json.dumps([decl for decl in schema["decls"] if decl[1]["info"]["type"] == "Typedef"][0]))
    result = collections.OrderedDict()
    result_files = collections.OrderedDict()
    result["files"] = result_files
    result_files["classes.hpp"] = classes_j2.render(ctx)
    # result_files["json.py"] = json_j2.render(ctx)

    rt_path = os.path.join(os.path.dirname(__file__), "rt")
    for root, dirs, files in os.walk(rt_path):
        for f in files:
            filepath = os.path.join(root, f)
            relpath = os.path.relpath(filepath, rt_path)
            with open(filepath, "r") as f:
                content = f.read()
            result_files[os.path.join("rt", relpath)] = content
    #result_files["serializers.py"] = serializers_j2.render(ctx)
    return result

def setup(app):
    app.register_target("cpp", generate_cpp)
