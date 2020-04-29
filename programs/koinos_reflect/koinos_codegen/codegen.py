
import pluginbase

import argparse
import json
import os
import sys

plugin_base = pluginbase.PluginBase(package="koinos_codegen.plugins")
# TODO:  search path for builtin plugins?

class Application:
    def __init__(self, name="koinos_codegen", plugin_searchpath=[]):
        self.name = name
        self.targets = {}
        self.plugin_source = plugin_base.make_plugin_source(
            searchpath=plugin_searchpath,
            identifier=self.name,
            )
        for plugin_name in self.plugin_source.list_plugins():
            plugin = self.plugin_source.load_plugin(plugin_name)
            plugin.setup(self)

    def register_target(self, name, target):
        self.targets[name] = target

class ApplicationError(Exception):
    pass

def main(argv):
    argparser = argparse.ArgumentParser(description="Analyze files")

    argparser.add_argument("schema", metavar="FILE", nargs="*", type=str, help="Schema file(s)")
    argparser.add_argument("-t", "--target", metavar="LANG", default="python", type=str, help="Target language (default=python)")
    argparser.add_argument("--target-path", metavar="DIRS", action="append", type=str, help="Search path for language target definitions")
    argparser.add_argument("--list-targets", action="store_true", help="List available language definitions and exit")
    argparser.add_argument("-p", "--package", metavar="PKG", default="", type=str, help="Output package name")
    argparser.add_argument("-o", "--output", metavar="DIR", default="build", type=str, help="Output directory")
    args = argparser.parse_args(argv)

    if args.package == "":
        raise ApplicationError("Required: package")

    package_dir = os.path.join(args.output, args.package)
    # if os.path.exists(package_dir):
    #     raise ApplicationError("Package dir must not exist")
    os.makedirs(package_dir, exist_ok=True)

    target_path = args.target_path
    if target_path is None:
        target_path = []
    app = Application(plugin_searchpath=target_path)

    try:
        if args.list_targets:
            for plugin_name in app.targets:
                print(plugin_name)
            return 0
        if len(args.schema) != 1:
            raise RuntimeError("Must specify exactly one input file")
        generate_target = app.targets[args.target]
        with open(args.schema[0], "r") as f:
            schema = json.load(f)
        generated = generate_target(schema)
        for filename, content in generated["files"].items():
            target_filename = os.path.join(package_dir, filename)
            target_dir = os.path.dirname(target_filename)
            os.makedirs(target_dir, exist_ok=True)
            with open(target_filename, "w") as f:
                f.write(content)
    finally:
        pass

if __name__ == "__main__":
    result = main(sys.argv[1:])
    sys.exit(result)
