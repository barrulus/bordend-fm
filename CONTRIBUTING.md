# Contributing to Nordend

Thanks for looking at Nordend. It is young and changing fast, and it is built with a clear idea
of what it should be: native, light, fast and safe. This page says how to help without wasting
your time.

## Before you write code

- **Bug reports are always welcome.** Open an issue with what you did, what you expected, what
  happened, and the output of `nordend --about`.
- **Small fixes: send a pull request.** A typo, a crash, a wrong result, a build problem on your
  system.
- **Features are welcome too: open an issue first.** Say what you would like and how you would
  do it, so we can agree on the shape before you write it. That way your pull request has a
  clear path to being merged, with you as the author of the commit and your name in the devlog
  and the release notes. Issues marked **good first issue** are a good place to start.
- **The core steps on the roadmap are written in-house**: the file picker, the preview engines
  and the formats they read, the sandbox, plugins, network shares. There, your ideas, issues and
  reference branches shape the work and are credited, but the code stays with the maintainer,
  so that these parts stay coherent.

## What fits Nordend

- **No toolkit, no GLib.** Nordend talks to Wayland, EGL and GLES itself. A new library is a
  decision of its own, discussed in an issue.
- **Open file formats are read by Nordend's own code**, in its caged preview engine (seccomp and
  landlock), never in the main process.
- **Plugins will be Luau only**, caged, for things outside Nordend (a local model, a service, a
  mount tool). There is no C++ plugin interface.
- **Idle means idle**: no CPU when nothing moves, and memory measured, not guessed.
- **Settings live in `config.toml`.** There is no settings window.

## A pull request

- One change per pull request, as small as it can be.
- Code: C++20, the existing style. Run `clang-format -i` on the files you touch.
- Tests: add or change them with the code (`tests/`, doctest).
- The CI builds every pull request with GCC and clang (`-Werror`), runs the tests, and runs them
  again under AddressSanitizer and UBSan. A green CI means it builds and passes the tests;
  every pull request is still read and tried by hand before it is merged, changed or declined.
- To run the same checks at home:

  ```sh
  clang-format --dry-run --Werror $(git ls-files 'src/*.cpp' 'src/*.hpp' 'tests/*.cpp' 'tests/*.hpp')
  cmake -B build -G Ninja && cmake --build build && build/tests/nordend-tests
  cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNORDEND_SANITIZER=address,undefined
  cmake --build build-asan && build-asan/tests/nordend-tests
  ```

- Docs that ship with Nordend (`README.md`, `docs/`) and the packaging are kept by the
  maintainer: leave them out of your pull request unless it is about them.

## License

By contributing you agree that your contribution is licensed under the MIT license, as Nordend
is.
