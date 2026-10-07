#!/usr/bin/env -S make -f

SUDO = sudo
USB_ID = 1eaf:0003

.PHONY: default
# Build the firmware in the podman container (see compose.yaml)
default:
	podman-compose run --rm build

# Build with a QMK toolchain installed on the host
.PHONY: local
local: clockworkpi_uconsole_default.bin

# Keymap picture for the README, drawn by keymap-drawer in the container
KEYMAP_DIR = clockworkpi/uconsole
.PHONY: layout
layout:
	podman-compose run --rm build make images/layout.svg

images/layout.svg: $(KEYMAP_DIR)/keymaps/default/keymap.json $(KEYMAP_DIR)/keyboard.json $(KEYMAP_DIR)/keymap-drawer.yaml
	keymap -c $(KEYMAP_DIR)/keymap-drawer.yaml parse -q $(KEYMAP_DIR)/keymaps/default/keymap.json -l Base Fn Gamepad Free \
	  | keymap -c $(KEYMAP_DIR)/keymap-drawer.yaml draw -j $(KEYMAP_DIR)/keyboard.json - -s Base Fn Gamepad > $@

.PHONY: reflash
reflash: default
	# dfu-util exits nonzero even without errors
	sh -c '$(SUDO) dfu-util -w -d $(USB_ID) -a 2 -D clockworkpi_uconsole_default.bin -R || true'

clockworkpi_uconsole_default.bin: qmk_firmware/.build/clockworkpi_uconsole_default.bin
	mv qmk_firmware/.build/clockworkpi_uconsole_default.bin $@

qmk_firmware/.build/clockworkpi_uconsole_default.bin: clockworkpi/uconsole/keymaps/default/keymap.json qmk_firmware
	sh -c 'cd qmk_firmware; qmk compile -kb clockworkpi/uconsole -km default'


qmk_firmware:
	rm -rf $@.tmp
	git clone --depth 1 https://github.com/qmk/qmk_firmware.git $@.tmp
	git -C qmk_firmware.tmp submodule sync --recursive
	git -C qmk_firmware.tmp submodule update --init --recursive
	mv $@.tmp $@
	ln -s $(PWD)/clockworkpi $(PWD)/qmk_firmware/keyboards/
