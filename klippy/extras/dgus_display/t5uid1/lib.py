# T5UID1 library
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
import struct

TYPES = {
    "int8": "b",
    "uint8": "B",
    "int16": "h",
    "uint16": "H",
    "int32": "i",
    "uint32": "I",
    "int64": "q"
}

class error(Exception):
    pass

def pack(*args):
    args_len = len(args)
    if args_len % 2 != 0:
        raise error("Invalid arguments")
    it = range(0, args_len, 2)
    format = "".join([TYPES[args[i]] for i in it])
    values = [args[i+1] for i in it]
    try:
        return bytearray(struct.pack(">" + format, *values))
    except:
        raise error("Invalid arguments")

def unpack(buffer, *args, **kwargs):
    args_len = len(args)
    if args_len < 1:
        raise error("Invalid arguments")
    size = len(buffer)
    format = ""
    for arg in args:
        format += TYPES[arg]
        size -= struct.calcsize(TYPES[arg])
    if size > 0 and not kwargs.get("strict", False):
        format += TYPES["uint8"] * size
    try:
        return struct.unpack(">" + format, buffer)[:args_len]
    except:
        raise error("Invalid arguments")

def build_command(command, data):
    message = pack("uint16", 0x5aa5,
                   "uint8", len(data) + 1,
                   "uint8", command)
    message.extend(data)
    return message

def read(address, wlen=None):
    if type(address) is tuple:
        wlen = address[1]
        address = address[0]
    if address < 0 or address > 0xffff:
        raise error("Invalid address")
    if wlen < 1 or wlen > 0x7d:
        raise error("Invalid wlen")
    cdata = pack("uint16", address,
                 "uint8", wlen)
    return build_command(0x83, cdata)

def write(address, data=None):
    if type(address) is tuple:
        data = address[1]
        address = address[0]
    if address < 0 or address > 0xffff:
        raise error("Invalid address")
    if len(data) < 1 or len(data) > 248 or len(data) % 2 != 0:
        raise error("Invalid data length")
    cdata = pack("uint16", address)
    cdata.extend(data)
    return build_command(0x82, cdata)

def get_versions(build=False):
    if build:
        return read(0x0f, 1)
    return (0x0f, 1)

def get_page(build=False):
    if build:
        return read(0x85, 1)
    return (0x85, 1)

def set_page(page, build=False):
    if page < 0 or page > 0xff:
        raise error("Invalid page")
    cdata = pack("uint8", 0x5a,
                 "uint8", 1,
                 "uint16", page)
    if build:
        return write(0x84, cdata)
    return (0x84, cdata)

def play_sound(start, slen=1, volume=-1, build=False):
    if start < 0 or start > 0xff:
        raise error("Invalid start")
    if slen < 0 or slen > 0xff:
        raise error("Invalid slen")
    if volume > 0xff:
        raise error("Invalid volume")
    if volume < 0:
        cdata = pack("uint8", start,
                     "uint8", slen)
    else:
        cdata = pack("uint8", start,
                     "uint8", slen,
                     "uint8", volume,
                     "uint8", 0)
    if build:
        return write(0xa0, cdata)
    return (0xa0, cdata)

def stop_sound(build=False):
    return play_sound(0, 0, build=build)

def get_volume(build=False):
    if build:
        return read(0xa1, 1)
    return (0xa1, 1)

def set_volume(volume, build=False):
    if volume < 0 or volume > 0xff:
        raise error("Invalid volume")
    cdata = pack("uint8", volume,
                 "uint8", 0)
    if build:
        return write(0xa1, cdata)
    return (0xa1, cdata)

def get_brightness(build=False):
    if build:
        return read(0x82, 1)
    return (0x82, 1)

def set_brightness(brightness, build=False):
    if brightness < 0 or brightness > 0x64:
        raise error("Invalid brightness")
    cdata = pack("uint8", brightness,
                 "uint8", brightness)
    if build:
        return write(0x82, cdata)
    return (0x82, cdata)

def enable_control(page, control_type, index, build=False):
    if page < 0 or page > 0xff:
        raise error("Invalid page")
    if control_type < 0 or control_type > 0x7f:
        raise error("Invalid control type")
    if index < 0 or index > 0xff:
        raise error("Invalid index")
    cdata = pack("uint16", 0x5aa5,
                 "uint16", page,
                 "uint8", index,
                 "uint8", control_type,
                 "uint16", 1)
    if build:
        return write(0xb0, cdata)
    return (0xb0, cdata)

def disable_control(page, control_type, index, build=False):
    if page < 0 or page > 0xff:
        raise error("Invalid page")
    if control_type < 0 or control_type > 0x7f:
        raise error("Invalid control type")
    if index < 0 or index > 0xff:
        raise error("Invalid index")
    cdata = pack("uint16", 0x5aa5,
                 "uint16", page,
                 "uint8", index,
                 "uint8", control_type,
                 "uint16", 0)
    if build:
        return write(0xb0, cdata)
    return (0xb0, cdata)

def read_control(page, control_type, index, build=False):
    if page < 0 or page > 0xff:
        raise error("Invalid page")
    if control_type < 0 or control_type > 0x7f:
        raise error("Invalid control type")
    if index < 0 or index > 0xff:
        raise error("Invalid index")
    cdata = pack("uint16", 0x5aa5,
                 "uint16", page,
                 "uint8", index,
                 "uint8", control_type,
                 "uint16", 2)
    if build:
        return write(0xb0, cdata)
    return (0xb0, cdata)

def write_control(page, control_type, index, content=None, build=False):
    if page < 0 or page > 0xff:
        raise error("Invalid page")
    if control_type < 0 or control_type > 0x7f:
        raise error("Invalid control type")
    if index < 0 or index > 0xff:
        raise error("Invalid index")
    if content is not None and (len(content) > 64 or len(content) % 2 != 0):
        raise error("Invalid content length")
    cdata = pack("uint16", 0x5aa5,
                 "uint16", page,
                 "uint8", index,
                 "uint8", control_type,
                 "uint16", 3)
    if content is not None:
        cdata.extend(content)
    if build:
        return write(0xb0, cdata)
    return (0xb0, cdata)

def read_nor(nor_address, address, wlen, build=False):
    if nor_address < 0 or nor_address > 0x27fff or nor_address % 2 != 0:
        raise error("Invalid NOR address")
    if address < 0 or address > 0xffff or address % 2 != 0:
        raise error("Invalid address")
    if wlen < 1 or wlen > 0xffff or wlen % 2 != 0:
        raise error("Invalid wlen")
    cdata = pack("uint32", nor_address,
                 "uint16", address,
                 "uint16", wlen)
    cdata[0] = 0x5a
    if build:
        return write(0x08, cdata)
    return (0x08, cdata)
