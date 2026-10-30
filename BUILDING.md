# Building utf99

The [README](README.md#building) covers the default build. Run the commands
below from the repository root.

## Installation

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/install/prefix
cmake --build build
cmake --install build
```

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries instead of static libraries. |
| `UTF99_BUILD_TESTING` | `ON` for standalone builds | Build and register the tests with CTest. |
| `UTF99_USE_SYSTEM_SIO99` | `OFF` | Find installed sio99 through pkg-config when no `sio99` target exists. |

Set options when configuring, for example:

```sh
cmake -S . -B build -DBUILD_SHARED_LIBS=ON -DUTF99_BUILD_TESTING=OFF
```

## Dependency integration

The default build uses a pinned sio99 Git submodule. No separate dependency
build or installation is required.

For an existing clone, populate the dependency with
`git submodule update --init --recursive`.

CMake resolves sio99 in this order:

- Reuse an existing `sio99` target supplied by a parent project.
- If `UTF99_USE_SYSTEM_SIO99=ON`, find installed sio99 through pkg-config.
- Otherwise, build the bundled submodule.

The bundled build defaults sio99 tests and optional stdio/heap adapters to
OFF unless the parent has explicitly configured them. `utf99` links sio99
publicly, so consumers receive its include paths and link dependency.

A parent project sharing sio99 with other libraries can add it once:

```cmake
add_subdirectory(src/sio99)
add_subdirectory(src/utf99)
target_link_libraries(my_application PRIVATE utf99)
```

For an installed dependency:

```sh
cmake -S . -B build -DUTF99_USE_SYSTEM_SIO99=ON
```

Use `PKG_CONFIG_PATH` for a nonstandard sio99 installation prefix. Installing
a bundled build also installs sio99. Installing with an existing or system
dependency leaves that dependency's installation to its owner. Installed
consumers can use `pkg-config --cflags --libs utf99`.

