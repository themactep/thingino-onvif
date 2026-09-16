#!/usr/bin/env python3
"""Regression tests for advertising ONVIF FOV-relative pan/tilt."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLACEHOLDER = "%PANTILT_RELATIVE_DEFAULT_SPACE%"
TEMPLATES = [
    "res/media_service_files/GetProfile_PTZ.xml",
    "res/media2_service_files/GetProfiles_PTZ.xml",
    "res/ptz_service_files/GetConfigurations.xml",
    "res/ptz_service_files/GetConfiguration.xml",
    "res/ptz_service_files/GetCompatibleConfigurations.xml",
]


class FovDefaultSpaceTest(unittest.TestCase):
    def test_space_selection_requires_both_fov_dimensions(self):
        source = r'''
#include <assert.h>
#include <string.h>
#include "ptz_service.h"

int main(void) {
    assert(strcmp(ptz_default_relative_pantilt_space(90.0, 50.0),
                  PANTILT_RELATIVE_FOV_SPACE_URI) == 0);
    assert(strcmp(ptz_default_relative_pantilt_space(90.0, 0.0),
                  PANTILT_RELATIVE_GENERIC_SPACE_URI) == 0);
    assert(strcmp(ptz_default_relative_pantilt_space(0.0, 50.0),
                  PANTILT_RELATIVE_GENERIC_SPACE_URI) == 0);
    assert(strcmp(ptz_default_relative_pantilt_space(-1.0, 50.0),
                  PANTILT_RELATIVE_GENERIC_SPACE_URI) == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / "test.c"
            binary_path = Path(directory) / "test"
            source_path.write_text(source)
            subprocess.run(
                ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Isrc", source_path, "-o", binary_path],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([binary_path], check=True)

    def test_every_ptz_configuration_template_uses_the_selected_space(self):
        for relative_path in TEMPLATES:
            with self.subTest(template=relative_path):
                text = (ROOT / relative_path).read_text()
                self.assertEqual(text.count(PLACEHOLDER), 1)

    def test_every_template_render_supplies_the_selected_space(self):
        expected_calls = {
            "src/media_service.c": 5,
            "src/media2_service.c": 3,
            "src/ptz_service.c": 6,
        }
        for relative_path, count in expected_calls.items():
            with self.subTest(source=relative_path):
                text = (ROOT / relative_path).read_text()
                self.assertEqual(text.count(f'"{PLACEHOLDER}"'), count)
                self.assertEqual(text.count("ptz_default_relative_pantilt_space("), count)


if __name__ == "__main__":
    unittest.main()
