#!/usr/bin/env python3
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

"""Typed turns over the serial console: the board's side (muse_chat_text.c) against
the host's (tools/muse/chat.py), and how tools/muse/avatar.py reads Muse's reply."""

from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "muse"))

import avatar  # noqa: E402
import chat  # noqa: E402

CONSOLE_LINE = 400   # muse_chat_text.c
SERIAL_LINE = 1024   # muse_input.c: the longest line the board takes

TRICKY = (
    'Plain words, "quotes", back\\slashes \\n that aren\'t newlines,\n'
    "real newlines\r\n\ttabs, a bell \x07, DEL \x7f, café, 日本語, "
    "emoji \U0001F989\U0001F9E1, and a ``` fence.\n"
)


class HarnessTest(unittest.TestCase):
    binary: Path

    @classmethod
    def setUpClass(cls) -> None:
        cc = shlex.split(os.environ.get("CC", "cc"))
        if not cc or shutil.which(cc[0]) is None:
            raise unittest.SkipTest("C compiler not available")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.tmp.name) / "muse_serial_chat_harness"
        proc = subprocess.run(
            [
                *cc,
                "-include",
                str(ROOT / "tests" / "host_compat.h"),
                "-std=gnu11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "components" / "muse"),
                str(ROOT / "tests" / "muse_serial_chat_harness.c"),
                str(ROOT / "components" / "muse" / "muse_chat_text.c"),
                str(ROOT / "components" / "muse" / "muse_text.c"),
                "-o",
                str(cls.binary),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if proc.returncode:
            raise AssertionError(proc.stdout + proc.stderr)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def run_harness(self, mode: str, data: bytes, *args: str) -> bytes:
        proc = subprocess.run([str(self.binary), mode, *args], input=data, capture_output=True)
        self.assertEqual(proc.returncode, 0, msg=proc.stderr.decode())
        return proc.stdout

    def frames(self, text: str) -> list[dict]:
        out = self.run_harness("console", text.encode())
        self.assertTrue(out.endswith(b"\n"))
        frames = []
        for line in out.split(b"\n")[:-1]:
            self.assertLessEqual(len(line) + 1, CONSOLE_LINE, msg=line)
            self.assertTrue(line.startswith(b"@chat {"), msg=line)
            frames.append(json.loads(line[len("@chat "):]))
        return frames

    def test_reply_survives_the_console(self) -> None:
        for text in (TRICKY, TRICKY * 40, "x" * 239, "x" * 240, "\U0001F989" * 300, ""):
            with self.subTest(length=len(text)):
                frames = self.frames(text)
                self.assertEqual([f["seq"] for f in frames], list(range(frames[0]["seq"], frames[0]["seq"] + len(frames))))
                reply = chat.Reply()
                for f in frames:
                    reply.feed(f)
                self.assertEqual(reply.message(0), text)
                self.assertEqual(reply.message(1), "")
                self.assertTrue(reply.intact())
                self.assertEqual(frames[0], {"seq": frames[0]["seq"], "type": "sent", "bytes": 12})
                self.assertEqual(frames[1]["on"], True)
                self.assertEqual(frames[-2]["messages"], 2)
                self.assertEqual(frames[-1], {"seq": frames[-1]["seq"], "type": "error", "text": "TOO LONG"})

    def test_long_text_splits_between_characters(self) -> None:
        frames = [f for f in self.frames("é\U0001F989" * 500) if f["type"] == "text" and f["msg"] == 0]
        self.assertGreater(len(frames), 1)
        for f in frames:
            self.assertTrue(f["text"])
            self.assertNotIn("�", f["text"])

    def test_message_survives_the_trip_in(self) -> None:
        for text in (TRICKY, TRICKY * 40, "\\" * 1000, "\n" * 1000, "", "a\\"):
            with self.subTest(length=len(text)):
                lines = chat.split_message(text)
                data = text.replace("\0", "").encode()
                self.assertEqual(lines[-1][1], len(data))
                payloads = []
                for i, (line, total) in enumerate(lines):
                    self.assertLessEqual(len(line), SERIAL_LINE - 1)
                    prefix = b">chat=" if i == len(lines) - 1 else b">chat+="
                    self.assertTrue(line.startswith(prefix) and line.endswith(b"\n"), msg=line[:40])
                    payloads.append(line[len(prefix):])
                out = self.run_harness("unescape", b"".join(payloads))
                got, sizes = b"", []
                while out:
                    n, _, rest = out.partition(b":")
                    got += rest[: int(n)]
                    sizes.append(len(got))
                    out = rest[int(n):]
                self.assertEqual(got, data)
                self.assertEqual(sizes, [total for _, total in lines])

    def pages(self, text: str, cols: int, lines: int) -> list[list[str]]:
        out = self.run_harness("pages", text.encode(), str(cols), str(lines)).decode()
        return [page.split("\n") for page in out.split("\f")[:-1]]

    def test_chinese_wraps_between_characters(self) -> None:
        text = ("我看到桌上有一个白色的杯子，旁边放着一本打开的书（可能是小说）。"
                "Muse 会把拍到的照片发给你，也可以用AI帮你描述画面里的东西！") * 3
        for cols, rows in ((16, 2), (23, 11), (24, 3)):
            with self.subTest(cols=cols, lines=rows):
                pages = self.pages(text, cols, rows)
                said = pages[0] + [line for page in pages[1:] for line in page[1:]]
                for page in pages[:-1]:
                    self.assertEqual(len(page), rows)   # nothing cut for want of bytes
                for line in said:
                    self.assertLessEqual(len(line), cols, msg=line)
                    self.assertFalse(line[0] in "，。！？）、" or line[-1] in "（", msg=line)
                    self.assertNotRegex(line, r"^(use|se|e|I)|(M|Mu|Mus|A)$")   # words stay whole
                self.assertEqual("".join(said).replace(" ", ""), text.replace(" ", ""))
                if cols == 23:
                    self.assertGreater(len(pages[0]), 1)
                    self.assertGreater(len(pages[0][0]), cols - 2)   # Chinese fills its lines

    def test_markdown_stays_off_the_caption(self) -> None:
        text = ("## Summary\n- I took a **photo** of your desk.\n* See [the docs](https://example.com/a?b=1) "
                "for `more`.\n> Quoted __line__ with ~~old~~ text.\n1. Numbers stay: 3.5 and snake_case.\n"
                "### 小结\n- 白色的**杯子**，旁边有[说明](http://x.cn/y)。")
        said = [line for page in self.pages(text, 24, 3) for line in page]
        joined = "\n".join(said)
        for mark in ("#", "**", "`", "](", "[", "> ", "__", "~~", "- I", "* See", "- 白"):
            self.assertNotIn(mark, joined, msg=joined)
        for words in ("Summary", "I took a photo of your desk.", "the docs for more", "Quoted line with old",
                      "Numbers stay: 3.5 and snake_case.", "小结", "白色的杯子，旁边有说明。"):
            self.assertIn(words, joined.replace("\n", " ").replace("  ", " ") if words.isascii()
                          else joined.replace("\n", ""), msg=joined)


class ReplyTest(unittest.TestCase):
    def test_final_replaces_short_stream(self) -> None:
        r = chat.Reply()
        r.feed({"type": "text", "msg": 0, "text": "Hel"})
        r.feed({"type": "final", "msg": 0, "text": "Hello"})
        r.feed({"type": "message_done", "msg": 0, "bytes": 5})
        r.feed({"type": "text", "msg": 1, "text": "Bye"})
        r.feed({"type": "message_done", "msg": 1, "bytes": 3})
        self.assertEqual(r.messages, ["Hello", "Bye"])
        self.assertEqual(r.text, "Hello\n\nBye")
        self.assertTrue(r.intact())

    def test_short_message_isnt_intact(self) -> None:
        r = chat.Reply()
        r.feed({"type": "text", "msg": 0, "text": "Hel"})
        r.feed({"type": "message_done", "msg": 0, "bytes": 5})
        self.assertFalse(r.intact())


class ExtractTest(unittest.TestCase):
    code = (ROOT / "avatar" / "muse_pixel.c").read_text()

    def test_finds_the_file(self) -> None:
        for reply in (
            f"Here I am:\n\n```c\n{self.code}```\n",
            f"```c\n{self.code}",
            self.code,
            f"```\nint x;\n```\n\n```c\n{self.code}```\nThat's me.",
        ):
            self.assertEqual(avatar.extract_c(reply), self.code.strip() + "\n")

    def test_no_file(self) -> None:
        self.assertIsNone(avatar.extract_c("NO AVATAR: looked in my profile"))
        self.assertIsNone(avatar.extract_c("```c\nint main(void) { return 0; }\n```\n"))

    def test_description(self) -> None:
        copyright = self.code.split("\n", 1)[0]
        apache = "/*\n * Copyright (c) Meta Platforms, Inc. and affiliates.\n *\n * Licensed under the Apache License.\n */"
        for header in (copyright, apache):
            for body in ("/*\n * Pip: a round teal owl\n * with amber eyes.\n */\n", "// Pip: a round teal owl\n// with amber eyes.\n"):
                self.assertEqual(avatar.description(f"{header}\n\n{body}#include <math.h>\n"),
                                 "Pip: a round teal owl with amber eyes.")
            self.assertIsNone(avatar.description(f"{header}\n\n#include <math.h>\n"))


class PacingTest(unittest.TestCase):
    """The Watcher's CH342 drops bytes from whole packets: chat goes to it paced."""

    WATCHER = [SimpleNamespace(device=f"/dev/cu.usbmodem56D5057217{n}", vid=0x1A86, pid=0x55D2,
                               serial_number="56D5057217") for n in (1, 3)]
    STICKS3 = SimpleNamespace(device="/dev/cu.usbmodem1101", vid=0x303A, pid=0x1001, serial_number="AA:BB")

    class Ser:
        baudrate = 115200

        def __init__(self):
            self.writes = []

        def write(self, data):
            self.writes.append(bytes(data))
            return len(data)

        def flush(self):
            pass

    def board(self, paced):
        b = chat.Board.__new__(chat.Board)
        b.ser, b.paced = self.Ser(), paced
        return b

    def test_watcher_takes_commands_on_its_s3_port(self) -> None:
        found = chat.ports.command_ports(self.WATCHER + [self.STICKS3])
        self.assertEqual(found, [self.STICKS3.device, self.WATCHER[1].device])
        self.assertTrue(chat.ports.paced(self.WATCHER[1].device, self.WATCHER))
        self.assertFalse(chat.ports.paced(self.STICKS3.device, [self.STICKS3]))

    def test_paced_writes_are_small_and_whole(self) -> None:
        line = chat.split_message("x" * 2000)[0][0]
        b = self.board(True)
        b.write(line)
        self.assertEqual(b"".join(b.ser.writes), line)
        self.assertLessEqual(max(map(len, b.ser.writes)), 64)

    def test_unpaced_writes_go_at_once(self) -> None:
        b = self.board(False)
        b.write_line("status")
        b.wake()
        self.assertEqual(b.ser.writes, [b">status\n"])


if __name__ == "__main__":
    unittest.main()
