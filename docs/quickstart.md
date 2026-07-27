# Quickstart

## Creating a synth

A fresh `Synth` starts on Vital's init preset.

```python
import vita

synth = vita.Synth()
synth.set_bpm(120.0)
synth.set_sample_rate(48000)
```

`set_sample_rate` affects every subsequent render, so set it before rendering
rather than between calls.

## Loading and saving presets

Presets are JSON. You can go through a file or a string, and round-trip either
way.

```python
synth.load_preset("path/to/patch.vital")   # from a .vital file
json_text = synth.to_json()                # serialize current state
synth.load_json(json_text)                 # restore from a string
synth.load_init_preset()                   # back to the default patch
```

`load_preset` and `load_json` return `False` for a malformed preset, or one
saved by a newer version of Vital, rather than raising.

A `Synth` also pickles, which is what makes it usable with `multiprocessing`:

```python
import pickle

restored = pickle.loads(pickle.dumps(synth))
```

## Controls

`get_controls()` returns a dict of live handles. Setting one takes effect on the
synth immediately.

```python
controls = synth.get_controls()
controls["filter_1_on"].set(1.0)
controls["filter_1_cutoff"].set(80.0)

print(controls["filter_1_cutoff"].value())        # 80.0
print(controls["filter_1_cutoff"].get_text())     # display string
```

Every control also has a normalized 0-1 view, which is usually what you want
when sweeping parameters programmatically or feeding them from a model:

```python
controls["filter_1_cutoff"].set_normalized(0.5)
print(controls["filter_1_cutoff"].get_normalized())
```

### Control metadata

`get_control_details` describes a control's range and how it should be
presented.

```python
info = synth.get_control_details("delay_style")
print(info.min, info.max, info.default_value)
print(info.scale)         # e.g. ValueScale.Indexed
print(info.is_discrete)   # True
print(info.options)       # ["Mono", "Stereo", "Ping Pong", "Mid Ping Pong"]
print(info.display_name, info.display_units)
```

Discrete controls take integer values within `[min, max]`, and `options` gives
the label for each. Continuous controls have an empty `options` list and are set
with any float in range.

## Modulation

Sources and destinations are addressed by name. The full sets are available up
front:

```python
print(vita.get_modulation_sources())        # 'lfo_1', 'env_2', 'random_1', ...
print(vita.get_modulation_destinations())   # 'filter_1_cutoff', 'osc_1_level', ...
```

```python
synth.connect_modulation("lfo_1", "filter_1_cutoff")
synth.get_controls()["modulation_1_amount"].set(1.0)

synth.disconnect_modulation("lfo_1", "filter_1_cutoff")
synth.clear_modulations()   # remove all of them at once
```

`connect_modulation` returns `False` if every modulation slot is already in use.

## Rendering

`render` plays one note and hands back a NumPy array shaped
`(2, render_dur * sample_rate)` -- channel-major, float32.

```python
audio = synth.render(
    midi_note=60,
    midi_velocity=0.7,   # 0-1, not 0-127
    note_dur=1.0,        # seconds the note is held
    render_dur=3.0,      # seconds of audio returned, including the tail
)
```

`render_dur` should exceed `note_dur` by enough to capture the release tail;
anything still sounding at the end is faded out to avoid a click.

To write a wav directly, skipping NumPy:

```python
synth.render_file("out.wav", 60, 0.7, 1.0, 3.0)
```

Most audio libraries want frames-major data, so transpose on the way out:

```python
from scipy.io import wavfile

wavfile.write("out.wav", 48000, audio.T)
```

## Constants

Discrete controls have named values in `vita.constants`, which is easier to read
than the raw integers:

```python
from vita.constants import SyncedFrequency, Effect, FilterModel

synth.get_controls()["lfo_1_tempo"].set(SyncedFrequency.k1_16)
```

Next: [rendering many presets in parallel](parallel.md).
