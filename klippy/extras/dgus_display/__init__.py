# Package definition for the extras/dgus_display directory
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
from . import dgus_display

def load_config(config):
    return dgus_display.load_config(config)

def load_config_prefix(config):
    return dgus_display.load_config(config)
