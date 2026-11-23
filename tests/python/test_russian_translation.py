import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
import prepare_russian_rom

PATCH = bytes.fromhex("425053318384808d61625863c24124358ab7544f372d8e9a")


class RussianPreparationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root / "translation").mkdir()
        self.source = self.root / "source.gba"
        self.patch_file = self.root / "translation.bps"
        self.output = self.root / "game.gba"
        self.source.write_bytes(b"abc")
        self.patch_file.write_bytes(PATCH)
        release = {
            "source_sha256": hashlib.sha256(b"abc").hexdigest(),
            "patch_sha256": hashlib.sha256(PATCH).hexdigest(),
            "target_sha256": hashlib.sha256(b"abXc").hexdigest(),
            "target_size": 4,
        }
        (self.root / "translation/release.json").write_text(json.dumps(release))
        self.root_override = patch.object(prepare_russian_rom, "ROOT", self.root)
        self.root_override.start()

    def tearDown(self):
        self.root_override.stop()
        self.directory.cleanup()

    def prepare(self, output=None):
        prepare_russian_rom.prepare(
            self.source, self.patch_file, output or self.output
        )

    def test_known_patch_and_idempotent_output(self):
        self.prepare()
        self.prepare()
        self.assertEqual(self.output.read_bytes(), b"abXc")
        self.assertEqual(self.source.read_bytes(), b"abc")
        self.assertEqual(self.patch_file.read_bytes(), PATCH)

    def test_wrong_source_leaves_no_output(self):
        self.source.write_bytes(b"abd")
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_wrong_patch_leaves_no_output(self):
        self.patch_file.write_bytes(PATCH[:-1] + b"0")
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_existing_different_output_is_preserved(self):
        self.output.write_bytes(b"existing")
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertEqual(self.output.read_bytes(), b"existing")

    def test_input_cannot_be_output(self):
        with self.assertRaises(ValueError):
            self.prepare(self.source)
        self.assertEqual(self.source.read_bytes(), b"abc")


class RussianIdentityTests(unittest.TestCase):
    def test_target_is_pinned_across_platforms(self):
        release = json.loads(
            (REPO / "translation/release.json").read_text(encoding="utf8")
        )
        sha = release["target_sha1"]
        for filename in (
            "game.toml", "android/game_android.toml", "android/app/build.gradle",
            "build.py", "build_apk.py", "src/main.cpp",
        ):
            with self.subTest(filename=filename):
                self.assertIn(sha, (REPO / filename).read_text(encoding="utf8"))


if __name__ == "__main__":
    unittest.main()
