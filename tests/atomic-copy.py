#!/usr/bin/env python3
"""Compile the actual portable production header and inject POSIX failure boundaries."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='datakeeper-atomic-test-') as directory:
    directory = Path(directory)
    binary = directory / 'test'
    subprocess.run(['clang++', '-std=c++23', '-Wall', '-Wextra', '-Werror', '-I', str(root / 'include'), str(root / 'tests/atomic-copy.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(directory / 'data')], check=True)
