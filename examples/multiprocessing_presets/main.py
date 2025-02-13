# This file is part of the Vita distribution (https://github.com/DBraun/Vita).
# Copyright (c) 2025 David Braun.

import argparse
from collections import namedtuple
import logging
import multiprocessing
import os
from pathlib import Path
import time
import traceback

# extra libraries to install with pip
import vita
import numpy as np
from scipy.io import wavfile
from tqdm import tqdm


Item = namedtuple("Item", "preset_path")


class Worker:

    def __init__(
        self,
        queue: multiprocessing.Queue,
        bpm: float = 120.0,
        note_duration: float = 2.0,
        render_duration: float = 5.0,
        pitch_low: int = 60,
        pitch_high: int = 72,
        velocity: int = 100,
        output_dir="output",
    ):
        self.queue = queue
        self.bpm = bpm
        self.note_duration = note_duration
        self.render_duration = render_duration
        self.pitch_low, self.pitch_high = pitch_low, pitch_high
        self.velocity = velocity
        self.output_dir = Path(output_dir)

    def startup(self):
        synth = vita.Synth()
        synth.set_bpm(self.bpm)
        self.synth = synth

    def process_item(self, item: Item):
        preset_path = item.preset_path
        self.synth.load_preset(preset_path)
        basename = os.path.basename(preset_path)

        for pitch in range(self.pitch_low, self.pitch_high + 1):
            audio = self.synth.render(
                pitch, self.velocity, self.note_duration, self.note_duration
            )
            output_path = self.output_dir / f"{pitch}_{basename}.wav"
            wavfile.write(str(output_path), 44_100, audio.transpose())

    def run(self):
        try:
            self.startup()
            while True:
                try:
                    item = self.queue.get_nowait()
                    self.process_item(item)
                except multiprocessing.queues.Empty:
                    break
        except Exception as e:
            return traceback.format_exc()


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

    # Create logger
    logging.basicConfig()
    logger = logging.getLogger("vita")
    logger.setLevel(logging_level.upper())

    # Glob all the preset file paths
    preset_paths = list(Path(preset_dir).rglob("*.vital"))

    # Get num items so that the progress bar works well
    num_items = len(preset_paths)

    # Create a Queue and add items
    input_queue = multiprocessing.Manager().Queue()
    for preset_path in preset_paths:
        input_queue.put(Item(str(preset_path)))

    # Create a list to hold the worker processes
    workers = []

    # The number of workers to spawn
    num_processes = num_workers or multiprocessing.cpu_count()

    # Log info
    logger.info(f"Note duration: {note_duration}")
    logger.info(f"Render duration: {render_duration}")
    logger.info(f"Using num workers: {num_processes}")
    logger.info(f"Pitch low: {pitch_low}")
    logger.info(f"Pitch high: {pitch_high}")
    logger.info(f"Output directory: {output_dir}")

    os.makedirs(output_dir, exist_ok=True)

    # Create a multiprocessing Pool
    with multiprocessing.Pool(processes=num_processes) as pool:
        # Create and start a worker process for each CPU
        for i in range(num_processes):
            worker = Worker(
                input_queue,
                bpm=bpm,
                note_duration=note_duration,
                render_duration=render_duration,
                pitch_low=pitch_low,
                pitch_high=pitch_high,
                output_dir=output_dir,
            )
            async_result = pool.apply_async(worker.run)
            workers.append(async_result)

        # Use tqdm to track progress. Update the progress bar in each iteration.
        pbar = tqdm(total=num_items)
        while True:
            incomplete_count = sum(1 for w in workers if not w.ready())
            pbar.update(
                num_items - input_queue.qsize() - pbar.n
            )  # not perfectly accurate.
            if incomplete_count == 0:
                break
            time.sleep(0.1)
        pbar.close()

    # Check for exceptions in the worker processes
    for i, worker in enumerate(workers):
        exception = worker.get()
        if exception is not None:
            logger.error(f"Exception in worker {i}:\n{exception}")

    logger.info("All done!")


if __name__ == "__main__":
    # We're using multiprocessing.Pool, so our code MUST be inside __main__.
    # See https://docs.python.org/3/library/multiprocessing.html

    # fmt: off
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset-dir", required=True, help="Directory path of Vital presets.")
    parser.add_argument("--bpm", default=120.0, type=float, help="Beats per minute for the Render Engine.")
    parser.add_argument("--note-duration", default=1, type=float, help="Note duration in seconds.")
    parser.add_argument("--pitch-low", default=60, type=int, help="Lowest MIDI pitch to be used (inclusive).")
    parser.add_argument("--pitch-high", default=60, type=int, help="Highest MIDI pitch to be used (inclusive).")
    parser.add_argument("--render-duration", default=1, type=float, help="Render duration in seconds.")
    parser.add_argument("--num-workers", default=None, type=int, help="Number of workers to use.")
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
