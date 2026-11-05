#!/sbin/sh
#
# SPDX-License-Identifier: Apache-2.0
#
# Tells the M3 Note boards apart by the fields their bootloaders put on the
# kernel command line, which are the same on every unit of a model:
#
#   M681:  color_type=M91<colour>             sw_version=6815<build>
#   L681:  androidboot.cus_color=L91<colour>  androidboot.cus_param=0x6815<build>
#   M1621: color_type=M1621<colour>           sw_version=621<build>  (M5 Note)
#
# Usage: check_board.sh <board>...
# Exits 0 if this phone is one of the given boards (m681, l681, m1621), 1 otherwise.

color_type=
sw_version=
cus_color=
cus_param=

for word in $(cat /proc/cmdline); do
    case "$word" in
        color_type=*) color_type=${word#*=} ;;
        sw_version=*) sw_version=${word#*=} ;;
        androidboot.cus_color=*) cus_color=${word#*=} ;;
        androidboot.cus_param=*) cus_param=${word#*=} ;;
    esac
done

board=unknown
case "$color_type:$sw_version:$cus_color:$cus_param" in
    M91[A-Z]:6815*::) board=m681 ;;
    ::L91[A-Z]:0x6815*) board=l681 ;;
    M1621[A-Z]:621*::) board=m1621 ;;
esac

echo "check_board: detected $board"
for allowed in "$@"; do
    [ "$board" = "$allowed" ] && exit 0
done
exit 1
