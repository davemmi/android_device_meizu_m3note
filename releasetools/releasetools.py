# SPDX-License-Identifier: Apache-2.0

"""Board check for the shared Meizu M3 Note package."""

import common

# Boards whose boot image this package carries: m5note_defconfig appends the
# M1621 (Meizu M5 Note) device tree only.
BOARDS = ("m1621",)


def FullOTA_Assertions(info):
  AssertBoard(info)


def IncrementalOTA_Assertions(info):
  AssertBoard(info)


def AssertBoard(info):
  boards = ", ".join('"%s"' % b for b in BOARDS)
  info.script.AppendExtra(
      'package_extract_file("install/bin/check_board.sh", "/tmp/check_board.sh");')
  info.script.AppendExtra(
      'set_metadata("/tmp/check_board.sh", "uid", 0, "gid", 0, "mode", 0755);')
  info.script.AppendExtra(
      'run_program("/sbin/sh", "/tmp/check_board.sh", %s) == 0 || '
      'run_program("/system/bin/sh", "/tmp/check_board.sh", %s) == 0 || '
      'abort("E%d: This package is for the Meizu M5 Note %s; '
      'this board is not supported.");'
      % (boards, boards, common.ErrorCode.DEVICE_MISMATCH,
         "/".join(b.upper() for b in BOARDS)))
