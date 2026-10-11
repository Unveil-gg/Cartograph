# Agent guide

[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Surgical diffs only; match surrounding code. No `third_party/` edits.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build && ctest --test-dir build --output-on-failure
```

After code edits: end with a 1–2 sentence imperative commit suggestion; commit only if asked.
