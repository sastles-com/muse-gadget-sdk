# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LinkAtomS3RDisplayTextTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        out = Path(cls.temp.name)
        source = (ROOT / "main/display_text.c").read_text()
        start = source.index("// ---- Layout (host-tested)")
        (out / "atoms3r_display_text_layout.inc").write_text(source[start:source.index("// ---- Rendering", start)])
        cc = shlex.split(os.environ.get("CC", "cc"))
        flags = ["-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"), "-I", str(out)]
        commands = [
            [*cc, "-std=c11", *flags, str(ROOT / "tests/link_atoms3r_display_text_harness.c"), "-o", str(out / "text")],
        ]
        for cmd in commands:
            compiled = subprocess.run(cmd, capture_output=True, text=True)
            if compiled.returncode:
                raise AssertionError(compiled.stdout + compiled.stderr)
        cls.out = out

    def test_text_to_layout(self):
        result = subprocess.run([str(self.out / "text")], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_command_is_advertised_and_dispatched_with_the_feature(self):
        noise = (ROOT / "main/noise_control.cpp").read_text()
        start = noise.index("#if CONFIG_HOMEHUB_ATOMS3R_DISPLAY_TEXT")
        self.assertIn('add_command(commands, "display.text"', noise[start:noise.index("#endif", start)])
        app = (ROOT / "main/app.c").read_text()
        start = app.index("static cJSON *on_ws_command(")
        dispatch = app[start:app.index("unsupported command", start)]
        block = dispatch[dispatch.index("#if CONFIG_HOMEHUB_ATOMS3R_DISPLAY_TEXT"):]
        self.assertIn('"display.text"', block[:block.index("#endif")])
        self.assertIn("display_text_command(params)", block[:block.index("#endif")])


if __name__ == "__main__":
    unittest.main()
