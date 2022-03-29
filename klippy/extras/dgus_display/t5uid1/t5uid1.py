# T5UID1 implementation
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
import logging, threading
from . import lib, debug, dpm

DGUS_IMPLEMENTATION = {
    "debug": debug.init,
    "dgus_printer_menu": dpm.init
}

RW_TIMEOUT = 3.
COMMAND_TIMEOUT = 5.

class T5UID1Error(Exception):
    pass

class T5UID1:
    error = T5UID1Error

    def __init__(self, config, uart):
        self.printer = config.get_printer()
        self.reactor = self.printer.get_reactor()
        self.config_name = config.get_name()

        if uart.rx_buffer < 48:
            raise config.error(
                "Option 'uart_rx_buffer' in section '%s' is not valid"
                % self.config_name)
        if uart.tx_buffer < 48:
            raise config.error(
                "Option 'uart_tx_buffer' in section '%s' is not valid"
                % self.config_name)
        if uart.rx_interval <= 0 or uart.rx_interval > 100:
            raise config.error(
                "Option 'uart_rx_interval' in section '%s' is not valid"
                % self.config_name)

        self._uart = uart

        self._buffer = bytearray()
        self._lock = threading.Lock()
        self._completion = (None, None)

        self._volume = config.getint("volume", 75, minval=0, maxval=100)
        self._brightness = config.getint("brightness", 100, minval=0,
                                         maxval=100)

        self.printer.register_event_handler("klippy:ready", self._handle_ready)

        self.impl = config.getchoice("implementation", DGUS_IMPLEMENTATION,
                                default="debug")(config, self)

    def get_status(self, eventtime):
        return self.impl.get_status(eventtime)

    def setup_shutdown_msg(self, data):
        if len(data) < 1:
            return
        try:
            self._uart.setup_shutdown_msg(data)
        except ValueError as e:
            raise self.error(str(e))

    def map_range(self, value, imin, imax, omin, omax):
        result = (value - imin) * (omax - omin) / (imax - imin) + omin
        if type(value) is int:
            result = int(round(result))
        return max(omin, min(result, omax))

    def _handle_ready(self):
        self.set_volume(self._volume, force=True, wait=False)
        self.set_brightness(self._brightness, force=True, wait=False)

    def send(self, data, minclock=0, reqclock=0):
        try:
            self._uart.uart_send(data, minclock=minclock, reqclock=reqclock)
        except ValueError as e:
            raise self.error(str(e))

    def _find_response(self):
        buf_len = len(self._buffer)
        if buf_len < 3:
            return False, 0
        if self._buffer[0] != 0x5a or self._buffer[1] != 0xa5:
            return False, 1
        cmd_len = self._buffer[2]
        if cmd_len < 1:
            return False, 3
        response_len = cmd_len + 3
        if buf_len < response_len:
            return False, 0
        return True, response_len

    def _process_command(self, command, data):
        if command == 0x82:
            if len(data) != 2:
                return
            if data[0] == 0x4f and data[1] == 0x4b:
                with self._lock:
                    caddr, completion = self._completion
                    if caddr is None and completion is not None:
                        self._completion = (None, None)
                        self.reactor.async_complete(completion, True)
        elif command == 0x83:
            dlen = len(data)
            if dlen < 3:
                self._buffer = bytearray()
                logging.warn("T5UID1: Invalid message")
                return
            try:
                addr, mlen = lib.unpack(data[:3], "uint16", "uint8")
            except lib.error:
                self._buffer = bytearray()
                logging.warn("T5UID1: Invalid message")
                return
            mlen *= 2
            if dlen < mlen + 3:
                self._buffer = bytearray()
                logging.warn("T5UID1: Invalid message")
                return
            with self._lock:
                caddr, completion = self._completion
                if caddr == addr and completion is not None:
                    self._completion = (None, None)
                    self.reactor.async_complete(completion, data[3:mlen + 3])
            self.reactor.register_async_callback(
                (lambda e, s=self, a=addr, d=data[3:mlen + 3]:
                 s.impl.receive(a, d)))
        else:
            logging.info("T5UID1: Unknown command: 0x%02x" % command)

    def process(self, data):
        if len(data) < 1:
            self._buffer = bytearray()
            logging.warn("T5UID1: Serial RX overflow")
            return
        self._buffer.extend(data)
        while True:
            has_response, pop_count = self._find_response()
            if has_response:
                self._process_command(self._buffer[3],
                                      self._buffer[4:pop_count])
            if pop_count > 0:
                self._buffer = self._buffer[pop_count:]
            else:
                break

    def read(self, address, wlen=None):
        if type(address) is tuple:
            wlen = address[1]
            address = address[0]
        cdata = lib.read(address, wlen)
        dlen = wlen * 2
        while True:
            if self.printer.is_shutdown():
                raise self.error("Printer is shutdown")
            with self._lock:
                completion = self._completion[1]
                if completion is None:
                    completion = self.reactor.completion()
                    self._completion = (address, completion)
                    break
            completion.wait()
        systime = self.reactor.monotonic()
        print_time = self._uart.mcu.estimated_print_time(systime) + 0.100
        reqclock = self._uart.mcu.print_time_to_clock(print_time)
        self.send(cdata, reqclock=reqclock)
        result = completion.wait(systime + RW_TIMEOUT)
        completion.complete(None)
        with self._lock:
            if self._completion[1] == completion:
                self._completion = (None, None)
        if type(result) is not bytearray:
            raise self.error("Timeout waiting for response")
        if len(result) != dlen:
            raise self.error("Invalid response")
        return result

    def write(self, address, data=None, wait=True):
        if type(address) is tuple:
            data = address[1]
            address = address[0]
        cdata = lib.write(address, data)
        if not wait:
            return self.send(cdata)
        while True:
            if self.printer.is_shutdown():
                raise self.error("Printer is shutdown")
            with self._lock:
                completion = self._completion[1]
                if completion is None:
                    completion = self.reactor.completion()
                    self._completion = (None, completion)
                    break
            completion.wait()
        systime = self.reactor.monotonic()
        print_time = self._uart.mcu.estimated_print_time(systime) + 0.100
        reqclock = self._uart.mcu.print_time_to_clock(print_time)
        self.send(cdata, reqclock=reqclock)
        result = completion.wait(systime + RW_TIMEOUT)
        completion.complete(None)
        with self._lock:
            if self._completion[1] == completion:
                self._completion = (None, None)
        if result != True:
            raise self.error("Timeout waiting for write acknowledgement")

    def get_versions(self):
        return lib.unpack(self.read(lib.get_versions()), "uint8", "uint8")

    def get_page(self):
        _, page = lib.unpack(self.read(lib.get_page()), "uint8", "uint8")
        return page

    def set_page(self, page, wait=True):
        address, cdata = lib.set_page(page)
        self.write(address, cdata, wait=wait)
        if not wait:
            return
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint8")
            if flag != 0x5a:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def play_sound(self, start, slen=1, volume=-1, wait=True):
        if volume < 0:
            volume = self._volume
        volume = self.map_range(volume, 0, 100, 0, 255)
        address, cdata = lib.play_sound(start, slen, volume)
        self.write(address, cdata, wait=wait)

    def stop_sound(self):
        self.play_sound(0, 0)

    def get_volume(self, bypass=False):
        if not bypass:
            return self._volume
        volume, = lib.unpack(self.read(lib.get_volume()), "uint8")
        return self.map_range(volume, 0, 255, 0, 100)

    def set_volume(self, volume, save=False, force=False, wait=True):
        if not force and volume == self._volume:
            return
        address, cdata = lib.set_volume(self.map_range(volume, 0, 100, 0, 255))
        self.write(address, cdata, wait=wait)
        if save:
            self._volume = volume
            configfile = self.printer.lookup_object("configfile")
            configfile.set(self.config_name, "volume", volume)

    def get_brightness(self, bypass=False):
        if not bypass:
            return self._brightness
        brightness, = lib.unpack(self.read(lib.get_brightness()), "uint8")
        return brightness

    def set_brightness(self, brightness, save=False, force=False, wait=True):
        if not force and brightness == self._brightness:
            return
        address, cdata = lib.set_brightness(brightness)
        self.write(address, cdata, wait=wait)
        if save:
            self._brightness = brightness
            configfile = self.printer.lookup_object("configfile")
            configfile.set(self.config_name, "brightness", brightness)

    def enable_control(self, page, control_type, index):
        address, cdata = lib.enable_control(page, control_type, index)
        self.write(address, cdata)
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint16")
            if flag != 0x5aa5:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def disable_control(self, page, control_type, index):
        address, cdata = lib.disable_control(page, control_type, index)
        self.write(address, cdata)
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint16")
            if flag != 0x5aa5:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def read_control(self, page, control_type, index):
        address, cdata = lib.read_control(page, control_type, index)
        self.write(address, cdata)
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint16")
            if flag != 0x5aa5:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def write_control(self, page, control_type, index, content=None):
        address, cdata = lib.write_control(page, control_type, index, content)
        self.write(address, cdata)
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint16")
            if flag != 0x5aa5:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def read_nor(self, nor_address, address, wlen):
        address, cdata = lib.read_nor(nor_address, address, wlen)
        self.write(address, cdata)
        systime = self.reactor.monotonic()
        timeout = systime + COMMAND_TIMEOUT
        while not self.printer.is_shutdown():
            flag, = lib.unpack(self.read(address, 1), "uint8")
            if flag != 0x5a:
                return
            if systime > timeout:
                raise self.error("Timeout waiting for acknowledgement")
            systime = self.reactor.pause(systime + 0.050)

    def register_base_commands(self, display):
        cmds = ["DGUS_PLAY_SOUND", "DGUS_STOP_SOUND", "DGUS_GET_VOLUME",
                "DGUS_SET_VOLUME", "DGUS_GET_BRIGHTNESS", "DGUS_SET_BRIGHTNESS"]
        gcode = self.printer.lookup_object("gcode")
        for cmd in cmds:
            gcode.register_mux_command(
                cmd, "DISPLAY", display, getattr(self, "cmd_" + cmd),
                desc=getattr(self, "cmd_" + cmd + "_help", None))

    cmd_DGUS_PLAY_SOUND_help = "Play a sound"
    def cmd_DGUS_PLAY_SOUND(self, gcmd):
        start = gcmd.get_int("START", minval=0, maxval=255)
        slen = gcmd.get_int("LEN", default=1, minval=0, maxval=255)
        volume = gcmd.get_int("VOLUME", default=-1, minval=0, maxval=100)
        try:
            self.play_sound(start, slen, volume)
        except self.error as e:
            raise gcmd.error(str(e))

    cmd_DGUS_STOP_SOUND_help = "Stop any currently playing sound"
    def cmd_DGUS_STOP_SOUND(self, gcmd):
        try:
            self.stop_sound()
        except self.error as e:
            raise gcmd.error(str(e))

    cmd_DGUS_GET_VOLUME_help = "Get the volume"
    def cmd_DGUS_GET_VOLUME(self, gcmd):
        try:
            volume = self.get_volume()
        except self.error as e:
            raise gcmd.error(str(e))
        gcmd.respond_info("Volume: %d%%" % volume)

    cmd_DGUS_SET_VOLUME_help = "Set the volume"
    def cmd_DGUS_SET_VOLUME(self, gcmd):
        volume = gcmd.get_int("VOLUME", minval=0, maxval=100)
        save = gcmd.get_int("SAVE", 0)
        try:
            self.set_volume(volume, save=save)
        except self.error as e:
            raise gcmd.error(str(e))

    cmd_DGUS_GET_BRIGHTNESS_help = "Get the brightness"
    def cmd_DGUS_GET_BRIGHTNESS(self, gcmd):
        try:
            brightness = self.get_brightness()
        except self.error as e:
            raise gcmd.error(str(e))
        gcmd.respond_info("Brightness: %d%%" % brightness)

    cmd_DGUS_SET_BRIGHTNESS_help = "Set the brightness"
    def cmd_DGUS_SET_BRIGHTNESS(self, gcmd):
        brightness = gcmd.get_int("BRIGHTNESS", minval=0, maxval=100)
        save = gcmd.get_int("SAVE", 0)
        try:
            self.set_brightness(brightness, save=save)
        except self.error as e:
            raise gcmd.error(str(e))
