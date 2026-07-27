# Parallel rendering

Vita releases the GIL for the whole of `render`, `render_file`, `load_preset`,
`load_json` and `to_json`. Those calls are pure C++ DSP with no Python objects
involved, so several Python threads can be inside them at once and actually run
on separate cores.

## Give each thread its own Synth

This is the one rule that matters. Each `Synth` has its own critical section, so
sharing a single instance across threads is *safe* -- it just serializes, and you
get no parallelism at all.

```python
import threading
from concurrent.futures import ThreadPoolExecutor

import vita

_local = threading.local()


def render_preset(path):
    synth = getattr(_local, "synth", None)
    if synth is None:
        synth = _local.synth = vita.Synth()   # one Synth per worker thread
    synth.load_preset(path)
    return synth.render(60, 0.7, 1.0, 2.0)


with ThreadPoolExecutor(max_workers=8) as pool:
    audios = list(pool.map(render_preset, preset_paths))
```

Building the `Synth` inside the worker and stashing it on a `threading.local` is
what keeps it per-thread. Creating them up front and handing them out works too,
as long as no two threads ever touch the same one.

Note that constructing a `Synth` is comparatively expensive and does *not*
release the GIL, which is why the pattern above builds one per worker and reuses
it rather than making a fresh one per item.

## Why not multiprocessing?

Processes work, and there is a
[multiprocessing example](https://github.com/DBraun/Vita/tree/main/examples/multiprocessing_presets)
in the repository. Threads have three advantages here:

- **No pickling.** Rendered audio comes back as an array in the same address
  space rather than being serialized through a pipe.
- **Shared memory.** Eight worker threads share one copy of the process, instead
  of eight copies of the interpreter and everything you imported.
- **No `fork()`.** On Linux `multiprocessing` defaults to the `fork` start
  method. Forking a process that already has threads running is unsafe, so if
  you have imported something like JAX beforehand, the workers can deadlock or
  crash. Threads sidestep that entirely.

## What to expect

Speedup is real but sublinear -- the per-thread `Synth` construction, the NumPy
array handoff, and memory bandwidth all cost something. On a 24-core machine, 24
renders across 8 workers measured roughly 3x faster than the same work serially.

Renders are not bit-reproducible between runs. Vital's oscillators and random
modulators are seeded from a process-wide counter, so a given `Synth` produces
slightly different audio each time regardless of threading. If you need
deterministic output, render the tail long enough that the differences are
inaudible, or avoid random modulation sources in the preset.

## Full example

[`examples/multithreading_presets`](https://github.com/DBraun/Vita/tree/main/examples/multithreading_presets)
renders a directory of presets across a thread pool, one `Synth` per worker,
writing a wav per pitch.

```bash
python main.py --preset-dir "path/to/vital_presets" --num-workers 8
```
