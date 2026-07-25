# Vita - Multithreading

This script renders many Vital presets in parallel using **threads** instead of
processes. Vita releases the Python GIL during the DSP-heavy render (and during
`load_preset` / `load_json` / `to_json` / `render_file`), so multiple Python
threads render in true parallel with no process-spawn or pickling overhead and
with shared memory.

Each worker thread keeps its **own** persistent `vita.Synth` (via
`threading.local`). A `Synth` must not be shared across threads: its per-instance
critical section would serialize concurrent renders, and its engine state is not
meant to be mutated from multiple threads at once.

## Why threads instead of `multiprocessing`?

The [`multiprocessing`](../multiprocessing_presets) example spawns worker
processes with `fork()`. If your program has already imported a library that
starts background threads before forking — most notably **JAX** — `fork()` in a
multithreaded process is unsafe and the workers can deadlock or crash. Because
this example never forks, it works fine in that scenario.

Example usage:

```bash
python main.py --preset-dir "path/to/vital_presets"
```

To see all available parameters:

```bash
python main.py --help
```
