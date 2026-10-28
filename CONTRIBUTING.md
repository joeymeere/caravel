# Contributing

Caravel is experimental, and some aspects of the API are subject to change in future iterations. If any of the following are true, please open an issue:

- **You think a feature is missing** — describe the use case, not just the solution
- **A bug is found** — include a minimal reproduction and the error output
- **You found an optimization** — provide an implementation reference and reproducible benchmarks

## Running the checks

Everything builds from the root `Makefile` into `build/`. CI runs all of these
on every pull request (`make check` runs them in one go):

```sh
make format-check     # clang-format (`make format` to fix)
make cli WERROR=1     # CLI, warnings as errors
make test-host        # header unit tests on the host, with ASan + UBSan
make test-compile     # compile probe across SBPF versions, C standards, options
make test-examples    # `caravel test` every example (SBF_VER=v3 for v3)
make bench            # example tests + compute-unit / size regression check
```

`test-compile`, `test-examples` and `bench` need platform-tools; install a
release where `caravel build` looks for it with `make platform-tools`
(`PLATFORM_TOOLS=v1.57` by default).

Formatting is checked with clang-format 22.1.8 (other versions can format
slightly differently). Get that exact version with
`pip install clang-format==22.1.8` (or `pipx run clang-format==22.1.8`) and
point the Makefile at it, e.g. `make format CLANG_FORMAT=/path/to/clang-format`.

If a change intentionally moves compute units or binary size, refresh the
baseline with `make bench-update` and commit `etc/baseline.json` alongside
the change.
