"""
Demo: VST-style normalized parameter control

This example shows how to control Vital synthesizer parameters using
normalized 0-1 values, similar to how VST/AU plugins work in DAWs.
"""

import vita
from scipy.io import wavfile

SAMPLE_RATE = 44_100

def main():
    synth = vita.Synth()
    synth.set_bpm(120.0)

    # Direct control - linear interpolation of actual values
    controls = synth.get_controls()
    # Turn the filter on
    controls["filter_1_on"].set(1.0)
    
    # Example 1: Comparing direct vs normalized control (Filter 1 Cutoff)
    print("=== Comparing Direct vs Normalized Control ===")
    
    # Get info about the filter 1 cutoff parameter
    info = synth.get_control_details("filter_1_cutoff")
    print(f"Parameter: {info.display_name}")
    print(f"Range: {info.min} to {info.max}")
    print(f"Scale: {str(info.scale)}")
    print()
    
    print("Direct control (linear interpolation):")
    for pct in [0.0, 0.25, 0.5, 0.75, 1.0]:
        value = info.min + pct * (info.max - info.min)
        controls["filter_1_cutoff"].set(value)
        print(f"  {pct*100:3.0f}% → value={value:.3f}")
    
    print("\nNormalized control (VST-style):")
    for norm in [0.0, 0.25, 0.5, 0.75, 1.0]:
        controls["filter_1_cutoff"].set_normalized(norm)
        actual = controls["filter_1_cutoff"].value()
        display_text = synth.get_control_text("filter_1_cutoff")
        print(f"  {norm*100:3.0f}% → value={actual:.3f} (displays as {display_text})")
        # Render a short note
        audio = synth.render(36, 0.8, 0.9, 1)  # C2, velocity 0.8, 0.9s note, 1.0s render
        
        filename = f"filter_sweep_{int(norm*100):03d}.wav"
        wavfile.write(filename, SAMPLE_RATE, audio.T)
        print(f"Rendered {filename} with filter at {norm*100:.0f}%")


    # Example 2: Comparing direct vs normalized control (Env 1 Delay)
    print("\n=== Comparing Direct vs Normalized Control ===")
    
    # Get info about the Env 1 Delay parameter
    info = synth.get_control_details("env_1_delay")
    print(f"Parameter: {info.display_name}")
    print(f"Range: {info.min} to {info.max}")
    print(f"Scale: {str(info.scale)}")
    print()
    
    print("Direct control (linear interpolation):")
    for pct in [0.0, 0.25, 0.5, 0.75, 1.0]:
        value = info.min + pct * (info.max - info.min)
        controls["env_1_delay"].set(value)
        print(f"  {pct*100:3.0f}% → value={value:.3f}")
    
    print("\nNormalized control (VST-style):")
    print("(Note: knob position has quartic relationship to display value)")
    for norm in [0.0, 0.25, 0.5, 0.75, 1.0]:
        controls["env_1_delay"].set_normalized(norm)
        actual = controls["env_1_delay"].value()
        display_text = synth.get_control_text("env_1_delay")
        expected_display = 4.0 * (norm ** 4)
        print(f"  {norm*100:3.0f}% → value={actual:.3f} (displays as {display_text}, expected ~{expected_display:.2f}s)")
        
    # Example 3: Discrete parameter control (Delay Style)
    print("\n=== Discrete Parameter Control ===")
    
    info = synth.get_control_details("delay_style")
    print(f"Delay styles: {info.options}")
    
    # Cycle through delay styles using normalized values
    num_styles = len(info.options)
    for i in range(num_styles):
        normalized = i / (num_styles - 1) if num_styles > 1 else 0
        controls["delay_style"].set_normalized(normalized)
        style = controls["delay_style"].get_text()
        print(f"Normalized {normalized:.2f} → {style}")


if __name__ == "__main__":
    main()
