"""Exercise the real installer under Steam's spaces/parentheses path layout."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class InstallerTest(unittest.TestCase):
    def test_new_and_existing_installations(self):
        script = Path(__file__).resolve().parents[2] / "Mod" / "install.bat"
        for upgrade in (False, True):
            with self.subTest(upgrade=upgrade), tempfile.TemporaryDirectory(prefix="ds-installer-") as tmp:
                root = Path(tmp).resolve()
                game = root / "Program Files (x86)" / "Don't Starve Together"
                mod = game / "mods" / "Luajit"
                self.assertTrue(mod.resolve().is_relative_to(root))
                source = mod / "bin64" / "windows"
                source.mkdir(parents=True)
                (game / "bin64").mkdir()
                shutil.copy2(script, mod / "install.bat")
                (source / "Winmm.dll").write_bytes(b"fixture shell; never executed")
                (source / "Injector.dll").write_bytes(b"fixture injector; never executed")
                (mod / "plugins").mkdir()
                (mod / "deps").mkdir()
                state = game / "data" / "unsafedata"
                if upgrade:
                    state.mkdir(parents=True)
                    (state / "luajit_crash.json").write_text("{1}", encoding="utf-8")
                    (state / "luajit_config.json").write_text(
                        '{"AlwaysEnableMod":true}', encoding="utf-8")
                for _ in range(2):
                    result = subprocess.run(["cmd.exe", "/d", "/c", "install.bat"],
                                            cwd=mod, capture_output=True, timeout=30)
                    self.assertEqual(result.returncode, 0,
                                     result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
                    self.assertEqual((game / "bin64" / "Winmm.dll").read_bytes(),
                                     b"fixture shell; never executed")
                    self.assertEqual((mod / "Injector.dll").read_bytes(),
                                     b"fixture injector; never executed")
                    marker = (state / "ds_luajit_injector.path").read_text(encoding="utf-8").strip()
                    self.assertEqual(Path(marker).resolve(), (mod / "Injector.dll").resolve())
                    self.assertFalse((state / "luajit_crash.json").exists())
                if upgrade:
                    archives = list(state.glob("luajit_crash.install-backup.*.json"))
                    self.assertEqual(len(archives), 1)
                    self.assertEqual(archives[0].read_text(encoding="utf-8"), "{1}")
                    self.assertTrue(json.loads((state / "luajit_config.json").read_text(encoding="utf-8"))
                                    ["AlwaysEnableMod"])


if __name__ == "__main__":
    unittest.main()
