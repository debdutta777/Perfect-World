"""Compile the Unreal Editor target. Does not launch the editor or package a game."""
import argparse
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--engine-root', type=Path, default=Path(r'C:\Program Files\Epic Games\UE_5.7'))
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
build=args.engine_root/'Engine/Build/BatchFiles/Build.bat'
if not build.exists():
    raise SystemExit('Build.bat not found. Supply --engine-root with your UE 5.7 installation.')
log=root/'Saved/Logs/Build.log'
log.parent.mkdir(parents=True,exist_ok=True)
env={k.upper() if os.name=='nt' else k:v for k,v in os.environ.items()}
subprocess.run([str(build),'PerfectWorldEditor','Win64','Development',
                str(root/'PerfectWorld.uproject'),'-WaitMutex','-NoHotReloadFromIDE','-NoUBA','-Log='+str(log)],
               env=env,check=True)
