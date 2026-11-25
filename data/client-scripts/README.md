# data/client-scripts

This directory holds the client scripts that are shipped with Neutrino
itself: Lua and Python scripts that are started from outside, from the
shell, a shell script or NI-Web, and run inside the running Neutrino with
its full script API. They are tied to the bindings in `src/gui/lua` and
`src/gui/python` and change together with them.

Plugins that are started from Neutrino's menus do not belong here; they
live in [ni-neutrino-plugins](https://github.com/neutrino-images/ni-neutrino-plugins).

    data/client-scripts/
    ├── luaclient/   Lua client scripts      -> LUACLIENTSCRIPTDIR  (ENABLE_LUA)
    │   └── <name>/
    └── pyclient/    Python client scripts   -> PYCLIENTSCRIPTDIR   (ENABLE_PYTHON)
        └── <name>/

One directory per client, named after the client that runs its scripts and
after the directory on the box it installs into, and one directory per
script inside it. Every script installs its own files from its own
`Makefile.am`, and it installs them straight into the target directory, not
into a subdirectory of its own name.

## How a client script runs

|                 | Lua                                                | Python                                            |
|-----------------|----------------------------------------------------|---------------------------------------------------|
| installed into  | `$(LUACLIENTSCRIPTDIR)`                                  | `$(PYCLIENTSCRIPTDIR)`                                  |
| on the box      | `/usr/share/tuxbox/neutrino/client-scripts/luaclient` | `/usr/share/tuxbox/neutrino/client-scripts/pyclient` |
| user override   | `$(LUACLIENTSCRIPTDIR_VAR)`: `/var/tuxbox/client-scripts/luaclient` | `$(PYCLIENTSCRIPTDIR_VAR)`: `/var/tuxbox/client-scripts/pyclient` |
| shebang         | `#!/usr/bin/luaclient`                             | `#!/usr/bin/pyclient`                             |
| server          | `CLuaServer` (`src/system/luaserver.cpp`)          | `CPythonServer` (`src/system/pyserver.cpp`)       |
| arguments       | `arg[2]`, `arg[3]`, ...                            | `sys.argv[2]`, `sys.argv[3]`, ...                 |
| result          | `return "..."` at the end                          | the global `result` (a `str`)                     |

`luaclient` and `pyclient` send the name of the script and its arguments
through a socket (`/tmp/luaclient.sock`, `/tmp/pyclient.sock`) to Neutrino.
A name is looked for as `<name>.lua` or `<name>.py`, first in the `_VAR`
directory and then in the shipped one; an absolute path is taken as it is.
Neutrino runs the script, and the client prints the result to stdout and
errors to stderr. If the client goes away, the script is aborted.

A client script has no `.cfg` and does not appear in any menu. In a script,
the directories are `DIR.LUACLIENTSCRIPTDIR`, `DIR.LUACLIENTSCRIPTDIR_VAR`,
`DIR["PYCLIENTSCRIPTDIR"]` and `DIR["PYCLIENTSCRIPTDIR_VAR"]`.

Both can be called by name or by path:

    luaclient nlab get dir.plugindir
    pyclient npab get dir.plugindir
    /usr/share/tuxbox/neutrino/client-scripts/luaclient/nlab.lua get apiversion

## What is here

| path | purpose |
|---|---|
| `luaclient/nlab` | Neutrino Lua API bridge: hands values of the Lua API to the shell |
| `pyclient/npab` | Neutrino Python API bridge: the Python counterpart of `nlab` |

## Adding a script

1. Create `luaclient/<name>/` or `pyclient/<name>/` with the script and a
   `Makefile.am`:

       installdir = $(PYCLIENTSCRIPTDIR)

       install_SCRIPTS = \
       	<name>.py

   Use `$(LUACLIENTSCRIPTDIR)` for Lua. `install_SCRIPTS` keeps the script
   executable, so that it can be run directly through its shebang.
2. Add `<name>` to `SUBDIRS` in `luaclient/Makefile.am` or `pyclient/Makefile.am`.
3. Add `data/client-scripts/<luaclient|pyclient>/<name>/Makefile` to
   `AC_CONFIG_FILES` in `configure.ac`.

Do not commit generated files such as `__pycache__/` or `*.pyc`.
