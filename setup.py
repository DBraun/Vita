#! /usr/bin/env python

"""Packaging for Vita.

The C++ extension is compiled from here, so an ordinary ``pip install .`` or
``python -m build --wheel`` is all that is needed on every platform. The actual
compilation is still driven by the Projucer-generated project for the host
platform -- a Makefile on Linux, an Xcode project on macOS, a Visual Studio
solution on Windows -- which this module invokes with the paths of the Python
interpreter currently running the build.
"""

import os
import platform
import re
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path

import setuptools
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.dist import Distribution

THIS_DIR = Path(__file__).resolve().parent
NANOBIND_DIR = THIS_DIR / "third_party" / "nanobind"


def get_vita_version() -> str:
    """Read the package version without importing the package.

    Returns:
        The version string from ``vita/version.py``.
    """
    text = (THIS_DIR / "vita" / "version.py").read_text()
    return text.split("=")[-1].strip().replace('"', "")


VITA_VERSION = get_vita_version()


class BinaryDistribution(Distribution):
    """Distribution which always forces a binary package with platform name."""

    def has_ext_modules(self) -> bool:
        """Report that this distribution contains a compiled extension.

        Returns:
            Always True.
        """
        return True


def macos_archs() -> str:
    """Return the architectures the macOS build should target.

    The Xcode project defaults to ``arm64 x86_64``, so without an override it
    builds both slices and lipos them together. nanobind's static library is
    built natively for one architecture only, so the other slice silently links
    without it -- Python extensions defer undefined symbols to load time, which
    hides the problem until import.

    Returns:
        A space-separated architecture list for xcodebuild's ``ARCHS``.
    """
    # cibuildwheel sets ARCHFLAGS ("-arch arm64", possibly several for
    # universal2); outside of it, build for the machine we are on.
    archflags = os.environ["ARCHFLAGS"] if "ARCHFLAGS" in os.environ else ""
    archs = re.findall(r"-arch\s+(\S+)", archflags)
    if archs:
        return " ".join(dict.fromkeys(archs))
    return platform.machine()


def simd_flags() -> str:
    """Return the architecture-specific compiler flags for the host machine.

    Returns:
        Flags to pass to the Linux build as ``SIMDFLAGS``.
    """
    machine = platform.machine().lower()
    if "aarch" in machine or "arm64" in machine:
        return "-march=armv8-a -mtune=cortex-a53"
    if "arm" in machine:
        return "-march=armv8-a -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard"
    return "-msse2"


class BuildVitaExtension(build_ext):
    """Compile the Vita extension module and generate its type stubs."""

    def run(self) -> None:
        """Build nanobind, compile the extension, and emit stubs."""
        self.build_nanobind()

        system = platform.system()
        if system == "Linux":
            binary = self.build_linux()
        elif system == "Darwin":
            binary = self.build_macos()
        elif system == "Windows":
            binary = self.build_windows()
        else:
            raise RuntimeError(f"Vita has no build recipe for platform: {system}")

        destination = Path(self.get_ext_fullpath("vita.vita"))
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(binary, destination)
        print(f"Placed extension at {destination}")

        if system == "Linux":
            subprocess.run(["strip", "--strip-unneeded", str(destination)], check=True)

        self.generate_stubs(destination.parent)

    def build_nanobind(self) -> None:
        """Configure and build nanobind's static library."""
        if (NANOBIND_DIR / ".git").exists() or (THIS_DIR / ".git").exists():
            # robin_map is a submodule of nanobind; a --recursive clone or a CI
            # checkout will already have it, but a plain clone will not.
            subprocess.run(
                ["git", "submodule", "update", "--init", "--recursive"],
                cwd=NANOBIND_DIR,
                check=True,
            )

        build_dir = NANOBIND_DIR / "build"
        subprocess.run(
            [
                "cmake",
                "-S", str(NANOBIND_DIR),
                "-B", str(build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
                f"-DPython_EXECUTABLE={sys.executable}",
            ],
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", str(build_dir), "--config", "Release", "--target", "nanobind-static"],
            check=True,
        )

    def build_linux(self) -> Path:
        """Compile the extension with the Projucer-generated Makefile.

        Returns:
            Path to the compiled shared object.
        """
        project_dir = THIS_DIR / "headless" / "builds" / "linux"
        subprocess.run(
            [
                "make",
                "-C", str(project_dir),
                f"-j{os.cpu_count() or 1}",
                "CONFIG=Release",
                f"SIMDFLAGS={simd_flags()}",
                "LIBS=-lstdc++fs",
                # INCLUDEPY and LIBDIR rather than sysconfig.get_paths(): under
                # build isolation this runs inside a venv, whose 'include' path
                # is empty. These two config vars resolve against the base
                # installation, so they point at the real headers either way.
                f"LDFLAGS=-L{sysconfig.get_config_var('LIBDIR')}",
                f"CXXFLAGS=-I{sysconfig.get_config_var('INCLUDEPY')}",
            ],
            check=True,
        )
        return project_dir / "build" / "libvita.so"

    def build_macos(self) -> Path:
        """Compile the extension with the Projucer-generated Xcode project.

        Returns:
            Path to the compiled shared object.
        """
        # The Xcode project reads both of these; derive them from the running
        # interpreter so the caller does not have to set them.
        env = dict(os.environ)
        env.setdefault("PYTHONMAJOR", f"{sys.version_info.major}.{sys.version_info.minor}")
        env.setdefault("pythonLocation", sys.base_prefix)
        print(f"PYTHONMAJOR={env['PYTHONMAJOR']} pythonLocation={env['pythonLocation']}")

        archs = macos_archs()
        print(f"Building for ARCHS={archs}")

        project = THIS_DIR / "headless" / "builds" / "osx" / "Vita.xcodeproj"
        subprocess.run(
            [
                "xcodebuild",
                "ONLY_ACTIVE_ARCH=NO",
                f"ARCHS={archs}",
                "-configuration", "Release",
                "-project", str(project),
                'CODE_SIGN_IDENTITY=',
                "CODE_SIGNING_REQUIRED=NO",
                'CODE_SIGN_ENTITLEMENTS=',
                "CODE_SIGNING_ALLOWED=NO",
            ],
            check=True,
            env=env,
        )
        return THIS_DIR / "headless" / "builds" / "osx" / "build" / "Release" / "vita.so.dylib"

    def build_windows(self) -> Path:
        """Compile the extension with the Visual Studio solution.

        Returns:
            Path to the compiled DLL.
        """
        env = dict(os.environ)
        env.setdefault("PYTHONMAJOR", f"{sys.version_info.major}.{sys.version_info.minor}")
        env.setdefault("pythonLocation", sys.base_prefix)
        print(f"PYTHONMAJOR={env['PYTHONMAJOR']} pythonLocation={env['pythonLocation']}")

        solution = THIS_DIR / "headless" / "builds" / "VisualStudio2022" / "Vita.sln"
        subprocess.run(
            ["msbuild", str(solution), "/property:Configuration=Release"],
            check=True,
            env=env,
        )
        return (
            THIS_DIR / "headless" / "builds" / "VisualStudio2022" / "x64" / "Release"
            / "Dynamic Library" / "vita.dll"
        )

    def generate_stubs(self, package_dir: Path) -> None:
        """Generate vita.pyi and py.typed next to the compiled extension.

        Args:
            package_dir: The staged ``vita`` package directory, which already
                contains the freshly built extension module.
        """
        stubgen = NANOBIND_DIR / "src" / "stubgen.py"
        # stubgen imports the extension, so the staging directory that holds the
        # vita package must be importable. Suppress bytecode so the __pycache__
        # it would leave behind does not end up inside the wheel.
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
        # --recursive so that vita.vita.constants gets a stub too; without it a
        # type checker cannot resolve the enums that vita/constants/__init__.py
        # re-exports. The output lands as vita/vita/__init__.pyi plus
        # vita/vita/constants.pyi, which is the layout PEP 561 expects for a
        # module that has submodules.
        subprocess.run(
            [
                sys.executable, str(stubgen),
                "--import", str(package_dir.parent),
                "--module", "vita.vita",
                "--recursive",
                "--output-dir", str(package_dir),
                "--marker-file", str(package_dir / "py.typed"),
                "--pattern-file", str(THIS_DIR / "stubgen_patterns.txt"),
            ],
            check=True,
            env=env,
        )


shutil.copy(THIS_DIR / "LICENSE", THIS_DIR / "vita")

long_description = (THIS_DIR / "README.md").read_text()

setup(
    name='vita',
    url='https://github.com/DBraun/Vita',
    project_urls={
        'Documentation': 'https://dbraun.github.io/Vita/',
        'Changelog': 'https://github.com/DBraun/Vita/blob/main/CHANGELOG.md',
        'Source': 'https://github.com/DBraun/Vita',
    },
    version=VITA_VERSION,
    author='David Braun',
    author_email='braun@ccrma.stanford.edu',
    description='Python bindings for the Vital synthesizer',
    long_description=long_description,
    long_description_content_type='text/markdown',
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C++",
        "Programming Language :: Python",
        "Topic :: Multimedia :: Sound/Audio",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
    ],
    keywords='audio music sound synthesizer',
    python_requires=">=3.9",
    install_requires=[],
    packages=setuptools.find_packages(include=["vita", "vita.*"]),
    # sources is empty because the extension is produced by the platform project
    # rather than by setuptools; BuildVitaExtension does the real work and only
    # uses this entry to work out where the result belongs.
    ext_modules=[Extension("vita.vita", sources=[])],
    cmdclass={"build_ext": BuildVitaExtension},
    include_package_data=True,
    package_data={"vita": ["LICENSE", "py.typed", "vita/*.pyi"]},
    zip_safe=False,
    distclass=BinaryDistribution,
)
