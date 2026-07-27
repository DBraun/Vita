# Vita

Python bindings for the [Vital](https://github.com/mtytel/vital) synthesizer.

Vita embeds Vital's DSP engine as a Python extension module. You load presets,
change any control, wire up modulations, and render audio straight into NumPy
arrays -- with no GUI, no audio device, and no plugin host.

```python
import vita

synth = vita.Synth()
synth.set_bpm(120.0)
synth.load_preset("my_patch.vital")

controls = synth.get_controls()
controls["filter_1_on"].set(1.0)
synth.connect_modulation("lfo_1", "filter_1_cutoff")

audio = synth.render(midi_note=60, midi_velocity=0.7, note_dur=1.0, render_dur=3.0)
print(audio.shape)  # (2, 132300) at 44.1 kHz
```

## Install

```bash
pip install vita
```

Wheels are published for CPython 3.9 through 3.14 on Linux (x86-64), macOS
(Apple silicon), and Windows (x86-64).

## Where to go next

```{toctree}
:maxdepth: 2

quickstart
parallel
api
changelog
```

- **[Quickstart](quickstart.md)** -- presets, controls, modulation, rendering.
- **[Parallel rendering](parallel.md)** -- render across threads at full speed.
- **[API reference](api.md)** -- every class and method.
- **[Changelog](changelog.md)** -- what changed, and when.

## License

Vita is GPLv3, inherited from Vital. See
[LICENSE](https://github.com/DBraun/Vita/blob/main/LICENSE).
