"""Package a Windows build of the game (verification rung 4, ROADMAP M0-15).

Builds the game target, cooks, stages, paks and archives it with Unreal's
BuildCookRun, the same pipeline a release uses. It does not run the game.
The engine version comes from EngineAssociation in PerfectWorld.uproject;
pass --engine-root for an engine installed somewhere else. Close the editor
first and never run this alongside another Unreal build.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
uproject = root / 'PerfectWorld.uproject'
association = json.loads(uproject.read_text(encoding='utf-8')).get('EngineAssociation', '5.8')

parser = argparse.ArgumentParser()
parser.add_argument('--engine-root', type=Path,
                    default=Path(r'C:\Program Files\Epic Games') / f'UE_{association}')
parser.add_argument('--config', default='Development', choices=['Development', 'Shipping'])
parser.add_argument('--output', type=Path, default=root / 'Saved' / 'Packages' / 'Win64')
args = parser.parse_args()

uat = args.engine_root / 'Engine/Build/BatchFiles/RunUAT.bat'
if not uat.exists():
    raise SystemExit(f'RunUAT.bat not found under {args.engine_root}. The project expects Unreal '
                     f'Engine {association}; install it or supply --engine-root.')

log = root / 'Saved/Logs/Package.log'
log.parent.mkdir(parents=True, exist_ok=True)
env = {k.upper() if os.name == 'nt' else k: v for k, v in os.environ.items()}
command = [str(uat), 'BuildCookRun', f'-project={uproject}', '-noP4', '-unattended', '-utf8output',
           '-platform=Win64', f'-clientconfig={args.config}', '-build', '-cook', '-stage', '-pak',
           '-archive', f'-archivedirectory={args.output}', '-ubtargs=-NoUBA']

# Tee the output: the console shows progress, the log keeps everything.
with log.open('w', encoding='utf-8', errors='replace') as out:
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding='utf-8', errors='replace')
    for line in process.stdout:
        print(line, end='')
        out.write(line)
    code = process.wait()

print(f'\nPackage log: {log}')
print(f'Packaged build: {args.output}' if code == 0 else f'BuildCookRun failed with exit code {code}')
raise SystemExit(code)
