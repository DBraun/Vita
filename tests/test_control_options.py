"""Tests for control metadata, especially the discrete controls' name tables.

``ValueDetails.string_lookup`` is a bare pointer with no length attached, and a
few of Vital's controls accept more values than they have names for. Sizing the
table from the value range instead of the table itself reads out of bounds,
which has crashed the interpreter and, worse, silently returned names belonging
to an unrelated array.
"""

import numpy as np
import pytest

import vita


@pytest.fixture(scope="module")
def synth():
    """A default Synth shared by the read-only metadata tests."""
    return vita.Synth()


@pytest.fixture(scope="module")
def control_names(synth):
    """Every control name, sorted."""
    return sorted(synth.get_controls())


def test_every_control_reports_options(synth, control_names):
    """Reading .options must be safe for all controls, not just the tested few.

    Some indexed controls (Polyphony, Pitch Bend Range, ...) are genuinely
    numeric and carry no name table, so being discrete does not imply having
    names. Continuous controls must never have any.
    """
    assert len(control_names) > 700

    named = 0
    for name in control_names:
        details = synth.get_control_details(name)
        options = list(details.options)
        assert all(isinstance(o, str) for o in options), name
        if not details.is_discrete:
            assert options == [], f"{name} is continuous but has names"
        elif options:
            named += 1

    assert named > 50, "expected many discrete controls to expose names"


def test_options_never_exceed_the_value_range(synth, control_names):
    """A name list longer than the range means we read past the table."""
    for name in control_names:
        details = synth.get_control_details(name)
        options = list(details.options)
        if not options:
            continue
        span = int(details.max - details.min + 1)
        assert len(options) <= span, (
            f"{name}: {len(options)} names for a range of {span} values"
        )


def test_options_are_plausible_names(synth, control_names):
    """Out-of-bounds reads surface as junk or absurdly long strings."""
    for name in control_names:
        for option in synth.get_control_details(name).options:
            assert 0 < len(option) < 64, f"{name}: implausible option {option[:80]!r}"
            assert option.isprintable(), f"{name}: unprintable option {option!r}"


@pytest.mark.parametrize(
    "name, expected",
    [
        # Range 0..3, four names -- range and table agree.
        ("delay_style", ["Mono", "Stereo", "Ping Pong", "Mid Ping Pong"]),
        # Range 0..2 but kOffOnNames only has two entries. The third value is
        # unnamed rather than read from whatever follows the array.
        ("osc_1_view_2d", ["Off", "On"]),
        ("view_spectrogram", ["Off", "On"]),
        # Range 0..9 but kFilterStyleNames only has five. The five that used to
        # follow came from kUnisonStackNames.
        ("filter_1_style", ["12dB", "24dB", "Notch Blend", "Notch Spread", "B/P/N"]),
    ],
)
def test_known_short_tables(synth, name, expected):
    """Controls whose value range is wider than their name table."""
    assert list(synth.get_control_details(name).options) == expected


# Controls whose value range is deliberately wider than their name table, and
# why. Anything not listed here should have a name for every value it accepts.
EXPECTED_UNNAMED_VALUES = {
    # A single style parameter spans every filter model, but each model has its
    # own name table; kFilterStyleNames covers the analog model's five.
    "filter_1_style": 5,
    "filter_2_style": 5,
    "filter_fx_style": 5,
    # GUI-only view state, stored in the preset but never read by the engine.
    # The third value's meaning belongs to Vital's editor, so it has no name
    # here.
    "osc_1_view_2d": 1,
    "osc_2_view_2d": 1,
    "osc_3_view_2d": 1,
    "view_spectrogram": 1,
}


def test_only_known_controls_have_unnamed_values(synth, control_names):
    """Every other control names every value it accepts.

    This is the check that would have caught the out-of-bounds read: before it
    was fixed, four *_destination controls also appeared here, reporting a
    fifteenth option that came from an unrelated array.
    """
    unnamed = {}
    for name in control_names:
        details = synth.get_control_details(name)
        options = list(details.options)
        if not options:
            continue
        gap = int(details.max - details.min + 1) - len(options)
        if gap:
            unnamed[name] = gap

    assert unnamed == EXPECTED_UNNAMED_VALUES


def test_destination_range_matches_its_names(synth):
    """Destination accepts exactly the routings it has names for.

    Vital sizes this parameter as kNumSourceDestinations + kNumEffects where
    every sibling uses count - 1, so it used to accept a trailing value that
    routed nowhere.
    """
    for name in ("osc_1_destination", "osc_2_destination", "osc_3_destination",
                 "sample_destination"):
        details = synth.get_control_details(name)
        options = list(details.options)
        assert len(options) == int(details.max - details.min + 1), name
        assert options[-1] == "REVERB", name


def test_get_control_text_agrees_with_options(synth):
    """get_control_text must return the same names, indexed by value."""
    for name in ("delay_style", "osc_1_view_2d", "filter_1_style"):
        controls = synth.get_controls()
        options = list(synth.get_control_details(name).options)
        for index, expected in enumerate(options):
            controls[name].set(index)
            assert synth.get_control_text(name) == expected, (name, index)


def test_get_control_text_clamps_unnamed_values(synth):
    """A value past the end of the table falls back to the last name."""
    controls = synth.get_controls()
    # osc_1_view_2d accepts 2 but only has names for 0 and 1.
    controls["osc_1_view_2d"].set(2)
    assert synth.get_control_text("osc_1_view_2d") == "On"


def test_render_still_works_after_metadata_reads(synth):
    """Reading metadata for every control must not disturb the engine."""
    audio = synth.render(60, 0.7, 0.2, 0.5)
    assert audio.shape[0] == 2
    assert np.isfinite(audio).all()
