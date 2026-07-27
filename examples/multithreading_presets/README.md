# Vita - Multithreading

This script renders many Vital presets in parallel using **threads** instead of
processes. Vita releases the Python GIL during the DSP-heavy render (and during
`load_preset` / `load_json` / `to_json` / `render_file`), so multiple Python
threads render in true parallel with no process-spawn or pickling overhead and
with shared memory.

Each worker thread keeps its **own** persistent `vita.Synth` (via
`threading.local`). Sharing a single `Synth` across threads is safe — its
critical section serializes concurrent calls — but it buys you nothing, because
only one thread renders at a time.

## Why threads instead of `multiprocessing`?

On Linux, `multiprocessing` defaults to the `fork` start method, which is what
the [`multiprocessing`](../multiprocessing_presets) example relies on. If your
program has already imported a library that starts background threads before
forking — most notably **JAX** — `fork()` in a multithreaded process is unsafe
and the workers can deadlock or crash. Because this example never forks, it
works fine in that scenario.

Threads also avoid pickling the audio back to the parent process and let every
worker share one copy of the loaded presets in memory.

Example usage:

```bash
python main.py --preset-dir "path/to/vital_presets"
```

To see all available parameters:

```bash
python main.py --help
```
