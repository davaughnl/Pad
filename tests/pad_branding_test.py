"""Pad identity/resource gates. Run with python3 -m unittest discover -s tests -p 'pad_branding_test.py'."""
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class PadBrandingTests(unittest.TestCase):
    def test_desktop_matches_executable_and_app_id(self):
        desktop = (ROOT / 'other/io.github.davaughnl.Pad.desktop').read_text()
        self.assertIn('Name=Pad\n', desktop)
        self.assertIn('Exec=pad --show %f\n', desktop)
        self.assertIn('Icon=io.github.davaughnl.Pad\n', desktop)
        main = (ROOT / 'src/main.cpp').read_text()
        self.assertIn('setApplicationName("Pad")', main)
        self.assertIn('setDesktopFileName("io.github.davaughnl.Pad")', main)

    def test_resource_paths_exist(self):
        files = ET.parse(ROOT / 'src/resources.qrc').findall('.//file')
        self.assertGreater(len(files), 50)
        for item in files:
            with self.subTest(resource=item.text):
                self.assertTrue((ROOT / 'src' / item.text).is_file())

    def test_packaging_outputs_pad(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('OUTPUT_NAME pad', cmake)
        self.assertIn('set(CPACK_PACKAGE_NAME "Pad")', cmake)
        self.assertIn('set(CPACK_PACKAGE_EXECUTABLES "pad;Pad")', cmake)
        self.assertIn('set(CPACK_CREATE_DESKTOP_LINKS "pad")', cmake)
        self.assertNotIn('bin/antimicrox.exe', cmake)

    def test_appdata_is_fork_not_upstream_release(self):
        root = ET.parse(ROOT / 'other/appdata/io.github.antimicrox.antimicrox.appdata.xml.in').getroot()
        self.assertEqual(root.findtext('name'), 'Pad')
        self.assertEqual(root.findtext('id'), 'io.github.davaughnl.Pad')
        self.assertEqual(root.findtext('launchable'), 'io.github.davaughnl.Pad.desktop')
        self.assertIsNone(root.find('releases'))
        self.assertIsNone(root.find('screenshots'))

    def test_translation_catalogs_keep_contexts_and_brand(self):
        catalogs = list((ROOT / 'share/antimicrox/translations').glob('*.ts'))
        self.assertGreater(len(catalogs), 20)
        for path in catalogs:
            root = ET.parse(path).getroot()
            with self.subTest(catalog=path.name):
                self.assertIn('MainWindow', [c.findtext('name') for c in root.findall('context')])
                for message in root.findall('.//message'):
                    source = message.findtext('source') or ''
                    translation = message.find('translation')
                    translated = ''.join(translation.itertext()) if translation is not None else ''
                    if 'Pad' in source and not any(x in source for x in ('Later project', 'Originally developed')):
                        self.assertIsNone(re.search(r'(?<![/])AntiMicroX(?![/])', translated), path.name + ': ' + source[:80])

    def test_licenses_are_installed_and_packaged(self):
        for name in ('LICENSE', 'src/pad/OFL.txt', 'src/pad/LUCIDE-LICENSE.txt', 'packaging/windows/Qt-LGPL-3.0.txt'):
            self.assertGreater((ROOT / name).stat().st_size, 100)
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('licenses/Pad', cmake)
        package = (ROOT / 'packaging/windows/package.ps1').read_text()
        for name in ('Pad-GPL-3.0.txt', 'Geist-OFL.txt', 'Lucide-ISC.txt', 'Qt-LGPL-3.0.txt'):
            self.assertIn(name, package)

    def test_secondary_ui_icons_do_not_use_desktop_themes(self):
        for path in (ROOT / 'src/gui').glob('*.ui'):
            with self.subTest(dialog=path.name):
                self.assertNotIn('<iconset theme=', path.read_text())
        shell = (ROOT / 'src/pad/padshell.cpp').read_text()
        self.assertIn('QProxyStyle', shell)
        self.assertIn('SP_DialogCancelButton', shell)
        settings = (ROOT / 'src/gui/mainsettingsdialog.ui').read_text()
        self.assertIn('Your default app is not changed.', settings)

    def test_upstream_credits_and_profile_engine_retained(self):
        self.assertIn('AntiMicroX', (ROOT / 'README.upstream.md').read_text())
        self.assertIn('Travis Nickles', (ROOT / 'src/gui/aboutdialog.ui').read_text())
        self.assertIn('https://github.com/AntiMicroX/antimicrox', (ROOT / 'src/gui/aboutdialog.ui').read_text())
        self.assertTrue((ROOT / 'src/xml/inputdevicexml.cpp').is_file())
        self.assertIn('GPL', (ROOT / 'README.md').read_text())

if __name__ == '__main__':
    unittest.main()
