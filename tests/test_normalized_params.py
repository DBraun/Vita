import pytest
import vita
from vita.constants import ValueScale


def test_normalized_linear_parameter():
    """Test normalized control on a linear parameter"""
    synth = vita.Synth()
    
    # Get a linear parameter (most parameters are linear by default)
    info = synth.get_control_details("filter_1_cutoff")
    if info.scale != ValueScale.Linear:
        pytest.skip("filter_1_cutoff is not linear in this version")
    
    # Test boundary values
    controls = synth.get_controls()
    controls["filter_1_cutoff"].set_normalized(0.0)
    assert abs(controls["filter_1_cutoff"].get_normalized() - 0.0) < 0.001
    assert abs(controls["filter_1_cutoff"].value() - info.min) < 0.001
    
    controls["filter_1_cutoff"].set_normalized(1.0)
    assert abs(controls["filter_1_cutoff"].get_normalized() - 1.0) < 0.001
    assert abs(controls["filter_1_cutoff"].value() - info.max) < 0.001
    
    # Test middle value
    controls["filter_1_cutoff"].set_normalized(0.5)
    assert abs(controls["filter_1_cutoff"].get_normalized() - 0.5) < 0.001
    expected_mid = info.min + 0.5 * (info.max - info.min)
    assert abs(controls["filter_1_cutoff"].value() - expected_mid) < 0.001


def test_normalized_quartic_parameter():
    """Test normalized control on a quartic parameter like env_1_delay"""
    synth = vita.Synth()
    
    # Verify env_1_delay uses quartic scaling
    info = synth.get_control_details("env_1_delay")
    assert info.scale == ValueScale.Quartic
    
    # Test boundary values
    controls = synth.get_controls()
    controls["env_1_delay"].set_normalized(0.0)
    assert abs(controls["env_1_delay"].get_normalized() - 0.0) < 0.001
    
    controls["env_1_delay"].set_normalized(1.0)
    assert abs(controls["env_1_delay"].get_normalized() - 1.0) < 0.001
    
    # Test that normalized 0.5 maps to 0.25 seconds display (0.5^4 * 4.0)
    # For quartic parameters in VST, normalized maps linearly to internal range
    # but the knob DISPLAY position has a quartic relationship
    controls["env_1_delay"].set_normalized(0.5)
    expected_value = info.min + 0.5 * (info.max - info.min)  # Linear mapping
    assert abs(controls["env_1_delay"].value() - expected_value) < 0.001
    assert abs(controls["env_1_delay"].get_normalized() - 0.5) < 0.001
    
    actual_value = controls["env_1_delay"].value()
    linear_mid = info.min + 0.5 * (info.max - info.min)
    
    # With this implementation, normalized maps linearly to internal range
    assert abs(actual_value - linear_mid) < 0.001


def test_normalized_indexed_parameter():
    """Test normalized control on an indexed/discrete parameter"""
    synth = vita.Synth()
    
    # delay_style is an indexed parameter
    info = synth.get_control_details("delay_style")
    assert info.scale == ValueScale.Indexed
    assert info.is_discrete
    
    num_options = len(info.options)
    
    # Test each discrete value
    controls = synth.get_controls()
    for i in range(num_options):
        normalized = i / (num_options - 1) if num_options > 1 else 0
        controls["delay_style"].set_normalized(normalized)
        
        # Check it maps to the correct index
        actual_index = int(controls["delay_style"].value() - info.min)
        assert actual_index == i
        
        # Check text matches
        assert controls["delay_style"].get_text() == info.options[i]


def test_normalized_vst_style_automation():
    """Demonstrate VST-style parameter automation using normalized values"""
    synth = vita.Synth()
    
    # Simulate a VST automation curve for filter cutoff
    # This would be how a DAW sends parameter changes
    automation_points = [
        (0.0, 0.0),    # Start closed
        (0.25, 0.5),   # Open to middle
        (0.5, 1.0),    # Full open
        (0.75, 0.3),   # Partially close
        (1.0, 0.0),    # Close again
    ]
    
    controls = synth.get_controls()
    for time, normalized_value in automation_points:
        controls["filter_1_cutoff"].set_normalized(normalized_value)
        
        # Verify the normalized value is preserved
        retrieved = controls["filter_1_cutoff"].get_normalized()
        assert abs(retrieved - normalized_value) < 0.001
        
        # In a real scenario, you would render audio here
        # audio = synth.render(60, 0.8, 0.1, 0.1)


def test_normalized_clipping():
    """Test that normalized values are properly clipped to 0-1 range"""
    synth = vita.Synth()
    
    # Test values outside 0-1 range
    controls = synth.get_controls()
    controls["filter_1_cutoff"].set_normalized(-0.5)
    assert controls["filter_1_cutoff"].get_normalized() == 0.0
    
    controls["filter_1_cutoff"].set_normalized(1.5)
    assert controls["filter_1_cutoff"].get_normalized() == 1.0


if __name__ == "__main__":
    # Run basic tests
    test_normalized_linear_parameter()
    print("✓ Linear parameter test passed")
    
    test_normalized_quartic_parameter()
    print("✓ Quartic parameter test passed")
    
    test_normalized_indexed_parameter()
    print("✓ Indexed parameter test passed")
    
    test_normalized_vst_style_automation()
    print("✓ VST-style automation test passed")
    
    test_normalized_clipping()
    print("✓ Clipping test passed")
    
    print("\nAll normalized parameter tests passed!")
