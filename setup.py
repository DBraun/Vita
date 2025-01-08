#! /usr/bin/env python

# NOTE: You can test building wheels locally with
# `python -m build --wheel`
# Then in the `dist` directory, `pip install vita`

import setuptools
from setuptools import setup, Extension
from setuptools.dist import Distribution
import os
import os.path
from pathlib import Path
import shutil
import platform
import glob


this_dir = os.path.abspath(os.path.dirname(__file__))

def get_VITA_VERSION():
    with open('vita/version.py', 'r') as f:
        text = f.read()
        version = text.split("=")[-1].strip().replace('"', "")
        return version

VITA_VERSION = get_VITA_VERSION()


class BinaryDistribution(Distribution):
    """Distribution which always forces a binary package with platform name"""
    def has_ext_modules(foo):
        return True


ext_modules = []
package_data = []

shutil.copy('LICENSE', 'vita')

if platform.system() == "Windows":

    build_folder = os.path.join(this_dir, "headless", "builds", "VisualStudio2022", "x64", "Release", "Dynamic Library")
    shutil.copy(os.path.join(build_folder, 'vita.dll'), os.path.join('vita', 'vita.pyd'))
    
    package_data += ['vita/vita.pyd']

elif platform.system() == "Linux":

    files = ['vita/vita.so']
    for file in files:
        filepath = os.path.abspath(file)
        assert os.path.isfile(filepath), ValueError("File not found: " + filepath)
    print('Using compiled files: ', str(files))

    package_data += files

elif platform.system() == "Darwin":

    build_folder = os.path.join(this_dir, "headless", "builds", "osx", "build", "Release")

    shutil.copy(os.path.join(build_folder, 'vita.so'), os.path.join('vita', 'vita.so'))

    package_data += ['vita/vita.so']

else:
    raise NotImplementedError(
        f"setup.py hasn't been implemented for platform: {platform}."
    )

package_data.append('vita/LICENSE')

# Every item in package_data should be inside the vita directory.
# Then we make the paths relative to this directory.
package_data = [os.path.relpath(os.path.abspath(a), os.path.join(this_dir, "vita")).replace('\\', '/') for a in package_data]
print('package_data: ', package_data)
long_description = (Path(__file__).parent / "README.md").read_text()

setup(
    name='vita',
    url='https://github.com/DBraun/Vita',
    project_urls={
        'Documentation': 'https://dirt.design/Vita',
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
    ],
    keywords='audio music sound synthesizer',
    python_requires=">=3.9",
    install_requires=[],
    packages=setuptools.find_packages(),
    py_modules=['vita'],
    include_package_data=True,
    package_data={
        "": package_data
    },
    zip_safe=False,
    distclass=BinaryDistribution,
    ext_modules=ext_modules
)
