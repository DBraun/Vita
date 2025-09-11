"""Python bindings for the Vital synthesizer"""

from .version import __version__

# Import the C++ extension module
# The compiled module is named _vita to avoid naming conflicts with the package
try:
    from ._vita import Synth, constants, get_modulation_sources, get_modulation_destinations
except ImportError as e:
    import sys
    raise ImportError(
        f"Failed to import _vita C++ extension: {e}\n"
        "Make sure the module is properly built and installed."
    ) from e

__all__ = [
    "Synth",
    "constants",
    "get_modulation_sources",
    "get_modulation_destinations",
    "__version__",
]
