CUSTOM_MATRIX = lite
SRC += matrix.c
VIA_ENABLE = yes
BACKLIGHT_DRIVER = pwm
POINTING_DEVICE_DRIVER = custom
SRC += timeout.c rate_meter.c glider.c trackball.c backlight_fx.c lock.c gamepad.c
