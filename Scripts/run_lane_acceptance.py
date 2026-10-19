"""Run AW-01 packaged evidence serially; no engine or third-party Python required."""
import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
import zlib


ROOT = Path(__file__).resolve().parents[1]
NAME = re.compile(r'[a-z0-9_]{1,64}\Z')
# Scenarios that are a fresh-process pair (explicit new, then continue, one shared slot). Each must read its own original save.
RESTART_PAIR_SCENARIOS = ('save_restart', 'frontages_round4')
FLAGS = ['-RenderOffScreen', '-unattended', '-nosplash', '-nosound',
         '-windowed', '-ResX=1280', '-ResY=720', '-log']


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    with temporary.open('w', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def tree_manifest(path):
    if not path.exists():
        return None
    if not path.is_dir() or is_link(path):
        raise ValueError(f'Unsafe save directory: {path}')
    result = {}
    for item in sorted(path.rglob('*')):
        if is_link(item):
            raise ValueError(f'Links are not allowed in protected saves: {item}')
        key = item.relative_to(path).as_posix()
        result[key] = sha256(item) if item.is_file() else '<directory>'
    return result


def is_link(path):
    # Also reject Windows junctions, including on Python versions before 3.12.
    return path.is_symlink() or bool(getattr(path.lstat(), 'st_file_attributes', 0)
                                    & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def png_check(path):
    """Decode non-interlaced 8-bit RGB/RGBA PNGs, including all five filters.

    RGB variance is measured across pixels (not across channels, nor alpha),
    so a solid red image and varying alpha over a black image both fail.
    """
    data = path.read_bytes()
    if len(data) <= 1024:
        raise ValueError('PNG is not larger than 1 KB')
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG')
    pos, compressed, header, ended = 8, bytearray(), None, False
    while pos < len(data):
        if pos + 12 > len(data):
            raise ValueError('Truncated PNG chunk')
        size = struct.unpack_from('>I', data, pos)[0]
        kind = data[pos + 4:pos + 8]
        end = pos + 8 + size
        if end + 4 > len(data):
            raise ValueError('Truncated PNG data')
        body = data[pos + 8:end]
        if zlib.crc32(kind + body) != struct.unpack_from('>I', data, end)[0]:
            raise ValueError('PNG CRC mismatch')
        if header is None and kind != b'IHDR':
            raise ValueError('PNG must start with IHDR')
        if kind == b'IHDR':
            if header is not None or size != 13:
                raise ValueError('Invalid PNG header')
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            compressed.extend(body)
        elif kind == b'IEND':
            if size:
                raise ValueError('Invalid IEND')
            ended = True
            pos = end + 4
            break
        elif kind[0] & 32 == 0 and kind != b'PLTE':
            raise ValueError('Unsupported critical PNG chunk')
        pos = end + 4
    if not ended or pos != len(data) or header is None:
        raise ValueError('Incomplete PNG')
    width, height, depth, color, method, filtering, interlace = header
    if (not width or not height or width * height > 16_777_216 or depth != 8
            or color not in (2, 6) or method or filtering or interlace):
        raise ValueError('Expected bounded, non-interlaced 8-bit RGB/RGBA PNG')
    channels = 3 if color == 2 else 4
    stride = width * channels
    expected = height * (stride + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(bytes(compressed), expected + 1)
    if len(raw) != expected or not decoder.eof or decoder.unused_data:
        raise ValueError('Invalid PNG decompressed length or stream')
    previous = bytearray(stride)
    first = None
    varied = False
    for y in range(height):
        start = y * (stride + 1)
        mode = raw[start]
        if mode > 4:
            raise ValueError('Unknown PNG filter')
        row = bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            predictors = (0, left, up, (left + up) // 2, paeth(left, up, corner))
            row[x] = (row[x] + predictors[mode]) & 255
        for x in range(0, stride, channels):
            pixel = bytes(row[x:x + 3])
            if first is None:
                first = pixel
            varied |= pixel != first
        previous = row
    if not varied:
        raise ValueError('PNG has zero RGB pixel variance')
    return {'file': path.name, 'bytes': len(data), 'width': width, 'height': height,
            'nonuniform_rgb': True, 'sha256': sha256(path)}


def paeth(left, up, corner):
    prediction = left + up - corner
    distances = [abs(prediction - item) for item in (left, up, corner)]
    return (left, up, corner)[distances.index(min(distances))]


def verify_result(out, scenario, commit):
    report = {'status': 'INCOMPLETE', 'errors': [], 'screenshots': []}
    try:
        result = json.loads((out / f'{scenario}.json').read_text(encoding='utf-8-sig'))
        if not isinstance(result, dict):
            raise ValueError('Result must be an object')
        if type(result.get('schema')) is not int or result['schema'] != 1:
            raise ValueError('Expected schema 1')
        if result.get('scenario') != scenario or result.get('commit') != commit:
            raise ValueError('Scenario or commit mismatch')
        for field in ('completed', 'passed'):
            if type(result.get(field)) is not bool:
                raise ValueError(f'{field} must be boolean')
        planned, steps = result.get('planned_steps'), result.get('steps')
        if (not isinstance(planned, list) or not planned
                or any(not isinstance(n, str) or not NAME.fullmatch(n) for n in planned)
                or len(set(planned)) != len(planned) or not isinstance(steps, list)):
            raise ValueError('Invalid planned_steps or steps')
        report['planned_steps'] = planned
        names = []
        for step in steps:
            if (not isinstance(step, dict) or not isinstance(step.get('name'), str)
                    or type(step.get('ok')) is not bool
                    or not isinstance(step.get('detail'), str)
                    or not isinstance(step.get('screenshot'), str)):
                raise ValueError('Malformed step')
            names.append(step['name'])
            shot = step['screenshot']
            if shot:
                expected = f'{scenario}__{step["name"]}.png'
                if shot != expected or (out / shot).resolve().parent != out.resolve():
                    raise ValueError('Screenshot must be the contracted filename in out')
                report['screenshots'].append(png_check(out / shot))
            elif step['name'].startswith('shot_') and step['ok']:
                raise ValueError('Successful screenshot step has no screenshot')
        report['observed_steps'] = names
        if names != planned:
            report['errors'].append('Planned steps missing, extra, or out of order')
        if not result['completed']:
            report['errors'].append('Driver did not complete')
        else:
            report['status'] = 'FAIL'
        if not result['passed'] or any(not step['ok'] for step in steps):
            report['errors'].append('Driver assertions did not all pass')
        if not report['screenshots']:
            report['errors'].append('No rendered screenshot evidence')
        if not report['errors']:
            report['status'] = 'PASS'
    except (OSError, ValueError, KeyError, TypeError, zlib.error) as error:
        report['errors'].append(str(error))
    return report


class SaveProtection:
    """Durable backups first; never delete saves. Recovery is idempotent."""
    def __init__(self, out, directories):
        self.out = out
        self.journal = out / 'save-protection.json'
        self.records = []
        self.directories = directories
        self.backup_root = out / 'save-backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')

    def prepare(self):
        self.flush()
        for index, directory in enumerate(self.directories):
            manifest = tree_manifest(directory)
            backup = self.backup_root / str(index)
            if manifest is not None:
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copytree(directory, backup)
                if tree_manifest(backup) != manifest or tree_manifest(directory) != manifest:
                    raise ValueError(f'Backup verification failed: {directory}')
            self.records.append({'path': str(directory), 'backup': str(backup),
                                 'manifest': manifest, 'restored': False})
            self.flush()

    def flush(self):
        write_json(self.journal, {'schema': 1, 'records': self.records})

    def restore(self):
        for index, record in enumerate(self.records):
            if record['restored']:
                continue
            path, backup = Path(record['path']), Path(record['backup'])
            original = record['manifest']
            if original is not None and tree_manifest(backup) != original:
                raise ValueError(f'Backup changed; restore refused: {backup}')
            current = tree_manifest(path)
            if current != original:
                # Prepare a complete verified replacement BEFORE moving anything.
                stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
                staged = path.with_name(f'{path.name}.aw02-restore-{stamp}')
                if original is not None:
                    shutil.copytree(backup, staged)
                    if tree_manifest(staged) != original:
                        raise ValueError(f'Restore staging verification failed: {staged}')
                if path.exists():
                    quarantine = path.with_name(f'{path.name}.aw02-retained-{stamp}')
                    path.rename(quarantine)
                    record.setdefault('retained', []).append(str(quarantine))
                    self.flush()
                if original is not None:
                    staged.rename(path)
            if tree_manifest(path) != original:
                raise ValueError(f'Save restore verification failed: {path}')
            record['restored'] = True
            self.flush()


def find_exe(package):
    # Bypass UE's root bootstrap: waiting for it alone is not process completion.
    candidates = [p.resolve() for p in package.rglob('PerfectWorld.exe')
                  if p.parent.name.lower() == 'win64'
                  and p.parent.parent.name.lower() == 'binaries']
    if len(candidates) != 1:
        raise ValueError('Expected one PerfectWorld/Binaries/Win64/PerfectWorld.exe; '
                         'supply the single Windows package, not the root bootstrap alone')
    return candidates[0]


def package_manifest(package):
    extensions = {'.exe', '.dll', '.pak', '.utoc', '.ucas'}
    return {str(p.relative_to(package)): sha256(p) for p in sorted(package.rglob('*'))
            if p.is_file() and p.suffix.lower() in extensions
            and 'Saved' not in p.relative_to(package).parts}


class ProcessStillRunning(RuntimeError):
    pass


def require_game_stopped():
    output = subprocess.check_output(
        ['tasklist', '/FI', 'IMAGENAME eq PerfectWorld.exe', '/FO', 'CSV', '/NH'],
        text=True, errors='replace', timeout=30)
    if any(row and row[0].lower() == 'perfectworld.exe' for row in csv.reader(output.splitlines())):
        raise ProcessStillRunning('PerfectWorld.exe is still running; close it before run/recovery')


def run_process(command, cwd, console, timeout):
    """The direct game executable is waited, including after forced termination."""
    started = time.monotonic()
    code, timed_out = None, False
    with console.open('wb') as stream:
        process = subprocess.Popen(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                   env={k.upper(): v for k, v in os.environ.items()})
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
        finally:
            if process.poll() is None:
                # /T also stops any crash reporter/child process before restoring saves.
                try:
                    if os.name == 'nt':
                        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                                       stdout=stream, stderr=subprocess.STDOUT, timeout=30,
                                       check=False)
                finally:
                    if process.poll() is None:
                        process.kill()
                    try:
                        process.wait(timeout=30)
                    except subprocess.TimeoutExpired as error:
                        raise ProcessStillRunning(f'Cannot stop PID {process.pid}; saves left protected') from error
    return {'exit_code': code if code is not None else process.returncode,
            'timed_out': timed_out, 'seconds': round(time.monotonic() - started, 3)}


def summary_write(out, summary):
    write_json(out / 'summary.json', summary)
    lines = ['# Lane acceptance', '', f'Status: {summary["status"]}', '',
             f'Commit: {summary["commit"]}', f'Executable SHA-256: {summary["exe_sha256"]}', '',
             'Automated evidence only; image review and VM offscreen proof are still required.', '']
    for run in summary['runs']:
        lines.append(f'- {run["scenario"]} ({run["phase"]}): {run["status"]}')
        lines.extend(f'  - {error}' for error in run.get('errors', []))
    lines.extend(['', f'Save restoration verified: {summary.get("saves_restored", False)}'])
    lines.extend(f'- {error}' for error in summary.get('errors', []))
    temporary = out / 'summary.md.tmp'
    temporary.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    temporary.replace(out / 'summary.md')


def acceptance(args):
    if os.name != 'nt':
        raise ValueError('Packaged runs require Windows; --self-test is portable')
    package, out = args.package.resolve(), args.out.resolve()
    exe = find_exe(package)
    project = exe.parents[2]
    if out.is_relative_to(package) or package.is_relative_to(out):
        raise ValueError('Output must be separate from the package')
    if out.exists() and any(out.iterdir()):
        raise ValueError('Use a fresh empty --out directory (stale evidence is refused)')
    out.mkdir(parents=True, exist_ok=True)
    commit = args.commit or subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if not re.fullmatch(r'[0-9a-fA-F]{7,64}', commit):
        raise ValueError('Commit must be a hexadecimal git SHA')
    lock = Path(os.environ['LOCALAPPDATA']) / 'PerfectWorld' / '.pw-lane-acceptance.lock'
    summary = {'schema': 1, 'status': 'INCOMPLETE', 'commit': commit, 'exe': str(exe),
               'exe_sha256': sha256(exe), 'runs': [], 'errors': [],
               'created_utc': datetime.now(timezone.utc).isoformat(), 'lock': str(lock)}
    directories = [project / 'Saved' / 'SaveGames',
                   Path(os.environ['LOCALAPPDATA']) / 'PerfectWorld' / 'Saved' / 'SaveGames']
    directories.extend(p.resolve() for p in args.saved_dir)
    directories = list(dict.fromkeys(directories))
    for path in directories:
        if out.is_relative_to(path) or path.is_relative_to(out):
            raise ValueError('Protected owner saves must be separate from output')
        if any(path != other and (path.is_relative_to(other) or other.is_relative_to(path))
               for other in directories):
            raise ValueError('Protected save directories must not overlap')
        # Reject junctions in ancestors as well as within SaveGames itself.
        for ancestor in (path, *path.parents):
            if ancestor.exists() and is_link(ancestor):
                raise ValueError(f'Protected path contains a link: {ancestor}')
    require_game_stopped()
    guard = SaveProtection(out, directories)
    summary_write(out, summary)
    guard.flush()
    # One lock per Windows user, shared even by different package directories.
    lock.parent.mkdir(parents=True, exist_ok=True)
    with lock.open('x', encoding='utf-8') as stream:
        stream.write(str(out))
        stream.flush()
        os.fsync(stream.fileno())
    try:
        summary_write(out, summary)
        identity = package_manifest(package)
        write_json(out / 'package-manifest.json', identity)
        guard.prepare()  # no game process can start before all copies verify
        summary['save_protection'] = str(guard.journal)
        for scenario in args.scenario:
            phases = [('part1', 'new'), ('part2', 'continue')] if args.restart_pair else [('single', args.start)]
            sandbox = out / scenario / 'user'
            sandbox.mkdir(parents=True)
            for phase, start in phases:
                evidence = out / scenario / phase
                evidence.mkdir()
                if package_manifest(package) != identity:
                    raise ValueError('Package changed before launch')
                command = [str(exe), *FLAGS, f'-UserDir={sandbox.as_posix()}/',
                           f'-PWAcceptance={scenario}', f'-PWAcceptanceOut={evidence.as_posix()}',
                           f'-PWAcceptanceCommit={commit}', f'-PWAcceptanceStart={start}',
                           '-PWAcceptanceSlot=0']
                run = {'scenario': scenario, 'phase': phase, 'status': 'INCOMPLETE',
                       'command': command, 'exe_sha256': summary['exe_sha256'],
                       'evidence': str(evidence), 'errors': []}
                summary['runs'].append(run)
                summary_write(out, summary)  # power loss leaves an INCOMPLETE run
                require_game_stopped()
                try:
                    run.update(run_process(command, exe.parent, evidence / 'console.log', args.timeout_s))
                finally:
                    run['logs'] = []
                    # Private log is primary; retain normal package/user logs as fallback.
                    for index, log in enumerate([sandbox / 'Saved/Logs/PerfectWorld.log',
                                                 project / 'Saved/Logs/PerfectWorld.log',
                                                 Path(os.environ['LOCALAPPDATA']) /
                                                 'PerfectWorld/Saved/Logs/PerfectWorld.log']):
                        if log.is_file():
                            target = evidence / f'PerfectWorld.{index}.log'
                            shutil.copy2(log, target)
                            run['logs'].append(str(target))
                run.update(verify_result(evidence, scenario, commit))
                if run['timed_out'] or run['exit_code'] != 0:
                    run['status'] = 'INCOMPLETE'
                    run['errors'].append('Timeout or nonzero process exit')
                if package_manifest(package) != identity:
                    run['status'] = 'FAIL'
                    run['errors'].append('Package identity changed during run')
                # Runtime proof: save files in private user directory, owner trees unchanged.
                isolated = tree_manifest(sandbox / 'Saved/SaveGames')
                run['isolation_observed'] = bool(isolated) and any(
                    value != '<directory>' for value in (isolated or {}).values())
                if not run['isolation_observed']:
                    run['status'] = 'INCOMPLETE'
                    run['errors'].append('No save file observed in isolated Saved/SaveGames')
                changed = [r['path'] for r in guard.records
                           if tree_manifest(Path(r['path'])) != r['manifest']]
                if changed:
                    run['status'] = 'FAIL'
                    run['errors'].append(f'Owner saves changed; backup restoration required: {changed}')
                summary_write(out, summary)
                if run['status'] != 'PASS':
                    break  # never launch restart part 2 after unclean/incomplete part 1
            if summary['runs'][-1]['status'] != 'PASS':
                break
    except (Exception, KeyboardInterrupt) as error:
        summary['errors'].append(f'{type(error).__name__}: {error}')
    finally:
        try:
            require_game_stopped()  # never restore into a still-running game
            guard.restore()
            summary['saves_restored'] = True
            lock.unlink()
        except Exception as error:
            summary['errors'].append(f'Save restoration failed: {error}; use --recover {out}')
        if (summary['runs'] and all(r['status'] == 'PASS' for r in summary['runs'])
                and not summary['errors'] and summary.get('saves_restored')):
            summary['status'] = 'PASS'
        elif any(r['status'] == 'FAIL' for r in summary['runs']):
            summary['status'] = 'FAIL'
        summary_write(out, summary)
    print(f'{summary["status"]}: {out / "summary.md"}')
    return 0 if summary['status'] == 'PASS' else 1


def recover(out):
    out = out.resolve()
    summary = json.loads((out / 'summary.json').read_text(encoding='utf-8'))
    lock = Path(summary['lock'])
    if not lock.exists() or lock.read_text(encoding='utf-8') != str(out):
        raise ValueError('Recovery lock does not match this output')
    require_game_stopped()
    guard = SaveProtection(out, [])
    guard.records = json.loads(guard.journal.read_text(encoding='utf-8'))['records']
    guard.restore()
    lock.unlink()
    summary['saves_restored'] = True
    summary['status'] = 'INCOMPLETE'
    summary.setdefault('errors', []).append('Recovered interrupted run; acceptance must be rerun')
    summary_write(out, summary)
    print('Recovery complete; backups retained. Acceptance remains INCOMPLETE.')
    return 0


def fixture_png(path, channels=3, solid=False, mode=0):
    width, height = 32, 32
    rows = [bytes((137, 31, 52, 255)[x % channels] if solid else
                  (x * 37 + y * 19 + x * y) % 256 for x in range(width * channels))
            for y in range(height)]
    raw = bytearray()
    previous = bytes(width * channels)
    for row in rows:
        raw.append(mode)
        for x, value in enumerate(row):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            predictor = (0, left, up, (left + up) // 2, paeth(left, up, corner))[mode]
            raw.append((value - predictor) & 255)
        previous = row

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))

    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
        '>IIBBBBB', width, height, 8, 2 if channels == 3 else 6, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(bytes(raw), level=0)) + chunk(b'IEND', b''))


def self_test():
    checks = 0

    def check(condition, label):
        nonlocal checks
        if not condition:
            raise AssertionError(label)
        checks += 1

    scratch = ROOT / 'Saved' / 'AW02SelfTest'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pw-aw02-', dir=scratch) as temporary:
        out = Path(temporary)
        for channels in (3, 4):
            for mode in range(5):
                path = out / 'filters.png'
                fixture_png(path, channels, mode=mode)
                check(png_check(path)['nonuniform_rgb'], f'RGB/RGBA filter {mode}')
        fixture_png(out / 'boot__shot_view.png')
        valid = {'schema': 1, 'scenario': 'boot', 'commit': 'abc1234',
                 'planned_steps': ['world_ready', 'shot_view'],
                 'steps': [{'name': 'world_ready', 'ok': True, 'detail': '', 'screenshot': ''},
                           {'name': 'shot_view', 'ok': True, 'detail': '',
                            'screenshot': 'boot__shot_view.png'}], 'completed': True, 'passed': True}

        def verify(value):
            write_json(out / 'boot.json', value)
            return verify_result(out, 'boot', 'abc1234')['status']

        check(verify(valid) == 'PASS', 'valid result')
        variants = []
        for field, value in [('schema', 2), ('schema', True), ('commit', 'wrong'),
                             ('scenario', 'wrong'), ('completed', False), ('passed', False),
                             ('completed', 1), ('planned_steps', []), ('steps', []),
                             ('steps', list(reversed(valid['steps'])))]:
            changed = copy.deepcopy(valid)
            changed[field] = value
            variants.append(changed)
        for value in variants:
            check(verify(value) != 'PASS', 'reject malformed/incomplete result')
        for shot in ('missing.png', '../escape.png', ''):
            changed = copy.deepcopy(valid)
            changed['steps'][1]['screenshot'] = shot
            check(verify(changed) != 'PASS', 'reject missing/unsafe screenshot')
        (out / 'boot__shot_view.png').rename(out / 'held.png')
        check(verify(valid) != 'PASS', 'reject absent contracted screenshot')
        (out / 'held.png').rename(out / 'boot__shot_view.png')
        for content in (b'{}', b'{broken', b'[]'):
            (out / 'boot.json').write_bytes(content)
            check(verify_result(out, 'boot', 'abc1234')['status'] != 'PASS', 'bad JSON/schema')
        (out / 'boot.json').unlink()
        check(verify_result(out, 'boot', 'abc1234')['status'] == 'INCOMPLETE', 'missing result')
        for channels in (3, 4):
            fixture_png(out / 'boot__shot_view.png', channels, solid=True)
            check(verify(valid) != 'PASS', 'reject uniform colour')
        (out / 'boot__shot_view.png').write_bytes(b'tiny')
        check(verify(valid) != 'PASS', 'reject small PNG')
        fixture_png(out / 'boot__shot_view.png')
        damaged = bytearray((out / 'boot__shot_view.png').read_bytes())
        damaged[-5] ^= 1
        (out / 'boot__shot_view.png').write_bytes(damaged)
        check(verify(valid) != 'PASS', 'reject corrupt PNG')
        # Exercise real backup/restore on temporary fake owner slots, never a game.
        owner, absent = out / 'owner/SaveGames', out / 'absent/SaveGames'
        owner.mkdir(parents=True)
        (owner / 'slot.json').write_bytes(b'owner save')
        guard = SaveProtection(out, [owner, absent])
        guard.prepare()
        (owner / 'slot.json').write_bytes(b'acceptance overwrote slot')
        absent.mkdir(parents=True)
        (absent / 'slot.json').write_bytes(b'new test slot')
        guard.restore()
        check((owner / 'slot.json').read_bytes() == b'owner save', 'restore owner bytes')
        check(not absent.exists(), 'restore original absence')
        check(tree_manifest(Path(guard.records[0]['backup'])) == guard.records[0]['manifest'],
              'backup retained')
        guard.restore()
        check((owner / 'slot.json').read_bytes() == b'owner save', 'idempotent restore')
        guard = SaveProtection(out, [owner])
        guard.prepare()
        (Path(guard.records[0]['backup']) / 'slot.json').write_bytes(b'corrupt backup')
        refused = False
        try:
            guard.restore()
        except ValueError:
            refused = True
        check(refused and (owner / 'slot.json').read_bytes() == b'owner save',
              'damaged backup refuses restoration without touching owner')
        first = run_process([sys.executable, '-c', 'raise SystemExit(0)'], out,
                            out / 'clean-exit.log', 10)
        check(first['exit_code'] == 0 and not first['timed_out'], 'clean helper process exit')
        failed = run_process([sys.executable, '-c', 'raise SystemExit(7)'], out,
                             out / 'failed-exit.log', 10)
        check(failed['exit_code'] == 7 and not failed['timed_out'], 'nonzero helper exit')
        stalled = run_process([sys.executable, '-c', 'import time; time.sleep(60)'], out,
                              out / 'timeout.log', 0.25)
        check(stalled['timed_out'] and stalled['exit_code'] != 0, 'stalled helper terminated')
    print(f'SELF-TEST PASS: {checks} checks (PNG filters, evidence parser, save backup/restore).')
    print('No game launched; helper exit/timeout checks passed. Packaged offscreen/restart remain unverified.')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--scenario', action='append')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--commit')
    parser.add_argument('--timeout-s', type=float, default=240)
    parser.add_argument('--start', choices=['auto', 'new', 'continue'], default='auto')
    parser.add_argument('--restart-pair', action='store_true',
                        help='Same scenario twice, new then continue, separate evidence, shared private saves')
    parser.add_argument('--saved-dir', type=Path, action='append', default=[],
                        help='Additional real SaveGames folder to protect (repeatable)')
    parser.add_argument('--recover', type=Path, help='Restore a stopped/interrupted run; launches no game')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.recover:
        return recover(args.recover)
    if not args.package or not args.out or not args.scenario:
        parser.error('--package, --scenario and --out are required unless --self-test/--recover')
    if not math.isfinite(args.timeout_s) or args.timeout_s <= 0:
        parser.error('--timeout-s must be finite and positive')
    if len(set(args.scenario)) != len(args.scenario) or any(not NAME.fullmatch(s) for s in args.scenario):
        parser.error('Use unique scenario names containing 1-64 lowercase letters, digits or underscores')
    if args.restart_pair and (len(args.scenario) != 1 or args.scenario[0] not in RESTART_PAIR_SCENARIOS or args.start != 'auto'):
        parser.error('--restart-pair requires exactly one of --scenario ' + ' / '.join(RESTART_PAIR_SCENARIOS) +
                     ' and default --start auto')
    return acceptance(args)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, ProcessStillRunning, subprocess.SubprocessError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        raise SystemExit(2)
