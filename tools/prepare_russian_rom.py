#!/usr/bin/env python3
"""Prepare the pinned Russian ROM locally from a USA dump and a BPS patch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile

from translation_bps import apply

ROOT = Path(__file__).resolve().parents[1]


def prepare(source_path, patch_path, output_path):
    release = json.loads((ROOT / "translation/release.json").read_text(encoding="utf8"))
    source = source_path.read_bytes()
    patch = patch_path.read_bytes()
    if hashlib.sha256(source).hexdigest() != release["source_sha256"]:
        raise ValueError("Нужен чистый ROM Final Fantasy Tactics Advance (USA).")
    if hashlib.sha256(patch).hexdigest() != release["patch_sha256"]:
        raise ValueError("Нужен BPS-патч Maksami audit-1.26 с указанной контрольной суммой.")
    target = apply(source, patch)
    if len(target) != release["target_size"]:
        raise ValueError("Неверный размер результата BPS.")
    if hashlib.sha256(target).hexdigest() != release["target_sha256"]:
        raise ValueError("Контрольная сумма русской ROM не совпала.")
    output_path = output_path.resolve()
    if output_path in (source_path.resolve(), patch_path.resolve()):
        raise ValueError("Для результата выберите отдельный файл.")
    if output_path.exists():
        if output_path.read_bytes() == target:
            return
        raise ValueError("Выходной файл уже существует и отличается от audit-1.26.")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output_path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(target)
            stream.flush()
            os.fsync(stream.fileno())
        # Exclusive creation also protects an output created concurrently.
        os.link(temporary, output_path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--patch", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "game.gba")
    args = parser.parse_args()
    try:
        prepare(args.source, args.patch, args.out)
    except (OSError, ValueError, AssertionError) as error:
        parser.exit(1, f"Не удалось подготовить ROM: {error}\n")
    print("Русская ROM audit-1.26 подготовлена и проверена.")


if __name__ == "__main__":
    main()
