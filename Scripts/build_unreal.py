"""Compile the Unreal Editor target. Does not launch the editor or package a game.

The engine version comes from EngineAssociation in PerfectWorld.uproject, so
migrating the project to a new engine version needs no change here. Pass
--engine-root to override it for an engine installed somewhere unusual.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
uproject = root / 'PerfectWorld.uproject'
association = json.loads(uproject.read_text(encoding='utf-8')).get('EngineAssociation', '5.7')

parser = argparse.ArgumentParser()
parser.add_argument('--engine-root', type=Path,
                    default=Path(r'C:\Program Files\Epic Games') / f'UE_{association}')
args = parser.parse_args()

build = args.engine_root / 'Engine/Build/BatchFiles/Build.bat'
if not build.exists():
    raise SystemExit(f'Build.bat not found under {args.engine_root}. The project expects Unreal '
                     f'Engine {association}; install it or supply --engine-root.')
log = root / 'Saved/Logs/Build.log'
log.parent.mkdir(parents=True, exist_ok=True)
env = {k.upper() if os.name == 'nt' else k: v for k, v in os.environ.items()}
subprocess.run([str(build), 'PerfectWorldEditor', 'Win64', 'Development',
                str(uproject), '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBA', '-Log=' + str(log)],
               env=env, check=True)
