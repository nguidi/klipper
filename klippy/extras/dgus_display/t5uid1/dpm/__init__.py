# Package definition for the extras/dgus_display/t5uid1/dpm directory
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
from . import dpm

def init(config, t5uid1):
    return dpm.DGUSPrinterMenu(config, t5uid1)
