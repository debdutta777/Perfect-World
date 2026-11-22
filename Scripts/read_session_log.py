#!/usr/bin/env python3
"""P509: reconstruct local pw.slice_session/1 JSON Lines (files or Saved/SessionLogs).

    python Scripts/read_session_log.py Saved/SessionLogs

Only recorded IDs and receipts are interpreted. Absence of a command never means
an opportunity was refused. A crash may leave an incomplete final line; it is
reported and the earlier complete observations remain readable.
"""

import argparse
from datetime import datetime, timedelta
import json
from pathlib import Path
import sys


def game_time(minutes):
    try:
        return (datetime(2042, 6, 2, 6) + timedelta(minutes=int(minutes))).strftime("%Y-%m-%d %H:%M")
    except (ValueError, TypeError, OverflowError):
        return str(minutes)


def read_rows(path):
    rows = []
    try:
        lines = path.read_text(encoding="utf-8-sig").splitlines()
    except (OSError, UnicodeError) as error:
        print(f"{path.name}: {error}", file=sys.stderr)
        return rows
    last_order = 0
    for number, line in enumerate(lines, 1):
        try:
            row = json.loads(line)
            if not isinstance(row, dict) or row.get("schema") != "pw.slice_session/1":
                raise ValueError("unsupported session schema")
            if "order" in row:
                order = int(row["order"])
                if order <= last_order:
                    raise ValueError("observation order did not increase")
                last_order = order
            rows.append(row)
        except (ValueError, TypeError) as error:
            print(f"{path.name}:{number}: omitted unreadable observation ({error})", file=sys.stderr)
    return rows


def describe(row, saved):
    kind = row.get("type")
    if kind == "boundary":
        action = row.get("boundary", "unknown")
        message = f"{action}; world {row.get('world_hash', '?')}; seed {row.get('world_seed', '?')}"
        if action in {"load", "continue", "load_or_continue", "world_adopted", "restart"}:
            previous = saved.get(row.get("saved_hash", row.get("world_hash")), [])
            previous = [item for item in previous if item[0] != row.get("session")]
            if previous:
                message += f"; resumes saved session {previous[-1][0]}"
            message += f"; next step: {row.get('next_step', 'unrecorded')}"
        return message
    if kind == "command":
        command = row.get("command_id", "?")
        action = row.get("action", "")
        ids = [f"{key}={row[key]}" for key in (
            "work_id", "appointment_id", "activity_id", "opportunity_id",
            "obligation_id", "route_id", "choice_id", "binding_id", "station_id",
            "order_id", "claim_id", "cause_id", "test_id", "member_id", "limitation_id"
        ) if row.get(key)]
        detail = " ".join([command, action, *ids]).strip()
        for key in ("link_ids", "evidence_ids", "assignments"):
            if row.get(key):
                detail += f"; {key}={row[key]}"
        if not row.get("accepted"):
            return f"Command refused: {detail} ({row.get('refusal_code', '?')}); no outcome"
        if action in {"decline", "cancel", "leave", "withdraw"}:
            detail = f"Refused/released opportunity: {detail}"
        minutes = row.get("charged_minutes", 0)
        if minutes:
            detail += f"; time charge {minutes} game min"
        return detail
    if kind == "receipt":
        lost = [f"{item['id']} ({item.get('overlap_minutes', '?')} min, source {item.get('source', '?')})"
                for item in row.get("displaced", [])]
        detail = f"Receipt {row.get('receipt_seq')}: {row.get('activity_id')} ({row.get('minutes', 0)} min)"
        detail += "; set aside: " + (", ".join(lost) if lost else "no recorded displaced alternatives")
        if row.get("omitted_count"):
            detail += f"; {row['omitted_count']} further alternatives, {row.get('omitted_overlap_minutes', 0)} overlap min"
        return detail
    if kind == "receipt_totals":
        return (f"Receipt totals (snapshot): {row.get('committed_minutes', 0)} committed min; "
                f"{row.get('displaced', 0)} displaced alternatives, {row.get('overlap_minutes', 0)} overlap min; "
                "further detail is bounded by the core receipt rules")
    if kind == "followup_outcome":
        return (f"Follow-up outcome: {row.get('task_id')} / {row.get('choice_id')}; "
                f"records: {', '.join(row.get('records', []))}")
    if kind == "event":
        event = row.get("event_id", "")
        if event == "exam.transition":
            return (f"ExamTransition: sitting {row.get('sitting')} {row.get('transition')} "
                    f"-> {row.get('participation')}; outside {row.get('outside_minutes', 0)} min; "
                    f"late {row.get('late_minutes', 0)} min; time called {row.get('time_called', False)}")
        if event == "day.event.completed":
            return f"Completed: {row.get('activity_id')} -> {row.get('outcome')} (instance {row.get('instance')})"
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="+", help="JSONL files or a SessionLogs directory")
    args = parser.parse_args(argv)
    files = set()
    for path in args.paths:
        files.update(path.glob("session_*.jsonl") if path.is_dir() else [path])
    sessions = [(path, read_rows(path)) for path in sorted(files)]
    sessions = [(path, rows) for path, rows in sessions if rows]
    sessions.sort(key=lambda item: (item[1][0].get("started_utc", ""), item[0].name))
    if not sessions:
        print("No readable session observations.", file=sys.stderr)
        return 1
    saved = {}
    for path, rows in sessions:
        header = rows[0]
        print(f"\nSession {header.get('session', '?')} ({path.name}), {header.get('started_utc', '?')}")
        print(f"Build {header.get('build', '?')}; executable {header.get('executable_sha1', '?')}; "
              f"manifest {header.get('manifest_sha1', '?')}; content {header.get('content_sha1', '?')}")
        for row in rows:
            if row.get("dropped_before"):
                print(f"  GAP: {row['dropped_before']} observations dropped; reconstruction is partial")
            detail = describe(row, saved)
            if detail:
                elapsed = int(row.get("elapsed_ms", 0)) / 1000
                print(f"  +{elapsed:8.3f}s | {game_time(row.get('game_time'))} | {detail}")
            if row.get("type") == "boundary" and row.get("boundary") == "save":
                saved.setdefault(row.get("world_hash"), []).append((row.get("session"), row.get("order")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
