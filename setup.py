#!/usr/bin/env python
"""
Minimal setup.py for backwards compatibility.
The actual build is handled by scikit-build-core via pyproject.toml.
"""

from setuptools import setup

# The actual configuration is in pyproject.toml
# This file exists for backwards compatibility with:
# - pip install -e .
# - python setup.py develop
# - legacy tools that expect setup.py

if __name__ == "__main__":
    setup()