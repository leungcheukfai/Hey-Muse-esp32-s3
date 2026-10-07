# Copyright (c) Meta Platforms, Inc. and affiliates.
# SPDX-License-Identifier: Apache-2.0

"""Exercise English UI strings and reply-language prompts on the host."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MuseLanguageTest(unittest.TestCase):
    def test_ui_strings_remain_english(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "muse_lang_test"
            subprocess.run(
                shlex.split(os.environ.get("CC", "cc"))
                + [
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "components/muse"),
                    str(ROOT / "tests/muse_lang_harness.c"),
                    str(ROOT / "components/muse/muse_lang.c"),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
