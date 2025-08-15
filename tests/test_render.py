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


def test_render(bpm=120.0, sample_rate=48000, note_dur=1.0, render_dur=3.0, pitch=36, velocity=0.7):

    synth = vita.Synth()
    # The initial preset is laoded by default.

    synth.set_bpm(bpm)
    synth.set_sample_rate(sample_rate)

    assert vita.get_modulation_sources()
    assert vita.get_modulation_destinations()

    # Custom synthesizer settings
    assert synth.connect_modulation("lfo_1", "filter_1_cutoff")
    controls = synth.get_controls()
    controls["modulation_1_amount"].set(1.0)
    controls["filter_1_on"].set(1.0)
    assert 1.0 == controls["filter_1_on"].value()
    controls["lfo_1_tempo"].set(SyncedFrequency.k1_16)

    # Render audio to numpy array shaped (2, NUM_SAMPLES)
    audio = synth.render(pitch, velocity, note_dur, render_dur)
    assert sample_rate == int(audio.shape[1] / render_dur) # assume int

    wavfile.write("generated_preset.wav", sample_rate, audio.T)

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

    info = synth.get_control_details("delay_style")
    print("min:", info.min)
    print("max:", info.max)
    print("default_value:", info.default_value)
    print("scale:", info.scale)           # e.g. ValueScale.Indexed
    print("discrete:", info.is_discrete)  # True
    print("options: ", info.options)      # ["Mono","Stereo","Ping Pong","Mid Ping Pong"]
    print("display_name:", info.display_name)
    print("display_units:", info.display_units)

    controls["delay_style"].set(0)
    text = synth.get_control_text("delay_style")
    assert text == info.options[0]
    print("text: ", text)
    controls["delay_style"].set(1)
    text = synth.get_control_text("delay_style")
    assert text == info.options[1]
    print("text: ", text)

    assert info.is_discrete
    for i in range(int(info.min), int(info.max)+1):
        controls["delay_style"].set(i)

    info = synth.get_control_details("env_1_delay")
    print("min:", info.min)
    print("max:", info.max)
    print("default_value:", info.default_value)
    print("scale:", info.scale)           # e.g. ValueScale.Quartic
    print("discrete:", info.is_discrete)  # False
    print("options: ", info.options)      # [""]

    assert not info.is_discrete
    steps = 100
    for i in range(steps):
        pct = i/(steps-1)
        # linearly interpolate
        y = info.min + pct * (info.max-info.min)
        controls["env_1_delay"].set(y)
