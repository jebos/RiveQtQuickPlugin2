import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


BOOTSTRAP_PATH = Path(__file__).resolve().parents[2] / "scripts" / "bootstrap.py"
SPEC = importlib.util.spec_from_file_location("bootstrap", BOOTSTRAP_PATH)
bootstrap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bootstrap)


class DependencyDiscoveryTests(unittest.TestCase):
    def test_yoga_revision_file_and_literal_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "dependencies").mkdir()
            (root / "dependencies/yoga.ref").write_bytes(b"rive_yoga_fb91f6cb0f8f\r\n")
            (root / "dependencies/harfbuzz.lua").write_text(
                "harfbuzz = dependency.github('rive-app/harfbuzz', 'rive_13.1.1')\n",
                encoding="utf-8",
            )
            specs = [bootstrap.PREMAKE_DEPENDENCY_SPECS[2], {
                "premake": "dependencies/harfbuzz.lua",
                "variable": "harfbuzz",
                "path": "3rdparty/harfbuzz",
                "check": "src/harfbuzz.cc",
            }]
            with patch.object(bootstrap, "PREMAKE_DEPENDENCY_SPECS", specs):
                entries = bootstrap.gather_rive_dependencies(root)

            self.assertEqual(entries, [{
                "path": "3rdparty/yoga",
                "url": "https://github.com/rive-app/yoga.git",
                "rev": "rive_yoga_fb91f6cb0f8f",
                "check": "yoga/Yoga.h",
            }, {
                "path": "3rdparty/harfbuzz",
                "url": "https://github.com/rive-app/harfbuzz.git",
                "rev": "rive_13.1.1",
                "check": "src/harfbuzz.cc",
            }])

    def test_empty_revision_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "dependencies").mkdir()
            (root / "dependencies/yoga.ref").write_text(" \n", encoding="utf-8")
            with patch.object(bootstrap, "PREMAKE_DEPENDENCY_SPECS",
                              [bootstrap.PREMAKE_DEPENDENCY_SPECS[2]]):
                with self.assertRaisesRegex(RuntimeError, "Empty dependency revision"):
                    bootstrap.gather_rive_dependencies(root)


if __name__ == "__main__":
    unittest.main()
