# Extended status information for DGUS displays
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

class DGUSStatus:
    def __init__(self, config):
        self.printer = config.get_printer()
        self.reactor = self.printer.get_reactor()
        self.gcode = self.printer.lookup_object("gcode")

        self.M73_registered = False
        self.M73_original = None

        self.ready = False
        self.start_at = None
        self.progress = None
        self.filename = None
        self.finish_at = None
        self.finish_at_naive = None
        self.pause_at = None

        self.idle_timeout = None
        self.pause_resume = None
        self.virtual_sdcard = None
        self.print_stats = None

        self.pause_timer = self.reactor.register_timer(self.check_pause)

        self.gcode.register_command("DGUS_SET_FILENAME",
                                    self.cmd_DGUS_SET_FILENAME,
                                    desc=self.cmd_DGUS_SET_FILENAME_help)

        self.printer.register_event_handler("klippy:ready", self.handle_ready)
        self.printer.register_event_handler("klippy:disconnect", self.reset)
        self.printer.register_event_handler("klippy:shutdown", self.reset)
        self.printer.register_event_handler("idle_timeout:printing",
                                            self.handle_printing)

    def handle_ready(self):
        self.toolhead = self.printer.lookup_object("toolhead")
        self.idle_timeout = self.printer.lookup_object("idle_timeout")
        self.pause_resume = self.printer.lookup_object("pause_resume", None)
        self.virtual_sdcard = self.printer.lookup_object("virtual_sdcard", None)
        self.print_stats = self.printer.lookup_object("print_stats", None)
        if not self.M73_registered:
            self.M73_original = self.gcode.register_command("M73", None)
            self.gcode.register_command("M73", self.cmd_M73)
            self.M73_registered = True
        self.ready = True

    def handle_printing(self, print_time):
        if self.start_at is None:
            self.start_at = self.reactor.monotonic()
            self.reactor.update_timer(self.pause_timer, self.start_at + 5.)

    def reset(self):
        if self.start_at is not None:
            self.start_at = None
            self.reactor.update_timer(self.pause_timer, self.reactor.NEVER)
        self.progress = None
        self.filename = None
        self.finish_at = None
        self.finish_at_naive = None
        self.pause_at = None

    def check_pause(self, eventtime):
        if self.pause_at is None:
            if self.is_paused(eventtime):
                self.pause_at = eventtime
        elif self.is_printing(eventtime):
            pause_duration = max(0., eventtime - self.pause_at)
            if self.start_at is not None:
                self.start_at += pause_duration
            if self.finish_at is not None:
                self.finish_at += pause_duration
            if self.finish_at_naive is not None:
                self.finish_at_naive[1] += pause_duration
            self.pause_at = None
        return eventtime + 5.

    def is_busy(self, eventtime):
        if not self.ready:
            return False
        return self.idle_timeout.state == "Printing"

    def is_at_temp(self, eventtime):
        if not self.ready:
            return False
        extruder = self.toolhead.get_extruder()
        try:
            heater = extruder.get_heater()
            return heater.get_temp(eventtime)[1] >= heater.min_extrude_temp
        except Exception:
            return False

    def is_printing_sd(self, eventtime):
        if not self.ready:
            return False
        if self.virtual_sdcard is not None and self.virtual_sdcard.is_active():
            return True
        return False

    def is_printing_serial(self, eventtime):
        if not self.ready:
            return False
        if self.is_busy(eventtime) and self.is_at_temp(eventtime):
            return True
        return False

    def is_printing(self, eventtime):
        if self.is_printing_sd(eventtime) or self.is_printing_serial(eventtime):
            return True
        return False

    def is_paused_sd(self, eventtime):
        if not self.ready:
            return False
        if self.print_stats is not None and self.print_stats.state == "paused":
            return True
        return False

    def is_paused_serial(self, eventtime):
        if not self.ready:
            return False
        if self.pause_resume is not None and self.pause_resume.is_paused:
            return True
        return False

    def is_paused(self, eventtime):
        if self.is_paused_sd(eventtime) or self.is_paused_serial(eventtime):
            return True
        return False

    def get_state(self, eventtime):
        state = "idle"
        sd = False
        if self.is_paused_sd(eventtime):
            state = "paused"
            sd = True
        elif self.is_paused_serial(eventtime):
            state = "paused"
        elif self.is_printing_sd(eventtime):
            state = "printing"
            sd = True
        elif self.is_printing_serial(eventtime):
            state = "printing"
        elif self.is_busy(eventtime):
            state = "busy"
        else:
            self.reset()
        return (state, sd)

    def get_progress(self, eventtime):
        if self.progress is not None:
            return self.progress
        if self.virtual_sdcard is not None and self.virtual_sdcard.is_active():
            return self.virtual_sdcard.progress()
        return None

    def get_filename(self, eventtime):
        if self.print_stats is not None and self.print_stats.filename:
            return self.print_stats.filename
        if self.filename is not None:
            return self.filename
        return None

    def get_printing_time(self, eventtime):
        if self.start_at is None:
            return None
        if self.pause_at is not None:
            printing_time = self.pause_at - self.start_at
        else:
            printing_time = eventtime - self.start_at
        return max(0, int(round(printing_time)))

    def get_remaining_time(self, eventtime):
        if self.finish_at is not None:
            if self.pause_at is not None:
                remaining_time = self.finish_at - self.pause_at
            else:
                remaining_time = self.finish_at - eventtime
        else:
            progress = self.get_progress(eventtime)
            if (self.finish_at_naive is None
                or self.finish_at_naive[0] != progress):
                printing_time = self.get_printing_time(eventtime)
                if (progress is None or progress < 0.05
                    or printing_time is None or printing_time < 300):
                    return None
                remaining = (1. - progress) / progress
                remaining_time = int(round(float(printing_time) * remaining))
                self.finish_at_naive = (progress, eventtime + remaining_time)
            remaining_time = self.finish_at_naive[1] - eventtime
        return max(0, int(round(remaining_time)))

    def get_status(self, eventtime):
        print_from = None
        progress = None
        filename = None
        printing_time = None
        remaining_time = None
        state, sd = self.get_state(eventtime)
        if state == "printing" or state == "paused":
            progress = self.get_progress(eventtime)
            if sd:
                print_from = "sd"
            else:
                print_from = "serial"
        if progress is not None:
            filename = self.get_filename(eventtime)
            printing_time = self.get_printing_time(eventtime)
            remaining_time = self.get_remaining_time(eventtime)
        return {
            "state": state,
            "print_from": print_from,
            "progress": progress,
            "filename": filename,
            "printing_time": printing_time,
            "remaining_time": remaining_time
        }

    def cmd_M73(self, gcmd):
        state = self.get_state(self.reactor.monotonic())[0]
        if state != "idle":
            progress = gcmd.get_float("P", None)
            remaining = gcmd.get_int("R", None)
            if progress is not None:
                if progress >= 0:
                    self.progress = min(1., max(0., (progress / 100.)))
                else:
                    self.progress = None
            if remaining is not None:
                if remaining >= 0:
                    self.finish_at = self.reactor.monotonic() + remaining * 60
                else:
                    self.finish_at = None
                self.finish_at_naive = None
        if self.M73_original is not None:
            self.M73_original(gcmd)

    cmd_DGUS_SET_FILENAME_help = "Set the name of the file being printed"
    def cmd_DGUS_SET_FILENAME(self, gcmd):
        state = self.get_state(self.reactor.monotonic())[0]
        if state == "idle":
            return
        name = gcmd.get("NAME", None)
        if name:
            self.filename = name
        else:
            self.filename = None

def load_config(config):
    return DGUSStatus(config)
