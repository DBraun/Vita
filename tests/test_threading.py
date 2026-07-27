"""Tests for parallel, thread-based rendering.

Vita releases the GIL during ``render``, ``render_file``, ``load_preset``,
``load_json`` and ``to_json``. These tests cover the two things that can go
wrong with that: renders producing garbage when run concurrently, and the
critical section being stranded so a later call hangs.
"""

import os
import threading
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import pytest

import vita

NOTE = 60
VELOCITY = 0.7
NOTE_DUR = 0.3
RENDER_DUR = 0.8


def _thread_local_synth(local: threading.local) -> vita.Synth:
    """Return this thread's Synth, creating it on first use.

    Args:
        local: A ``threading.local`` used to hold one Synth per worker thread.

    Returns:
        The calling thread's own ``vita.Synth`` instance.
    """
    synth = getattr(local, "synth", None)
    if synth is None:
        synth = local.synth = vita.Synth()
    return synth


def test_parallel_render_produces_valid_audio():
    """Many threads, each with its own Synth, all produce usable audio."""
    local = threading.local()
    expected_samples = None

    def render(_):
        synth = _thread_local_synth(local)
        return synth.render(NOTE, VELOCITY, NOTE_DUR, RENDER_DUR)

    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(render, range(64)))

    assert len(results) == 64
    for audio in results:
        assert audio.ndim == 2
        assert audio.shape[0] == 2
        assert np.isfinite(audio).all()
        if expected_samples is None:
            expected_samples = audio.shape[1]
        assert audio.shape[1] == expected_samples
        assert np.abs(audio).max() > 0.0


def test_parallel_load_and_serialize(tmp_path):
    """load_preset / to_json / load_json are safe to run concurrently.

    These paths build wavetables, which is what exercises the per-thread
    FourierTransform.
    """
    seed = vita.Synth()
    preset_path = tmp_path / "preset.vital"
    preset_path.write_text(seed.to_json())

    local = threading.local()

    def round_trip(_):
        synth = _thread_local_synth(local)
        assert synth.load_preset(str(preset_path))
        json_text = synth.to_json()
        assert synth.load_json(json_text)
        return synth.render(NOTE, VELOCITY, NOTE_DUR, RENDER_DUR)

    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(round_trip, range(48)))

    assert all(np.isfinite(audio).all() for audio in results)


def test_parallel_render_file(tmp_path):
    """render_file writes one correct wav per thread without interleaving."""
    local = threading.local()

    def write(index):
        synth = _thread_local_synth(local)
        path = tmp_path / f"out{index}.wav"
        assert synth.render_file(
            str(path), NOTE, VELOCITY, NOTE_DUR, RENDER_DUR
        )
        return path

    with ThreadPoolExecutor(max_workers=8) as pool:
        paths = list(pool.map(write, range(16)))

    sizes = {os.path.getsize(path) for path in paths}
    assert len(sizes) == 1, f"wav files differ in size: {sizes}"
    assert sizes.pop() > 44


def test_shared_synth_serializes_without_deadlock():
    """A Synth shared across threads is serialized by its critical section.

    Sharing is not the recommended usage -- it gains no parallelism -- but it
    must not deadlock or corrupt state.
    """
    synth = vita.Synth()

    def render(_):
        return synth.render(NOTE, VELOCITY, NOTE_DUR, RENDER_DUR)

    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(render, range(16)))

    assert all(np.isfinite(audio).all() for audio in results)


@pytest.mark.parametrize(
    "payload", ["not json at all", "{}", '{"synth_version": "9.9.9"}', "[]"]
)
def test_failed_load_does_not_strand_the_lock(payload):
    """A rejected load must leave the Synth renderable.

    ``load_json`` pauses processing while it mutates engine state. If a failure
    path skipped the resume, the next ``render`` would block forever with the
    GIL released -- an uninterruptible hang rather than a Python error.
    """
    synth = vita.Synth()
    outcome = {}

    def load_then_render():
        outcome["loaded"] = synth.load_json(payload)
        outcome["shape"] = synth.render(
            NOTE, VELOCITY, NOTE_DUR, RENDER_DUR
        ).shape

    thread = threading.Thread(target=load_then_render, daemon=True)
    thread.start()
    thread.join(timeout=60)

    assert not thread.is_alive(), "render() hung after a failed load_json"
    assert outcome["loaded"] is False
    assert outcome["shape"][0] == 2
