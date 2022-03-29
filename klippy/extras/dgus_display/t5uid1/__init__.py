# Package definition for the extras/dgus_display/t5uid1 directory
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
from . import t5uid1

def init(config, uart):
    return t5uid1.T5UID1(config, uart)
