# T5UID1 pages
#
# Copyright (C) 2021  Desuuuu <contact@desuuuu.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

class Page(object):
    def __init__(self, config, display, touch):
        name_parts = config.get_name().split()
        if len(name_parts) != 2:
            raise config.error("Section name '%s' is not valid"
                               % config.get_name())
        self.name = name_parts[1]

        self.id = config.getint("id", minval=0, maxval=0xff)
        cdisplay = list(config.getlist("display", []))
        ctouch = list(config.getlist("touch", []))
        self.display = {}
        for d in display:
            if d.page is None:
                if d.name not in cdisplay:
                    continue
                cdisplay.remove(d.name)
            elif d.page != self.name:
                continue
            if d.name in self.display:
                raise config.error(
                    "Display control '%s' is already defined in page '%s'"
                    % (d.name, self.name))
            self.display[d.name] = d
        if len(cdisplay) > 0:
            raise config.error(
                "Option 'display' in section '%s' is not valid"
                % config.get_name())
        self.touch = {}
        for t in touch:
            if t.page is None:
                if t.name not in ctouch:
                    continue
                ctouch.remove(t.name)
            elif t.page != self.name:
                continue
            if t.name in self.touch:
                raise config.error(
                    "Touch control '%s' is already defined in page '%s'"
                    % (t.name, self.name))
            self.touch[t.name] = t
        if len(ctouch) > 0:
            raise config.error(
                "Option 'touch' in section '%s' is not valid"
                % config.get_name())

class PageGlobal(Page):
    def __init__(self, config, display, touch):
        self.name = "global"
        self.id = None
        self.display = {}
        for d in display:
            if d.page is not None:
                continue
            if d.name in self.display:
                raise config.error(
                    "Display control '%s' is already defined in page '%s'"
                    % (d.name, self.name))
            self.display[d.name] = d
        self.touch = {}
        for t in touch:
            if t.page is not None:
                continue
            if t.name in self.touch:
                raise config.error(
                    "Touch control '%s' is already defined in page '%s'"
                    % (t.name, self.name))
            self.touch[t.name] = t

def parse_pages(config, display, touch):
    pages = {}
    page_global = PageGlobal(config, display, touch)
    pages[page_global.name] = page_global
    for c in config.get_prefix_sections("dgus_page "):
        page = Page(c, display, touch)
        if page.name in pages:
            raise config.error("Page '%s' is already defined"
                               % page.name)
        pages[page.name] = page
    return pages
