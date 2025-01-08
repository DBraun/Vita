# Vita

Vita is a Python module for interacting with the [Vital Synthesizer](https://github.com/mtytel/vital). **It is not an official product related to Vital**.

## Installation

Vita is supported on Linux, macOS, and Windows. Install with `pip`:

```bash
pip install vita
```

## Example

```python
from scipy.io import wavfile
import vita

SAMPLE_RATE = 44_100

bpm = 120.0
note_dur = 1.0
render_dur = 3.0
pitch = 36  # integer
velocity = 0.7  # [0.0 to 1.0]

synth = vita.Synth()
# The initial preset is loaded by default.

synth.set_bpm(bpm)

# Let's make a custom modulation using
# the available modulation sources and destinations.
# These lists are constant.
print("potential sources:", vita.get_modulation_sources())
print("potential destinations:", vita.get_modulation_destinations())

# "lfo_1" and "filter_1_cutoff" are potential sources and destinations.
assert synth.connect_modulation("lfo_1", "filter_1_cutoff")

controls = synth.get_controls()
controls["modulation_1_amount"].set(1.0)
controls["filter_1_on"].set(1.0)
controls["lfo_1_tempo"].set(vita.constants.SyncedFrequency.k1_16)

# Render audio to numpy array shaped (2, NUM_SAMPLES)
audio = synth.render(pitch, velocity, note_dur, render_dur)

wavfile.write("generated_preset.wav", SAMPLE_RATE, audio.T)

# Dump current state to JSON text
preset_path = "generated_preset.vital"

json_text = synth.to_json()
with open(preset_path, "w") as f:
    f.write(json_text)

# Load JSON text
with open(preset_path, "r") as f:
    json_text = f.read()
    assert synth.load_json(json_text)

# Or load directly from file
assert synth.load_preset(preset_path)

# Load the initial preset, which also clears modulations
synth.load_init_preset()
# Or just clear modulations.
synth.clear_modulations()
```

### Issues

If you find any issues with the code, report them at https://github.com/DBraun/Vita.

### Code Licensing
If you are making a proprietary or closed source app and would like to use Vital's source code, contact licensing@vital.audio for non GPLv3 licensing options.

### What can you do with the source
The source code is licensed under the GPLv3. If you download the source or create builds you must comply with that license.

### Things you can't do with this source
 - Do not create an app and distribute it on the iOS app store. The app store is not comptabile with GPLv3 and you'll only get an exception for this if you're paying for a GPLv3 exception for Vital's source (see Code Licensing above).
 - Do not use the name "Vital", "Vital Audio", "Tytel" or "Matt Tytel" for marketing or to name any distribution of binaries built with this source. This source code does not give you rights to infringe on trademarks.
 - Do not connect to any web service at https://vital.audio, https://account.vital.audio or https://store.vital.audio from your own builds. This is against the terms of using those sites.
 - Do not distribute the presets that come with the free version of Vital. They're under a separate license that does not allow redistribution.
