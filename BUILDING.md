Building sailjail
=================
Sailjail uses [Meson build system](https://mesonbuild.com/). To build it
use the following commands:

    meson setup build
    meson compile -C build

See [the next section](#useful-setup-options) for more tips.

Useful setup options
--------------------
Some useful options for `meson setup`:

| option                   | explanation                             |
| -----------------------: | :-------------------------------------  |
| -Db\_coverage=true       | Build with support for coverage reports |
| -Dlibdbusaccess=disabled | Build without libdbusaccess support     |

Running unit tests
------------------
Unit tests can be run with meson:

    meson test -C build

After that you can also generate a coverage report if you built with
coverage reports support:

    ninja coverage -C build

It will print you URI to the generated report, assuming that you have
the commands to build the reports in your system.

Sailjail daemon diagrams
------------------------
There is an architecture diagram generated with graphviz and a png
version of it can be generated with `sailjaild.png` target. You can find
it in _build/daemon/_ after building it:

    meson compile sailjaild.png -C build

Similarly a dataflow diagram can be built with `dataflow.png` target
and prompter state diagram with `prompter.png` target.

Cheatsheet for make users
-------------------------
For those that come from the world of makefiles:

| make command               | meson equivalent                              |
| :------------------------- | :-------------------------------------------- |
| Initial setup              | `mkdir build && cd build && meson setup .. .` |
| `make`                     | `meson compile`                               |
| `make install`             | `meson install`                               |
| `make install DESTDIR=foo` | `DESTDIR=foo meson install`                   |
| `make run-tests`           | `meson test`                                  |
| `make coverage`            | `meson setup .. . -Db_coverage=true`<br>`meson test`<br>`ninja coverage` |

Meson doesn't support building inside source directory. Here we create a
build directory and run the commands inside that. Alternatively you may
use `-C build` argument to specify the build directory.
