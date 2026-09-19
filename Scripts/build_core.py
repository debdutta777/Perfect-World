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
if os.name == 'nt':
    config += ['-G', 'Visual Studio 17 2022', '-A', 'x64']
for command in [config,
                ['cmake', '--build', str(args.build_dir.resolve()), '--config', 'Release'],
                ['ctest', '--test-dir', str(args.build_dir.resolve()), '-C', 'Release', '--output-on-failure']]:
    subprocess.run(command, env=env, check=True)
