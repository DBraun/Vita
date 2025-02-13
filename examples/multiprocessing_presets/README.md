# Vita - Multiprocessing

This script demonstrates how to use [multiprocessing](https://docs.python.org/3/library/multiprocessing.html) to efficiently generate one-shots. The number of workers is by default `multiprocessing.cpu_count()`. Each worker has a persistent synthesizer instance. Each worker consumes paths of presets from a multiprocessing [Queue](https://docs.python.org/3/library/multiprocessing.html#pipes-and-queues). For each preset, the worker renders out audio for a configurable MIDI pitch range. The output audio path includes the pitch and preset name.

Example usage:

```bash
python main.py --preset-dir "path/to/vital_presets"
```

To see all available parameters:
```bash
python main.py --help
```
