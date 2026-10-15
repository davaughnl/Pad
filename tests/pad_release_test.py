"""Fast release-contract gates, with no third-party Python dependencies."""
import os
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseContractTests(unittest.TestCase):
    def test_release_waits_for_both_platforms_and_stays_draft(self):
        workflow = (ROOT / '.github/workflows/release.yml').read_text()
        self.assertIn('needs: [windows, linux]', workflow)
        self.assertIn('gh release create "$GITHUB_REF_NAME" --draft --verify-tag', workflow)
        self.assertIn("if: github.event_name == 'push'", workflow)
        self.assertIn('workflow_dispatch:', workflow)
        self.assertNotIn('--draft=false', workflow)
        self.assertIn('sha256sum -c -- *.sha256', workflow)
        for asset in ('Windows-x64-Setup.exe', 'Windows-x64.zip', 'Ubuntu-22.04-amd64.deb'):
            self.assertIn(asset, workflow)

    def test_source_version_matches_requested_release(self):
        expected = os.environ.get('PAD_VERSION')
        if not expected:
            self.skipTest('A version is required only in release validation builds')
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        default = re.search(r'set\(PAD_VERSION "([^"]*)" CACHE STRING', cmake)
        self.assertIsNotNone(default)
        self.assertEqual(default.group(1), expected, 'Tag must match source release version')

    def test_linux_release_package_validates_compiled_version(self):
        script = (ROOT / 'packaging/linux/package.sh').read_text()
        self.assertIn('PAD_VERSION:STRING=$version', script)
        self.assertIn('CPACK_DEBIAN_PACKAGE_VERSION=$package_version', script)
        self.assertIn('dpkg-deb -f "${packages[0]}" Version', script)
        self.assertIn('dpkg-deb -f "${packages[0]}" Architecture', script)

    def test_installer_identity_and_version_metadata(self):
        script = (ROOT / 'packaging/windows/installer/pad.nsi').read_text()
        for entry in ('Name "Pad"', 'VIProductVersion "${VERSION}.0"',
                      'VIAddVersionKey "ProductVersion" "${VERSION}"',
                      '"DisplayVersion" "${VERSION}"'):
            self.assertIn(entry, script)
        self.assertIn('"$INSTDIR\\bin\\pad.exe"', script)

    def test_updater_asset_names_match_packages(self):
        source = (ROOT / 'src/pad/updatemanager.cpp').read_text()
        self.assertIn('https://api.github.com/repos/davaughnl/Pad/releases/latest', source)
        self.assertIn('Pad-%1-Windows-x64-Setup.exe', source)
        self.assertIn('QStringLiteral(".sha256")', source)


if __name__ == '__main__':
    unittest.main()
