import importlib.util
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "build.py"
spec = importlib.util.spec_from_file_location("midnight_build", MODULE)
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildPreparationTests(unittest.TestCase):
    def test_external_juce_and_isolated_output_in_generated_exporter(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = build.prepare_project(Path("D:/Projects/Dev/JUCE"), Path(temporary))
            text = path.read_text()
            self.assertNotIn(r"..\..\JUCE\modules", text)
            tree = ET.parse(path)
            for output in tree.findall(".//m:OutDir", build.NS):
                self.assertIn("MidnightNative", output.text)
            for name in tree.findall(".//m:TargetName", build.NS):
                self.assertEqual(name.text, "MidnightNativePOC")
            for output in tree.findall(".//m:Link/m:OutputFile", build.NS):
                self.assertEqual(output.text, "$(OutDir)$(TargetName).exe")
            self.assertTrue((Path(temporary) / "resources.rc").is_file())
            self.assertTrue((Path(temporary) / "icon.ico").is_file())

    def test_baseline_keeps_source_and_output_separate(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = build.prepare_project(Path("D:/Projects/Dev/JUCE"), Path(temporary), baseline=True)
            tree = ET.parse(path)
            for output in tree.findall(".//m:OutDir", build.NS):
                self.assertIn("ChataigneBaseline", output.text)
            for name in tree.findall(".//m:TargetName", build.NS):
                self.assertEqual(name.text, "Chataigne")

    def test_missing_juce_reports_blocker(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, issues = build.preflight(Path(temporary))
            self.assertTrue(any("JUCE" in issue for issue in issues))


if __name__ == "__main__":
    unittest.main()
