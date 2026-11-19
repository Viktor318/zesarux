#!/bin/bash
# make-next-sd.sh <distro folder> <image file> [size, default 1G]
#
# Makes an SD card image for ZEsarUX (ZX Spectrum Next / TBBlue) from the files
# of a System/Next distribution: the contents of sn-complete-*.zip,
# sn-nosrc-*.zip or a checkout/download of https://gitlab.com/thesmog358/tbblue
# (the folder that has TBBLUE.FW, TBBLUE.TBU, machines/, nextzxos/ ...).
#
# Keep the size at 2 GB or less: ZEsarUX does not emulate larger (SDHC) cards
# with image files.
#
# The image is like the ones ZEsarUX ships (tbblue.mmc): a partition table with
# one FAT32 partition from sector 2048, the distribution files on it.
#
# Needs (Ubuntu / WSL):  sudo apt install mtools dosfstools fdisk
#
# Example:
#   ./make-next-sd.sh ~/next/sn-complete-24.11 ~/next/next-24.11.img 1G
# then in ZEsarUX: Storage > SD/MMC > MMC file: next-24.11.img, MMC and DivMMC
# ports on (persistent writes off, to keep the image as it is), restart.

set -e

SRC="$1"
IMG="$2"
SIZE="${3:-1G}"
# absolute path: the files are copied from inside the distribution folder
IMG="$(realpath -m "$IMG")"

if [ -z "$SRC" ] || [ -z "$IMG" ]; then
    echo "Usage: $0 <distro folder> <image file> [size, default 1G]"
    exit 1
fi
if [ ! -f "$SRC/TBBLUE.FW" ] && [ ! -f "$SRC/tbblue.fw" ]; then
    echo "$SRC has no TBBLUE.FW: is it the root folder of the distribution?"
    exit 1
fi
for t in mcopy mkfs.fat sfdisk; do
    command -v $t >/dev/null || { echo "$t is missing: sudo apt install mtools dosfstools fdisk"; exit 1; }
done
if [ -e "$IMG" ]; then
    echo "$IMG already exists, not overwriting it"
    exit 1
fi

# an empty image, a partition table with one FAT32 (LBA) partition from sector 2048
truncate -s "$SIZE" "$IMG"
echo 'start=2048, type=c' | sfdisk -q "$IMG"
# the FAT32 file system on the partition (offset in sectors)
mkfs.fat -F 32 -n NEXT --offset 2048 "$IMG" >/dev/null
# the files of the distribution onto it (@@1M: the partition starts at 1 MB)
# (.git, if the folder is a git checkout, is not copied)
( cd "$SRC" && for f in * .[!.]*; do
    [ -e "$f" ] || continue
    [ "$f" = ".git" ] && continue
    mcopy -s -i "$IMG@@1M" "$f" ::/
  done )

echo "Done: $IMG ($SIZE)"
mdir -i "$IMG@@1M" ::/ | tail -n +4
