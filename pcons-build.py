#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pcons>=0.24"]
# ///
"""Pcons build for MXParser.

Install MXLex and Argz first, then build with::

    uvx pcons -B build/pcons --reconfigure

For dependencies installed outside standard system locations, pass their
common or path-separated prefixes with ``PREFIX=/path/to/prefix``.

Other options are ``VARIANT=debug``, ``PROGRAMS=0``,
``PCONS_INSTALL_PREFIX=<stage-prefix>``, and
``PCONS_FINAL_PREFIX=<runtime-prefix>``.
"""

import os
from pathlib import Path

from pcons import (
    ImportedTarget,
    PackageDescription,
    Project,
    find_c_toolchain,
    get_platform,
    get_var,
    write_file,
)
from pcons.core.subst import SourcePath, TargetPath

VERSION = "0.1.0"

project_dir = Path(__file__).parent.resolve()
platform = get_platform()


def option(name: str, default: bool = False) -> bool:
    """Read an ON/OFF build option."""
    return get_var(name, "1" if default else "0").lower() in (
        "1",
        "on",
        "true",
        "yes",
    )


extra_prefixes = [
    Path(prefix).expanduser()
    for prefix in (get_var("PREFIX") or "").split(os.pathsep)
    if prefix
]
if extra_prefixes:
    os.environ["PKG_CONFIG_PATH"] = os.pathsep.join(
        [str(prefix / "lib" / "pkgconfig") for prefix in extra_prefixes]
        + [os.environ.get("PKG_CONFIG_PATH", "")]
    )

search_prefixes = extra_prefixes + [Path("/usr/local"), Path("/usr")]

project = Project("MXParser", root_dir=project_dir)
env = project.Environment(toolchain=find_c_toolchain())
env.cxx.set_standard(23)
env.set_variant(get_var("VARIANT", "release"))
env.cxx.flags.extend(["-Wall", "-Wextra", "-pedantic"])

if platform.is_linux:
    env.install.copycmd = ["install", "-D", SourcePath(), TargetPath()]


def imported_header(name: str, include_dir: Path) -> ImportedTarget:
    """Create a header-only imported dependency."""
    return ImportedTarget.from_package(
        PackageDescription(name=name, include_dirs=[str(include_dir)])
    )


def find_mxlex():
    """Find an installed MXLex package."""
    package = project.find_package("mxlex", required=False)
    if package is not None:
        return package

    for prefix in search_prefixes:
        header = prefix / "include" / "MXLex" / "token.hpp"
        libraries = [prefix / "lib" / "libmxlex.a", prefix / "lib64" / "libmxlex.a"]
        library = next((path for path in libraries if path.is_file()), None)
        if header.is_file() and library is not None:
            return ImportedTarget.from_package(
                PackageDescription(
                    name="mxlex",
                    include_dirs=[str(prefix / "include")],
                    library_dirs=[str(library.parent)],
                    libraries=["mxlex"],
                )
            )

    raise SystemExit(
        "MXLex was not found. Install MXLex first and pass "
        "PREFIX=/path/to/prefix when it is outside a standard location."
    )


def find_argz():
    """Find an installed Argz public header."""
    for prefix in search_prefixes:
        if (prefix / "include" / "Argz" / "argz.hpp").is_file():
            return imported_header("argz", prefix / "include")

    raise SystemExit(
        "Argz was not found. Install Argz first and pass "
        "PREFIX=/path/to/prefix when it is outside a standard location."
    )


mxlex = find_mxlex()
argz = find_argz()

mxparser = project.StaticLibrary(
    "mxparser",
    env,
    sources=[
        project_dir / "source" / "parser.cpp",
        project_dir / "source" / "sym_tab.cpp",
        project_dir / "source" / "ast.cpp",
    ],
)
mxparser.public.include_dirs.append(project_dir)
mxparser.link(mxlex)

if option("PROGRAMS", default=True):
    test_parser = project.Program(
        "test-parser", env, sources=[project_dir / "test.cpp"]
    )
    parser_test = project.Program(
        "parser-test", env, sources=[project_dir / "parser-test.cpp"]
    )
    parse_expr = project.Program(
        "parse-expr", env, sources=[project_dir / "examples" / "expr.cpp"]
    )
    test_parser.link(mxparser)
    parser_test.link(mxparser, argz)
    parse_expr.link(mxparser)

final_prefix = Path(
    get_var(
        "PCONS_FINAL_PREFIX",
        get_var("PCONS_INSTALL_PREFIX", str(project_dir / "dist")),
    )
)
pc_file = project.build_dir / "mxparser.pc"
write_file(
    pc_file,
    f"prefix={final_prefix}\n"
    "exec_prefix=${prefix}\n"
    "libdir=${prefix}/lib\n"
    "includedir=${prefix}/include\n\n"
    "Name: mxparser\n"
    "Description: C++23 parser library\n"
    f"Version: {VERSION}\n"
    "Requires: mxlex\n"
    "Libs: -L${libdir} -lmxparser\n"
    "Cflags: -I${includedir}\n",
)
public_headers = sorted((project_dir / "MXParser").glob("*.hpp"))

project.Alias(
    "install",
    project.Install("lib", [mxparser], mode=0o644),
    project.Install("include/MXParser", public_headers, mode=0o644),
    project.Install("lib/pkgconfig", [pc_file], mode=0o644),
)
