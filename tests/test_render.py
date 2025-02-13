from scipy.io import wavfile

import vita
from vita.constants import (
    SourceDestination,
    Effect,
    FilterModel,
    RetriggerStyle,
    SpectralMorph,
    DistortionType,
    UnisonStackType,
    RandomLFOStyle,
    SynthFilterStyle,
    WaveShape,
    CompressorBandOption,
    VoiceOverride,
    VoicePriority,
    SynthLFOSyncOption,
    SynthLFOSyncType,
    SyncedFrequency,
)

SAMPLE_RATE = 44_100


def test_render(bpm=120.0, note_dur=1.0, render_dur=3.0, pitch=36, velocity=0.7):

    synth = vita.Synth()
    # The initial preset is laoded by default.

    synth.set_bpm(bpm)

    assert vita.get_modulation_sources()
    assert vita.get_modulation_destinations()

    # Custom synthesizer settings
    assert synth.connect_modulation("lfo_1", "filter_1_cutoff")
    controls = synth.get_controls()
    controls["modulation_1_amount"].set(1.0)
    controls["filter_1_on"].set(1.0)
    controls["lfo_1_tempo"].set(SyncedFrequency.k1_16)

    # Render audio to numpy array shaped (2, NUM_SAMPLES)
    audio = synth.render(pitch, velocity, note_dur, render_dur)

    wavfile.write("generated_preset.wav", SAMPLE_RATE, audio.T)

    # Dump current state to json text
    json_text = synth.to_json()
    preset_path = "generated_preset.vital"
    with open(preset_path, "w") as f:
        f.write(json_text)

    # Load JSON text
    with open(preset_path, "r") as f:
        json_text1 = f.read()
        assert synth.load_json(json_text1)

    assert json_text == json_text1

    # Or load directly from file:
    assert synth.load_preset(preset_path)

    # Load the initial preset, which also clears modulations
    synth.load_init_preset()
    # Or just clear modulations.
    synth.clear_modulations()
