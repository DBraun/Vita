# This file is part of the Vita distribution (https://github.com/DBraun/Vita).
# Copyright (c) 2025 David Braun.

"""Render many Vital presets in parallel using threads instead of processes.

Vita releases the Python GIL during the DSP-heavy render (and during
``load_preset`` / ``load_json`` / ``to_json`` / ``render_file``), so multiple
Python threads render in true parallel with no process/pickle overhead and no
``fork()``. Each thread MUST own its own ``vita.Synth`` -- a ``Synth`` is not
safe to share across threads (its per-instance critical section would serialize
concurrent renders anyway).

Unlike the multiprocessing example, this works even after ``import jax`` (or any
library that spawns threads at import), where a ``fork()``-based pool would
crash the workers.
"""

import argparse
import logging
import os
from pathlib import Path
import threading
import time
from concurrent.futures import ThreadPoolExecutor

# extra libraries to install with pip
import vita
import numpy as np
from scipy.io import wavfile
from tqdm import tqdm


class ThreadRenderer:
    """Renders presets across a thread pool, one persistent Synth per thread."""

    def __init__(
        self,
        bpm: float = 120.0,
        note_duration: float = 2.0,
        render_duration: float = 5.0,
        pitch_low: int = 60,
        pitch_high: int = 72,
        velocity: float = 0.7,
        sample_rate: int = 44_100,
        output_dir: str = "output",
    ):
        self.bpm = bpm
        self.note_duration = note_duration
        self.render_duration = render_duration
        self.pitch_low, self.pitch_high = pitch_low, pitch_high
        self.velocity = velocity
        self.sample_rate = sample_rate
        self.output_dir = Path(output_dir)
        # Each worker thread lazily builds and reuses its own Synth here.
        self._local = threading.local()

    def _get_synth(self) -> vita.Synth:
        synth = getattr(self._local, "synth", None)
        if synth is None:
            synth = vita.Synth()
            synth.set_bpm(self.bpm)
            synth.set_sample_rate(self.sample_rate)
            self._local.synth = synth
        return synth

    def process_item(self, preset_path: str) -> None:
        synth = self._get_synth()
        synth.load_preset(preset_path)
        basename = os.path.basename(preset_path)
        for pitch in range(self.pitch_low, self.pitch_high + 1):
            audio = synth.render(
                pitch, self.velocity, self.note_duration, self.render_duration
            )
            output_path = self.output_dir / f"{pitch}_{basename}.wav"
            wavfile.write(str(output_path), self.sample_rate, audio.transpose())

    def run(self, preset_paths, num_workers: int) -> None:
        with ThreadPoolExecutor(max_workers=num_workers) as pool:
            futures = [
                pool.submit(self.process_item, str(p)) for p in preset_paths
            ]
            for fut in tqdm(futures):
                fut.result()  # re-raise any worker exception


def main(
    preset_dir,
    bpm: float = 120.0,
    note_duration: float = 2.0,
    render_duration: float = 4.0,
    pitch_low: int = 60,
    pitch_high: int = 60,
    num_workers=None,
    output_dir="output",
    logging_level="INFO",
):
    logging.basicConfig()
    logger = logging.getLogger("vita")
    logger.setLevel(logging_level.upper())

    preset_paths = list(Path(preset_dir).rglob("*.vital"))
    num_threads = num_workers or os.cpu_count()

    logger.info(f"Note duration: {note_duration}")
    logger.info(f"Render duration: {render_duration}")
    logger.info(f"Using num threads: {num_threads}")
    logger.info(f"Pitch low: {pitch_low}")
    logger.info(f"Pitch high: {pitch_high}")
    logger.info(f"Output directory: {output_dir}")

    os.makedirs(output_dir, exist_ok=True)

    renderer = ThreadRenderer(
        bpm=bpm,
        note_duration=note_duration,
        render_duration=render_duration,
        pitch_low=pitch_low,
        pitch_high=pitch_high,
        output_dir=output_dir,
    )

    t0 = time.perf_counter()
    renderer.run(preset_paths, num_threads)
    logger.info(f"All done in {time.perf_counter() - t0:.2f}s!")


if __name__ == "__main__":
    # fmt: off
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset-dir", required=True, help="Directory path of Vital presets.")
    parser.add_argument("--bpm", default=120.0, type=float, help="Beats per minute for the Render Engine.")
    parser.add_argument("--note-duration", default=1, type=float, help="Note duration in seconds.")
    parser.add_argument("--pitch-low", default=60, type=int, help="Lowest MIDI pitch to be used (inclusive).")
    parser.add_argument("--pitch-high", default=60, type=int, help="Highest MIDI pitch to be used (inclusive).")
    parser.add_argument("--render-duration", default=1, type=float, help="Render duration in seconds.")
    parser.add_argument("--num-workers", default=None, type=int, help="Number of worker threads to use.")
    parser.add_argument("--output-dir", default=os.path.join(os.path.dirname(__file__), "output"), help="Output directory.")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "NOTSET"], help="Logger level.")
    # fmt: on
    args = parser.parse_args()

    main(
        args.preset_dir,
        args.bpm,
        args.note_duration,
        args.render_duration,
        args.pitch_low,
        args.pitch_high,
        args.num_workers,
        args.output_dir,
        args.log_level,
    )
