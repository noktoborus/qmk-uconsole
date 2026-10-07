#!/bin/sh
# Build the uConsole firmware inside the container.
#
# Usage: qmk-uconsole-build [keymap ...]
#   With no arguments builds "default".
#   Resulting clockworkpi_uconsole_<keymap>.bin files land in the repo root.
set -eu

REPO=${QMK_UCONSOLE_DIR:-/src}
KB=clockworkpi/uconsole

cd "$REPO"

if [ $# -gt 0 ]; then
	KEYMAPS="$*"
else
	KEYMAPS="default"
fi

# 1. qmk_firmware: use the pinned submodule if this is a git checkout,
#    otherwise fall back to a shallow clone (same as the Makefile does).
if [ ! -f qmk_firmware/Makefile ]; then
	echo ">> fetching qmk_firmware"
	if [ -e .git ]; then
		git submodule update --init --recursive --depth 1 qmk_firmware \
			|| git submodule update --init --recursive qmk_firmware
	else
		rm -rf qmk_firmware.tmp
		git clone --depth 1 https://github.com/qmk/qmk_firmware.git qmk_firmware.tmp
		git -C qmk_firmware.tmp submodule update --init --recursive --depth 1
		rm -rf qmk_firmware
		mv qmk_firmware.tmp qmk_firmware
	fi
fi

# 2. Expose the keyboard to QMK. A relative link works both inside the
#    container and on the host (an absolute host path would not).
link=qmk_firmware/keyboards/clockworkpi
if [ -L "$link" ] || [ ! -e "$link" ]; then
	ln -sfn ../../clockworkpi "$link"
else
	echo "error: $link exists and is not a symlink" >&2
	exit 1
fi

# 3. Compile.
cd qmk_firmware
for km in $KEYMAPS; do
	echo ">> qmk compile -kb $KB -km $km"
	qmk compile -j "$(nproc)" -kb "$KB" -km "$km"
	cp ".build/clockworkpi_uconsole_${km}.bin" "$REPO/"
	echo ">> $REPO/clockworkpi_uconsole_${km}.bin"
done
