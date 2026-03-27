
"""
HC-SR04 Distance Monitor — STM32MP157C-DK2
GTK fullscreen display running under Weston.
"""

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GLib
import threading
import time

RPMSG_DEV = "/dev/ttyRPMSG0"
CLOSE_CM  = 20
MID_CM    = 60


latest_distance = None
lock = threading.Lock()

def reader_thread():
    global latest_distance
    while True:
        try:
            with open(RPMSG_DEV, "r") as f:
                for line in f:
                    line = line.strip()
                    if line.startswith("DIST:"):
                        try:
                            val = float(line.split(":")[1])
                            with lock:
                                latest_distance = val
                        except ValueError:
                            pass
        except Exception:
            time.sleep(1)

class DistanceWindow(Gtk.Window):
    def __init__(self):
        super().__init__()
        self.set_title("Distance Monitor")
        self.fullscreen()
        self.set_keep_above(True)

        #Dark background
        self.override_background_color(
            Gtk.StateFlags.NORMAL,
            Gdk.RGBA(0.04, 0.04, 0.08, 1.0)
        )

        #Main vertical box
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        vbox.set_homogeneous(False)
        self.add(vbox)

        #Top label "DISTANCE"
        self.label_title = Gtk.Label(label="DISTANCE")
        self.label_title.set_markup(
            '<span font="16" color="#3C3C50" weight="bold">DISTANCE</span>'
        )
        vbox.pack_start(self.label_title, False, False, 20)

        self.label_dist = Gtk.Label(label="--")
        self.label_dist.set_markup(
            '<span font="96" weight="bold" color="#3C3C50">--</span>'
        )
        vbox.pack_start(self.label_dist, True, True, 0)

        #Unit label
        self.label_unit = Gtk.Label(label="cm")
        self.label_unit.set_markup(
            '<span font="28" color="#C8C8C8">cm</span>'
        )
        vbox.pack_start(self.label_unit, False, False, 0)

        #Progress bar
        self.progress = Gtk.ProgressBar()
        self.progress.set_fraction(0.0)
        self.progress.set_margin_start(20)
        self.progress.set_margin_end(20)
        self.progress.set_margin_bottom(20)
        self.progress.set_margin_top(10)
        vbox.pack_end(self.progress, False, False, 0)

       
        self.css_provider = Gtk.CssProvider()
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            self.css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
        self._set_bar_color("#32DC64")  #start green

        self.connect("destroy", Gtk.main_quit)

        #update per 100ms
        GLib.timeout_add(100, self.update_display)

    def _set_bar_color(self, color):
        css = f"""
        progressbar trough {{
            background-color: #1E1E2E;
            border-radius: 4px;
            min-height: 14px;
        }}
        progressbar progress {{
            background-color: {color};
            border-radius: 4px;
            min-height: 14px;
        }}
        window {{
            background-color: #0A0A14;
        }}
        """
        self.css_provider.load_from_data(css.encode())

    def update_display(self):
        with lock:
            dist = latest_distance

        if dist is None:
            self.label_dist.set_markup(
                '<span font="60" weight="bold" color="#3C3C50">--</span>'
            )
            self.label_title.set_markup(
                '<span font="16" color="#3C3C50" weight="bold">WAITING FOR SENSOR</span>'
            )
            self.progress.set_fraction(0.0)
        else:
            #distance color relater
            if dist < CLOSE_CM:
                color_hex = "#FF3C3C"
                bar_hex   = "#FF3C3C"
            elif dist < MID_CM:
                color_hex = "#FFD232"
                bar_hex   = "#FFD232"
            else:
                color_hex = "#32DC64"
                bar_hex   = "#32DC64"

            self.label_dist.set_markup(
                f'<span font="96" weight="bold" color="{color_hex}">{dist:.1f}</span>'
            )
            self.label_title.set_markup(
                '<span font="16" color="#3C3C50" weight="bold">DISTANCE</span>'
            )
            self.label_unit.set_markup(
                f'<span font="28" color="#C8C8C8">cm</span>'
            )
            self._set_bar_color(bar_hex)
            self.progress.set_fraction(min(dist / 200.0, 1.0))

        return True  #keep timeout running

def main():
    #Start reafing
    t = threading.Thread(target=reader_thread, daemon=True)
    t.start()

    win = DistanceWindow()
    win.show_all()
    Gtk.main()

if __name__ == "__main__":
    main()
