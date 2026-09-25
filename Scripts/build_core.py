"""Build and test portable creation rules; requires CMake and a C++20 toolchain."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
# Windows can receive duplicate Path/PATH entries from host runtimes. MSBuild
# rejects these; normalize only this child process environment, never the host.
env = {k.upper() if os.name == 'nt' else k: v for k, v in os.environ.items()}
config = ['cmake', '-S', str(root), '-B', str(args.build_dir.resolve())]
# No -G: an existing build directory keeps the generator it was configured
# with, and a fresh one gets CMake's default, the newest Visual Studio
# installed. A machine with only VS 2026 therefore works without edits.
if os.name == 'nt':
    config += ['-A', 'x64']
for command in [config,
                ['cmake', '--build', str(args.build_dir.resolve()), '--config', 'Release'],
                ['ctest', '--test-dir', str(args.build_dir.resolve()), '-C', 'Release', '--output-on-failure']]:
    subprocess.run(command, env=env, check=True)
