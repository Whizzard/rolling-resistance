#!/usr/bin/env python3
"""Fake test stand for UI work: serves firmware/web/index.html and simulates the firmware.

No ESP32 needed. Endpoints as on the stand: /api/state, /api/cmd, /api/run, /api/profiles, /api/files, /fs.
The guided measurement run (14 steps) runs on a shortened clock, so a whole run takes
a few minutes. Physics is a rough stand-in -- good enough to see every screen, not for
judging numbers.

    python3 tools/fake_stand.py                      # http://localhost:8080/
    python3 tools/fake_stand.py --sd unreadable      # SD state: ready|absent|readonly|unreadable
    python3 tools/fake_stand.py --fast 4             # run clock 4x faster still

The page is re-read on every request: edit index.html, reload the browser.
"""
import argparse
import json
import math
import pathlib
import random
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGE = ROOT / "firmware" / "web" / "index.html"
STATEJSON = ROOT / "firmware" / "src" / "statejson.cpp"

SD_STATES = {"absent": 0, "ready": 1, "readonly": 2, "unreadable": 3}
PLATEAUS = [15.0, 28.8, 40.0, 50.0, 60.0]
LOAD_N = 494.3
DRUM_U = 2.842
R_DRUM = DRUM_U / (2 * math.pi)
ARM = 0.084
LOCK = threading.Lock()


def crr_true(v):
    return 0.0045 * (1 + 0.004 * (v - 28.8))


def m_idle(v):
    return 0.05 + 0.0008 * v + 0.000004 * v * v


class Stand:
    def __init__(self, sd, fast):
        self.fast = fast
        self.t0 = time.time()
        self.wall = 0
        self.target = 0.0
        self.kmh = 0.0
        self.loaded = False          # wheel on the drum
        self.sd = SD_STATES[sd]
        self.sd_fmt_until = 0
        self.sd_fmt_res = ""
        self.sd_fmt_n = 0
        self.fl = 1
        self.rec = False
        self.rec_man = False
        self.rec_kb = 0.0
        self.note = ""
        self.err = ""
        self.set = {"drum_u": DRUM_U, "arm": ARM, "calib_arm": 0.330, "npc": 0.00002668, "load_n": LOAD_N,
                    "ctrl_kg": 1.0, "m_wheel": 0.95, "j_wheel": None, "a0": 0.05, "b0": 1.1e-5,
                    "a1": 0.004, "b1": 2.0e-6, "plateaus": list(PLATEAUS), "tare": 81234}
        day = 86400
        now = int(time.time())
        # last-changed stamps as the firmware sends them (local time in s; 0 = unknown)
        self.chg = {"drum_u": now - 9 * day, "arm": now - 3 * day, "calib_arm": 0, "n_per_count": now - 3 * day,
                    "load_n": now - 2 * day, "ctrl_kg": now - day, "plateaus": now - 5 * day,
                    "tare": now - 3600, "a0": now - 2 * day, "a1": now - 2 * day, "m_wheel": now - 6 * day}
        self.run = Run(self)
        self.trace = []          # raw values at 80 SPS, last TRACE_N
        self.trace_t = time.time()

    def up_ms(self):
        return int((time.time() - self.t0) * 1000)

    def stamp(self, k):
        self.chg[k] = self.wall or int(time.time())

    def tick(self, dt):
        rate = 6.0 * dt
        self.kmh += max(-rate, min(rate, self.target - self.kmh))
        if self.target == 0 and self.kmh < 0.3:
            self.kmh = 0.0
        if self.sd_fmt_until and time.time() > self.sd_fmt_until:
            self.sd_fmt_until, self.sd, self.sd_fmt_res = 0, 1, "ok"; self.sd_fmt_n += 1
        if self.rec:
            self.rec_kb += 2.1 * dt
        # HX711 at standstill: white noise plus a slow wobble (air, cable)
        now = time.time()
        while self.trace_t < now:
            self.trace_t += 1 / 80
            wob = 25 * math.sin(2 * math.pi * 0.6 * self.trace_t)
            self.trace.append(int(81234 + wob + random.gauss(0, 12) + (300 * math.sin(self.trace_t * 9) if self.kmh > 0.5 else 0)
                                  + self.run.load_counts()))
        del self.trace[:-160]
        self.run.tick()

    def force(self):
        v = self.kmh
        if v <= 0.1:
            return random.gauss(0, 0.01)
        f = (m_idle(v) / R_DRUM) + (crr_true(v) * LOAD_N if self.loaded else 0)
        return f + random.gauss(0, 0.03)

    def state(self, keys):
        v = self.kmh
        f = self.force()
        nm = f * R_DRUM
        pw = f * v / 3.6
        crr = (crr_true(v) + random.gauss(0, 0.00003)) if (self.loaded and v > 5) else None
        steady = abs(self.target - v) < 0.3 and v > 5
        erpm = v / 3.6 / DRUM_U * 60 * 15
        sd_pct = {1: 37, 2: 81}.get(self.sd)
        d = {k: 0 for k in keys}
        d.update({
            "link": True, "fault": 0, "faultTxt": "", "status": "", "age": 40, "fw": "6.05", "note": self.note,
            "noteBad": False, "braking": False, "kmh": round(v, 2), "target": self.target, "ramp": 0,
            "duty": round(v / 100, 3), "driving": v > 0.5, "erpm": round(erpm), "rpm": round(v / 3.6 / DRUM_U * 60, 1),
            "vin": 36.4, "imot": round(0.4 + pw / 36, 2), "iin": round(pw / 36 * 0.6, 2), "watt": round(pw * 1.6, 1),
            "tfet": 31.2, "tmot": 27.5, "tmotOpen": False, "counts": round(f / self.set["npc"] * R_DRUM / ARM),
            "raw": 81234 + round(f / self.set["npc"]), "rawNow": True, "tare": self.set["tare"], "ripple": 0.04,
            "n": round(f, 3), "pct": round(abs(nm / ARM) / 49.03 * 100, 1), "kg": round(nm / ARM / 9.81, 3),
            "nm": round(nm, 4), "roll": round(f, 3) if self.loaded else None, "pw": round(pw, 2),
            "crr": crr, "corr": crr, "steady": steady, "a0": self.set["a0"], "b0": self.set["b0"],
            "a1": self.set["a1"], "b1": self.set["b1"], "mWheel": self.set["m_wheel"], "jWheel": 0.098,
            "jMeas": self.set["j_wheel"], "spindown": None, "spindownN": 0, "bar": 5.0, "tTire": 21.5, "pSrc": "ble",
            "pName": "AirSpy 4C1A", "pBatt": 88, "pAge": 2, "pFresh": True, "pConn": True, "revSrc": "drum",
            "relayBrake": False, "wiz": 0, "wizKmh": 40, "taring": False, "descending": False, "uWheelMem": 2105,
            "err": self.err, "cal": True, "saturated": False, "cellOk": True, "dir": getattr(self, "dir", 1), "learning": False,
            "brk": time.time() < getattr(self, "jog_until", 0), "brkDuty": (0.042 if getattr(self, "jog_until", 0) and time.time() >= self.jog_until else 0),
            "brkAmps": 0, **self.dir_json(), "up": self.up_ms(),
            "sensors": [[1200 + int(v * 40), 12, False]] * 4, "mSrc": "hall", "mPpr": 90, "mRes": 0.1, "mMiss": 0,
            "mGl": 0, "ldEmpty": 2.1, "ldWeight": 25.0, "ldArm": 0.42, "nDrum": 8, "nWheel": 1, "uWheel": 2105,
            "uWheelN": 10, "uWheelMin": 10, "plateaus": self.set["plateaus"], "phase": "SETTLE" if v > 0.5 and not steady else ("POINT" if steady else "IDLE"),
            "run": 0, "plateau": 0, "sfault": "", "load": self.set["load_n"], "motorOpen": False, "estop": False,
            "drumU": self.set["drum_u"], "arm": self.set["arm"], "calArm": self.set["calib_arm"], "npc": self.set["npc"],
            "vmax": 100, "vmin": 10, "step": 5, "fsN": 49.03, "maxN": 45, "ctrlKg": self.set["ctrl_kg"], "mMin": 0.5, "mMax": 1.4, "stopMin": 100, "stopMax": 140, "stopBar": 160,
            "rs": self.run.step if self.run.step else 0, "build": "fake", "mRad": self.set["m_wheel"],
            "sto": {"sd": self.sd, "sdPct": sd_pct, "sdMB": 30436, "sdFmt": bool(self.sd_fmt_until), "sdFmtRes": "" if self.sd_fmt_until else self.sd_fmt_res, "sdFmtN": self.sd_fmt_n, "sdFs": "FAT32" if self.sd in (1, 2) else "", "flFs": "LittleFS", "fl": self.fl,
                    "flPct": 2, "flMB": 3, "flFmt": False, "rec": self.rec, "recMan": self.rec_man,
                    "recOn": "sd", "recKB": int(self.rec_kb), "recPend": 3, "recDrop": 0},
            "chg": self.chg,
        })
        return d

    def dir_json(self):
        """Direction detection as speedctl: 3 s slow start, then positive (fake)."""
        u = getattr(self, "dir_until", 0)
        if u and time.time() >= u and getattr(self, "dir_res", -2) == -2:
            self.dir_res = 1
        run = bool(u) and time.time() < u
        return {"dirDet": run, "dirRes": getattr(self, "dir_res", -2), "dirDN": 2.4 if not run and u else (0.8 * (3 - (u - time.time())) if run else 0)}

    def cmd(self, c):
        self.note = ""
        arg = None
        m = re.match(r"^([A-Za-z]+?)(-?[0-9.,]+)?$", c)
        if c.startswith("R"):
            return self.run.cmd(c)
        if m and m.group(2):
            arg = float(m.group(2).replace(",", "."))
        k = c[:1]
        if c == "ed":
            self.dir_until, self.dir_res = time.time() + 3, -2
        elif k == "e" and arg in (1.0, -1.0):
            self.dir = int(arg)
        elif k == "x":
            self.jog_until, self.note = time.time() + 3, "Slow test, keep clear"
        elif k == "v" and arg is not None:
            self.target = max(0.0, min(100.0, arg))
        elif k == "s":
            self.target, self.note = 0.0, "STOP"
        elif k == "c":
            self.target = 0.0
        elif k == "t":
            self.stamp("tare")
        elif k == "F":
            which = c[1:2]
            if which == "s" and self.sd in (1, 2, 3):   # firmware: SD also when readable
                self.sd_fmt_until = time.time() + 4
            elif which == "f":
                self.note = "Flash is readable"
        elif c == "L1":
            if self.sd != 1:
                self.note = "No SD"
                return 409, "No SD"
            self.rec, self.rec_man, self.rec_kb = True, True, 0
        elif c == "L0":
            self.rec = False
        elif k == "h" and arg is not None:
            p = self.set["plateaus"]
            if arg < 0 and -arg in p:
                p.remove(-arg)
            elif arg > 0 and arg not in p:
                p.append(arg)
                p.sort()
            self.stamp("plateaus")
        else:
            key = {"d": ("drum_u", "drum_u"), "a": ("arm", "arm"), "b": ("calib_arm", "calib_arm"),
                   "l": ("load_n", "load_n"), "K": ("ctrl_kg", "ctrl_kg"), "y": ("m_wheel", "m_wheel"),
                   "Y": ("j_wheel", "j_wheel")}.get(k)
            if key and arg is not None:
                self.set[key[0]] = arg
                self.stamp(key[1])
            elif k == "z":
                self.chg = {}
        return 200, "ok"


# ------------------------------------------------------------------- guided run
TITLES = {1: "Create run", 2: "Visual check, E-stop", 3: "System check", 4: "Zero, wheel lifted",
          5: "Check weight, end stop", 6: "Idle + wheel spin-down", 7: "Wheel model", 8: "Wheel down, load",
          9: "Warm-up", 10: "Reference + plateaus", 11: "Reference after", 12: "Stop", 13: "Zero, post-run",
          14: "Protocol", 99: "Aborted"}


class Run:
    def __init__(self, stand):
        self.s = stand
        self.reset()

    def reset(self):
        self.step, self.sub, self.t_sub = 0, 0, time.time()
        self.id = ""
        self.meta = {"q": -1, "u": -1, "k": None, "t": "", "w": "", "h": "", "n": "", "r": "", "s": "", "b": None, "c": None, "l": None, "lc": None, "p": 0}
        self.checks, self.pre, self.post, self.warm = [], [], [], []
        self.anchor_pre, self.pts, self.anchor, self.stat, self.warns = [], [], [], [], []
        self.tare_sigma = self.tare_chk = self.zero_end = None
        self.stop_live = self.stop_peak = None
        self.ctrl_live = self.ctrl_nm = self.ctrl_dev = None
        self.ctrl_adopted = False
        self.ctrl_rep = self.stop_zero = None
        self.a0 = self.b0 = None
        self.anchor_dn = None
        self.k = 0
        self.prog = None
        self.prog_txt = ""

    def el(self):
        return (time.time() - self.t_sub) * self.s.fast

    def go(self, step, sub=0):
        self.step, self.sub, self.t_sub, self.k = step, sub, time.time(), 0
        self.prog = None

    def next_sub(self):
        self.sub += 1
        self.t_sub, self.k = time.time(), 0
        self.prog = None

    def point(self, v, k, anchor=False):
        m = (m_idle(v) + crr_true(v) * LOAD_N * R_DRUM) * (1 + random.gauss(0, 0.002))
        cr = (m - m_idle(v)) / R_DRUM / LOAD_N
        return [v, k + 1, round(m, 4), round(abs(random.gauss(0.004, 0.001)), 4), round(cr * 1.01, 5), round(cr, 5),
                True, round(random.gauss(0, 0.002), 4), round(random.gauss(0, 0.001), 4)]

    # automatic progress, called every tick
    def tick(self):
        st, sub, e = self.step, self.sub, self.el()
        S = self.s
        if False:
            pass
        elif st == 4 and sub == 2:
            # hand turn: 0..90 degrees in 3 s, then still after 4.5 s -> Measure button
            self.turn_deg = min(97.0, e * 25)
            self.turn_ready = e > 4.5
            self.prog = min(1, self.turn_deg / 90)
            self.prog_txt = (f"{self.turn_deg:.0f} degrees, still" if self.turn_ready else
                             f"{self.turn_deg:.0f} degrees, still moving" if self.turn_deg >= 90 else
                             f"{self.turn_deg:.0f} of 90 degrees turned")
        elif st == 4 and sub in (1, 3):
            self.prog, self.prog_txt = min(1, e / 4), "Taring" if sub == 1 else "Rotor turned, reading"
            if e >= 4:
                if sub == 1:
                    self.tare_sigma = 0.012
                else:
                    self.tare_chk = round(random.gauss(0.04, 0.03), 3)
                    S.stamp("tare")
                self.next_sub()
        elif st == 5 and sub == 0:
            # weight hangs (about 79 % FS), the press adds up to about 125 % FS, then back
            x = (e % 6) / 6
            self.stop_live = 79 + (46 * math.sin(math.pi * x) if e < 6 else 0)
            self.stop_peak = max(self.stop_peak or 0, self.stop_live)
        elif st == 5 and sub in (4, 6):
            self.prog, self.prog_txt = min(1, e / 4), ("Reading zero point" if sub == 4 else "Reading check weight again")
            if e >= 4:
                if sub == 4:
                    self.stop_zero = round(random.gauss(0.02, 0.02), 3)
                    self.sub = 5
                else:
                    self.ctrl_rep = round(random.gauss(0.05, 0.08), 3)
                    self.sub = 7
                self.t_sub, self.prog = time.time(), None
        elif st == 5 and sub == 1:
            tg = S.set["ctrl_kg"] * 9.81 * S.set["calib_arm"]
            self.ctrl_live = tg * (1.003 + random.gauss(0, 0.0005)) if e > 2 else tg * e / 2
        elif st == 5 and sub == 2:
            self.prog, self.prog_txt = min(1, e / 4), "Check weight, measuring"
            tg = S.set["ctrl_kg"] * 9.81 * S.set["calib_arm"]
            self.ctrl_live = tg * (1.0031 + random.gauss(0, 0.0005))
            if e >= 4:
                self.ctrl_raw = round(tg * 1.0031 / (S.set["npc"] * S.set["arm"]), 1)
                self.ctrl_nm = tg * 1.0031
                self.ctrl_dev = 0.31
                self.next_sub()
        elif st == 6 and sub == 1:
            plat = S.set["plateaus"]
            i = int(e // 4)
            if i < len(plat):
                S.target = plat[i]
                self.prog, self.prog_txt = i / len(plat), f"Idle at {plat[i]:.1f} km/h"
                if e % 4 > 3 and len(self.pre) == i:
                    v = plat[i]
                    self.pre.append([v, i + 1, round(m_idle(v), 4), 0.003, True, round(random.gauss(0, 0.002), 4), 0.0])
            else:
                S.target = 0
                self.next_sub()
        elif st == 6 and sub == 2:
            self.prog, self.prog_txt = min(1, e / 6), "Wheel spin-down"
            if e >= 6:
                self.a0, self.b0 = 0.0512, 1.08e-5
                S.set["a0"], S.set["b0"] = self.a0, self.b0
                S.stamp("a0")
                self.next_sub()
        elif st == 9:
            warm_ms = 60000
            S.target = 28.8
            S.loaded = True
            self.sub_ms = min(warm_ms, e * 1000 * 4)
            minute = int(self.sub_ms / 2000)
            while len(self.warm) < minute:
                n = len(self.warm) + 1
                c = crr_true(28.8) * (1 + 0.06 * math.exp(-n / 8))
                self.warm.append([n, round(c * LOAD_N * 8, 1), round(c, 5), 5.0])
            if self.sub_ms >= warm_ms:
                self.go(10)
        elif st == 10:
            S.loaded = True
            seq = [28.8] + S.set["plateaus"]
            i = int(e // 3)
            if i < len(seq):
                S.target = seq[i]
                self.prog, self.prog_txt = i / len(seq), f"Point {i + 1} of {len(seq)} · {seq[i]:.1f} km/h"
                done = len(self.anchor_pre) + len(self.pts)
                if e % 3 > 2.5 and done == i:
                    if i == 0:
                        self.anchor_pre.append(self.point(seq[i], 0))
                    else:
                        self.pts.append(self.point(seq[i], i - 1))
            else:
                self.go(11)
        elif st == 11:
            S.target = 28.8
            self.prog, self.prog_txt = min(1, e / 4), "Reference after"
            if e >= 4 and not self.anchor:
                self.anchor.append(self.point(28.8, 0))
                self.anchor_dn = round(self.anchor[0][2] / R_DRUM - self.anchor_pre[0][2] / R_DRUM, 3)
                self.go(12)
        elif st == 12:
            S.target = 0
            self.prog, self.prog_txt = 1 - S.kmh / 30, "Braking"
            if S.kmh == 0:
                S.loaded = False
                self.go(13)
        elif st == 13 and sub == 1:
            self.prog, self.prog_txt = min(1, e / 4), "Zero after run"
            if e >= 4:
                self.zero_end = round(random.gauss(0.05, 0.03), 3)
                self.next_sub()
        elif st == 13 and sub == 2:
            plat = S.set["plateaus"]
            i = int(e // 3)
            if i < len(plat):
                S.target = plat[i]
                self.prog, self.prog_txt = i / len(plat), f"Idle after at {plat[i]:.1f} km/h"
                if e % 3 > 2 and len(self.post) == i:
                    self.post.append([plat[i], round(m_idle(plat[i]) * 1.01, 4), round(random.gauss(0.01, 0.01), 3)])
            else:
                S.target = 0
                self.next_sub()

    def make_checks(self):
        S = self.s
        sd = S.sd
        self.checks = [
            ["Controller", 1, "FW 6.05 · 36.4 V · no fault"],
            ["Motor on controller", 1, "disconnect quiet"],
            ["Load cell", 1, "responds · 0.3 % of full scale"],
            ["Calibration factor", 1, f"{S.set['npc']:.4g} N/count"],
            ["Cell arm", 1, f"{S.set['arm'] * 1000:.1f} mm · L_cal {S.set['calib_arm'] * 1000:.1f} mm"],
            ["Revolutions", 1, "source drum"],
            ["Tire pressure", 1, "AirSpy 4C1A · 5.00 bar · 21.5 °C · battery 88 %"],
            ["Plateaus", 1, " · ".join(f"{p:.1f}" for p in S.set["plateaus"]) + " km/h, reference 28.8 before and after"],
            ["Load", 1, f"{S.set['load_n']:.1f} N"],
            ["Storage", 1, "SD 37 % used · protocol and raw data"] if sd == 1
            else ["Storage", 0, f"SD {['absent', 'ready', 'read-only', 'unreadable'][sd]}: protocol to flash, no raw data"],
        ]

    def finish(self):
        self.stat = []
        for p in self.anchor_pre + self.pts:
            self.stat.append([p[0], 10, 10, p[2], p[3], p[4], p[5], 0.00004, 9])
        if self.anchor_dn is not None and abs(self.anchor_dn) > 0.05:
            self.warns.append(f"W04 Reference drift {self.anchor_dn:+.3f} N")
        if self.s.rec:
            self.s.rec = False

    def cmd(self, c):
        S = self.s
        st, sub = self.step, self.sub
        if c == "R":
            if st == 0:
                self.reset()
                self.id = time.strftime("%Y-%m-%d-%H%M%S")
                self.go(1)
            return 200, "ok"
        if c.startswith("Rm"):
            k, _, v = c[2:].partition("=")
            if k in ("b", "c", "l", "lc", "p", "k", "u"):   # numbers in the firmware JSON (run.cpp json())
                try:
                    v = float(v.replace(",", "."))
                    v = int(v) if k in ("p", "u") else v
                except ValueError:
                    v = 0 if k == "p" else (-1 if k == "u" else None)
            if k == "q":
                try:
                    i = int(v); p = PROFILES.d["prof"][i]
                    self.meta.update({"q": i, "t": p["t"], "s": p["s"], "w": p["r"], "h": p["h"]})
                except (ValueError, IndexError):
                    self.meta["q"] = -1
                return 200, "ok"
            self.meta[k] = v
            return 200, "ok"
        if c == "Rx":
            if st not in (0, 14, 99):
                S.target = 0
                self.warns.append(f"W06 Run aborted in step {st}: cancelled")
                self.finish()
                self.go(99)
            return 200, "ok"
        if c == "Rb":
            if 2 <= st <= 8:
                if st == 4:
                    self.make_checks()
                self.go(st - 1, 1 if st == 4 else 0)
            return 200, "ok"
        if c == "Rn":
            if st == 3:
                self.make_checks()
            elif st == 4:
                self.go(4)
            elif st == 5 and sub == 3:
                self.go(5, 1)
            elif st == 5 and sub == 0:
                self.go(5, 0)
                self.stop_peak = None
            elif st == 5 and sub == 7:
                self.go(5, 6)
            elif st == 5 and sub == 5:
                self.go(4)
            return 200, "ok"
        if c == "Rs":
            if st == 5 and sub == 3:
                self.ctrl_adopted = True
                S.stamp("n_per_count")
            elif st == 5 and sub == 1:
                self.warns.append("W01 Check weight skipped")
                self.stop_peak = None
                self.go(5, 0)
            elif st == 5 and sub == 0:
                self.warns.append("W17 End-stop check skipped")
                self.go(5, 7)
            elif st == 6 and sub == 0:
                self.go(7)
            elif st == 7:
                self.go(8)
            elif st == 13 and sub == 0:
                self.finish()
                self.go(14)
            elif st == 9:
                self.sub_ms = 60000
                self.go(10)
            return 200, "ok"
        if c != "Rk":
            return 200, "ok"
        # Rk: the main button
        if st == 1:
            q = self.meta.get("q", -1)
            if q is None or q < 0 or PROFILES.miss(PROFILES.d["prof"][q]):
                return 200, "ok"
            self.go(2)
            if S.sd == 1:
                S.rec, S.rec_man, S.rec_kb = True, False, 0
        elif st == 2:
            self.make_checks()
            self.go(3, 1)
        elif st == 3 and sub == 1:
            self.go(4)
        elif st == 4 and (sub == 0 or (sub == 2 and getattr(self, "turn_ready", False))):
            self.turn_deg, self.turn_ready = 0, False
            self.next_sub()
        elif st == 4 and sub == 4:
            self.go(5, 1)
        elif st == 5 and sub == 1:
            self.next_sub()
        elif st == 5 and sub == 3:
            self.stop_peak = None
            self.go(5, 0)
        elif st == 5 and sub == 0:
            self.go(5, 6)
        elif st == 5 and sub == 7:
            self.go(5, 4)
        elif st == 5 and sub == 5:
            self.go(6)
        elif st == 6 and sub == 0:
            self.next_sub()
        elif st == 6 and sub == 3:
            self.go(7)
        elif st == 7:
            self.go(8)
        elif st == 8:
            self.go(9)
        elif st == 13 and sub == 0:
            self.next_sub()
        elif st == 13 and sub == 3:
            self.finish()
            self.go(14)
        elif st in (14, 99):
            self.reset()
        return 200, "ok"

    def buttons(self):
        st, sub = self.step, self.sub
        ok = alt = redo = None
        back = 2 <= st <= 8
        txt = ""
        if st == 1:
            ok, txt = "Next", ""
        elif st == 2:
            ok, txt = "Checked", "Drum and weighing part free; Wheel secured; E-stop within reach"
        elif st == 3:
            ok, redo, txt = "Next", "Again", "Everything the run needs now is there."
        elif st == 4:
            ready = getattr(self, "turn_ready", False)
            turned = getattr(self, "turn_deg", 0) >= 90
            txt = {0: "Lift the wheel off the drum, rotor free.", 1: "Hands off.",
                   2: "Quarter turn reached, drum still. Hands off, then measure." if ready
                   else "Quarter turn reached. Wait until the drum is still." if turned
                   else "Turn the drum further by hand until the bar is full, then let go.",
                   3: "Hands off.", 4: ""}[sub]
            if sub == 0:
                ok = "Wheel free, drum still"
            if sub == 2 and ready:
                ok = "Measure"
            if sub == 4:
                ok, redo = "Next", "Again"
        elif st == 5:
            if sub == 0:
                txt, ok, alt, redo = "Check weight stays on. Push the arm slowly against the end stop, then let go and wait until the weight hangs still.", "Released, still", "Skip", "Again"
            elif sub == 1:
                txt, ok, alt = "Hang the check weight.", "Weight hangs", "Skip"
            elif sub in (2, 4, 6):
                txt = "Hands off, measuring."
            elif sub == 3:
                txt, ok, alt, redo = "Matches the calibration. Leave the weight on: the end stop comes next.", "Next", None if self.ctrl_adopted else "Adopt as calibration", "Again"
            elif sub == 7:
                txt, ok, redo = "Same as before the press. Remove the weight, then Next.", "Next", "Again"
            else:
                txt, ok = "Zero unchanged: the overload left no bias.", "Next"
        elif st == 6:
            if sub == 0:
                txt, ok, alt = ("Spin up with the wheel resting, lift at 40 km/h: idle loss and wheel model in one. Same wheel as the stored model "
                                "(Schwalbe SX-R \u00b7 Vittoria Latex \u00b7 test wheel, 4.50 bar, run 2026-11-25-183012): stored allowed in a pressure series.",
                                "Measure new", "Use stored")
            elif sub == 3:
                ok = "Next"
            else:
                txt = "Keep clear."
        elif st == 7:
            txt, ok, alt = "From this run. J_wheel measured. At 30 km/h 0.0121 Nm, previous model of this wheel 0.0118 Nm (+3 %).", "Next", None
        elif st == 8:
            txt, ok = "Sliding weight in position, beam level, wheel centred.", "Start measurement"
        elif st == 9:
            alt = "Skip rest"
        elif st == 13:
            if sub == 0:
                txt, ok, alt = "Lift the wheel off the drum.", "Wheel lifted", "Skip"
            elif sub == 3:
                ok = "Next"
        elif st in (14, 99):
            ok = "Close"
        return ok, alt, redo, back, txt

    def json(self):
        S = self.s
        ok, alt, redo, back, txt = self.buttons()
        tg = S.set["ctrl_kg"] * 9.81 * S.set["calib_arm"]
        live = self.step == 9 and S.kmh > 25
        md = f"/fs/sd/vmrr/{self.id}/protocol.md" if self.step in (14, 99) else None
        if md:
            FILES.write_run(self.id)
        return {
            "step": self.step, "sub": self.sub, "id": self.id, "title": TITLES.get(self.step, ""), "txt": txt,
            "ok": ok, "alt": alt, "redo": redo, "back": back, "subMs": getattr(self, "sub_ms", 0), "warmMs": 60000,
            "lvP": round(crr_true(28.8) * LOAD_N * 8, 1) if live else None,
            "lvCrr": round(crr_true(28.8), 5) if live else None, "lvM": None,
            "prog": self.prog, "progTxt": self.prog_txt, "tareSigma": self.tare_sigma,
            "tareSigmaN": self.tare_sigma, "tareChk": self.tare_chk, "zeroEnd": self.zero_end, "zeroLim": 0.25,
            "ctrlNm": self.ctrl_nm, "ctrlDev": self.ctrl_dev, "ctrlOn": self.step == 5 and self.sub in (1, 2, 3, 0, 6),
            "ctrlRep": getattr(self, "ctrl_rep", None), "stopZero": getattr(self, "stop_zero", None),
            "ctrlAdopted": self.ctrl_adopted, "turnDeg": getattr(self, "turn_deg", None) if (self.step == 4 and self.sub == 2) else None, "ctrlTarget": tg, "ctrlLive": self.ctrl_live,
            "ctrlExpect": (tg / (S.set["npc"] * S.set["arm"])) if S.set.get("npc") else None,
            "ctrlRaw": getattr(self, "ctrl_raw", None),
            "tareAgeS": (time.time() - S.chg["tare"]) if S.chg.get("tare") else None,
            "calEpoch": S.chg.get("n_per_count", 0), "nowEpoch": S.wall or int(time.time()),
            "ctrlKg": (self.ctrl_nm / 9.81 / S.set["calib_arm"]) if self.ctrl_nm else None,
            "ctrlTargetKg": S.set["ctrl_kg"], "calArmMm": S.set["calib_arm"] * 1000,
            "stopLive": self.stop_live, "stopPeak": self.stop_peak, "stopMin": 100, "stopMax": 140, "stopBar": 160,
            "bar": 5.0, "barWarm": 5.0, "tTire": 21.5, "meta": self.meta, "checks": self.checks,
            "pre": self.pre, "a0": self.a0, "b0": self.b0, "a0Old": 0.05, "post": self.post, "warm": self.warm,
            "anchorPre": self.anchor_pre, "pts": self.pts, "anchor": self.anchor, "stat": self.stat,
            "anchorKmh": 28.8, "anchorDN": self.anchor_dn, "warn": self.warns, "md": md,
            "csv": f"/fs/sd/vmrr/{self.id}/points.csv" if md else None, "dir": f"/fs/sd/vmrr/{self.id}" if md else None,
            "plateaus": S.set["plateaus"],
            **self.trace_json(),
        }

    def load_counts(self):
        """Counts the hanging check weight adds to the fake cell (step 5)."""
        S = self.s
        if self.step != 5 or self.sub in (4, 5) or not S.set.get("npc"):
            return 0
        tg = S.set["ctrl_kg"] * 9.81 * S.set["calib_arm"]
        if self.sub == 1:
            return (self.ctrl_live or 0) / (S.set["npc"] * S.set["arm"])
        return tg * (1.0036 if self.sub == 6 else 1.0031) / (S.set["npc"] * S.set["arm"])

    def trace_json(self):
        S = self.s
        if not (self.step == 4 or (self.step == 13 and self.sub == 1)
                or (self.step == 5 and (self.sub >= 4 or self.sub in (1, 2)))) or not S.trace:
            return {}
        mean = round(sum(S.trace) / len(S.trace))
        npc_froll = S.set["npc"] * S.set["arm"] / R_DRUM
        return {"trMean": mean, "trNow": S.trace[-1], "trNpc": npc_froll, "trTare": S.set["tare"],
                "trace": [v - mean for v in S.trace]}


# ------------------------------------------------------------------- HTTP
class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def out(self, code, body, ctype="application/json"):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        with LOCK:
            if u.path in ("/", "/index.html"):
                return self.out(200, PAGE.read_text(), "text/html")
            if u.path == "/api/state":
                if "now" in q:
                    STAND.wall = int(float(q["now"][0]))
                return self.out(200, json.dumps(STAND.state(KEYS)))
            if u.path == "/api/run":
                return self.out(200, json.dumps(STAND.run.json()))
            if u.path == "/api/profiles":
                return self.out(200, json.dumps(PROFILES.out()))
            if u.path == "/api/files":
                return self.out(200, json.dumps(FILES.listing(q.get("m", ["sd"])[0], q.get("p", ["/vmrr"])[0])))
            if u.path.startswith("/fs/"):
                code, body, ctype = FILES.get(u.path, [a.split("=")[0] for a in u.query.split("&")])
                return self.out(code, body, ctype)
        self.out(404, "not found", "text/plain")

    def do_POST(self):
        u = urlparse(self.path)
        if u.path == "/api/profiles":
            with LOCK:
                return self.out(200, json.dumps(PROFILES.op(parse_qs(u.query))))
        if u.path == "/api/cmd":
            c = parse_qs(u.query).get("c", [""])[0]
            with LOCK:
                code, msg = STAND.cmd(c)
            print("cmd", c, code)
            return self.out(code, msg, "text/plain")
        self.out(404, "not found", "text/plain")


class Files:
    """The SD card and flash as the file browser sees them (webui.cpp /api/files, /fs).
    Demo content in memory, clearly marked as fake; a finished fake run adds its files."""

    def __init__(self):
        now = int(time.time())
        demo = ("# Measurement run 2026-11-25-183012 (FAKE)\n\nDemo content of tools/fake_stand.py, "
                "not a measurement.\n\n## Wheel\n\n| Part | Name |\n|---|---|\n| Tire | Tire A, 28-622 |\n"
                "| Tube | Latex tube (latex) |\n\n## Points\n\n| km/h | Crr |\n|---|---|\n| 28.8 | 0.00451 |\n\n"
                "- tare raw value `81234`\n- **check weight** +0.31 %\n")
        self.fs = {"sd": {"/vmrr": None, "/vmrr/2026-11-25-183012": None,
                          "/vmrr/2026-11-25-183012/protocol.md": (demo, now - 86400),
                          "/vmrr/2026-11-25-183012/points.csv": ("kind,plateau,v_kmh,crr\nmeasure,1,28.80,0.00451\n", now - 86400),
                          "/vmrr/2026-11-26-101500": None,
                          "/raw": None},
                   "flash": {"/vmrr": None, "/profiles.tsv": ("RRP2\n", now - 7200)}}

        self.add_raw("2026-11-25-183012", 1200000, now - 86400, "Recorded by the guided run `2026-11-25-183012`.")
        self.add_raw("2026-11-26-101500", 118562, now - 600, "Started by hand (console `L1` or Manual), not by a run.")

    @staticmethod
    def fake_meta(name, unix):
        """raw_<s>.json in the shape of calc/rawlog.h header(); FAKE values, dimensions and
        calibration unknown."""
        return json.dumps({"file": name + ".csv", "legend": name + ".md", "format": "vmrr-raw", "version": 3,
                           "firmware": "FAKE", "commit": "FAKE", "stand_rev": "FAKE stand (tools/fake_stand.py)",
                           "start_local": "FAKE", "unix_start_local": unix, "run_id": None,
                           "setup": {"marks_per_rev": 8, "wheel_marks_per_rev": 1, "motor_pole_pairs": 15,
                                     "drum_circumference_m": None, "torque_arm_m": None, "hx711_rate_sps": 80,
                                     "wheel_circumference_m": None},
                           "calibration": {"loadcell_n_per_count": None, "tare_offset_counts": 0, "validity": -1,
                                           "validity_text": "not calibrated (FAKE)", "record": None}}, indent=2)

    def add_raw(self, sess, size, t, origin):
        d, name = self.fs["sd"], f"raw_{sess}"
        d[f"/vmrr/{sess}/{name}.csv"] = ("x" * size, t)
        d[f"/vmrr/{sess}/{name}.json"] = (self.fake_meta(name, t - 300), t)
        d[f"/vmrr/{sess}/{name}.md"] = (f"# Raw log {name} (FAKE)\n\n{origin}\n\nDemo content of tools/fake_stand.py.\n", t)

    def write_run(self, rid):
        d = self.fs["sd"]
        if f"/vmrr/{rid}/protocol.md" in d:
            return
        now = int(time.time())
        d[f"/vmrr/{rid}"] = None
        d[f"/vmrr/{rid}/protocol.md"] = (f"# Measurement run {rid} (FAKE)\n\nFake run of tools/fake_stand.py.\n", now)
        d[f"/vmrr/{rid}/points.csv"] = ("kind,plateau,v_kmh\nmeasure,1,28.80\n", now)
        self.add_raw(rid, 900000, now, f"Recorded by the guided run `{rid}`.")

    def listing(self, m, p):
        d = self.fs.get(m, {})
        p = p.rstrip("/") or "/"
        ok = p == "/" or (p in d and d[p] is None)
        out = []
        for k, v in d.items():
            parent = k.rsplit("/", 1)[0] or "/"
            if parent == p and k != p:
                out.append({"n": k.rsplit("/", 1)[1], "dir": 1 if v is None else 0,
                            "s": 0 if v is None else len(v[0]), "t": 0 if v is None else v[1]})
        return {"m": m, "p": p, "state": 1, "fs": "FAT32" if m == "sd" else "LittleFS", "pct": 37 if m == "sd" else 2,
                "mb": 3814 if m == "sd" else 3, "rec": "", "d": out, "ok": ok}

    def get(self, path, q):
        m, _, rest = path[4:].partition("/")
        p = "/" + rest.rstrip("/")
        d = self.fs.get(m, {})
        if p not in d:
            return 404, "not found", "text/plain"
        if "info" in q and p.rsplit("/", 1)[-1].startswith("raw_") and p.endswith(".csv"):   # FAKE, shape as webui.cpp sendRawInfo
            n = len(d[p][0])
            meta = d.get(p[:-4] + ".json")
            hdr = meta[0] if meta else ""
            return 200, json.dumps({"size": n, "hdrLen": 0, "ok": True, "unix": int(d[p][1]) - 300,
                "hdr": hdr, "t0": 0, "t1": 300_000_000, "end": "clean", "bad": 0, "gap": 0,
                "types": [{"t": "S", "n": 300, "b": 10500}, {"t": "D", "n": 18012, "b": 270180},
                          {"t": "W", "n": 3000, "b": 45000}, {"t": "T", "n": 24000, "b": 456000},
                          {"t": "M", "n": 1500, "b": 93000}, {"t": "C", "n": 600, "b": 18600},
                          {"t": "P", "n": 60, "b": 1320}, {"t": "N", "n": 3, "b": 110}],
                "edges": [[18000, 12], [0, 0], [3000, 0], [0, 0]], "textN": 3,
                "text": [[0, "raw log started by hand"], [120000000, "note FAKE"], [300000000, "raw log stopped"]]}), "application/json"
        if "zip" in q or "csv" in q:
            return 200, b"PK\x05\x06" + b"\0" * 18, "application/zip"
        if d[p] is None:
            return 404, "not found", "text/plain"
        return 200, d[p][0], "text/plain" if "view" in q else "application/octet-stream"


FILES = Files()


class Profiles:
    """Wheel profiles as firmware/src/profiles.cpp keeps them; demo entries, not real wheels."""
    L = {"t": "tire", "s": "tube", "r": "rim", "h": "hub"}

    def __init__(self):
        self.d = {"tire": [{"n": "Tire A", "e": "28-622"}, {"n": "Tire B", "e": "32-622"}],
                  "tube": [{"n": "Latex tube", "a": ["Maker A", "Latex", "latex"]}, {"n": "Butyl tube", "a": ["Maker B", "Butyl", "butyl"]}],
                  "rim": [{"n": "Rim 1", "a": ["Maker C", "Rim 1", "19", "622"]}],
                  "hub": [{"n": "Hub 1", "sp": 32, "ax": "quick release", "br": "none", "dr": 0, "co": "silver"}],
                  "prof": [{"t": "Tire A", "s": "Latex tube", "r": "Rim 1", "h": "Hub 1", "a1": 0.011, "b1": 2.1e-6,
                            "u": 2.105, "m": 1.42, "j": None, "bar": 4.5, "run": "2026-11-25-183012", "ep": 1795631412,
                            "bd": None, "bl": None, "bn": None, "bt": None}]}

    def miss(self, p):
        m = []
        t = next((i for i in self.d["tire"] if i["n"].lower() == p["t"].lower()), None)
        if not p["t"]: m.append("tire")
        elif not (t and t.get("e")): m.append("ETRTO")
        for k, w in (("s", "tube"), ("r", "rim"), ("h", "hub")):
            if not p[k]: m.append(w)
        h = next((i for i in self.d["hub"] if i["n"].lower() == p["h"].lower()), {}) if p["h"] else None
        if h is not None:
            if not h.get("sp"): m.append("spokes")
            if not h.get("ax"): m.append("axle")
            if not h.get("br"): m.append("brake")
            if h.get("dr", -1) not in (0, 1): m.append("drive wheel")
        u = next((i for i in self.d["tube"] if i["n"].lower() == p["s"].lower()), None) if p["s"] else None
        if p["s"] and not (u and len(u.get("a", [])) > 2 and u["a"][2]): m.append("tube material")
        r = next((i for i in self.d["rim"] if i["n"].lower() == p["r"].lower()), None) if p["r"] else None
        if p["r"]:
            a = (r or {}).get("a", ["", "", "", ""])
            if not a[2]: m.append("rim inner width")
            if not a[3]: m.append("rim diameter")
            elif t and "-" in t.get("e", "") and t["e"].split("-")[1] != a[3]: m.append("rim diameter does not match the tire ETRTO")
        if p.get("m") is None: m.append("mass")
        return ", ".join(m)

    def out(self):
        o = json.loads(json.dumps(self.d))
        for k in ("tire", "tube", "rim", "hub"):
            for it in o[k]:
                it["use"] = sum(1 for p in self.d["prof"] if p[k[0] if k != "tube" else "s"].lower() == it["n"].lower())
                it.setdefault("e", ""); it["ep"] = 0
        for i, p in enumerate(o["prof"]):
            p["miss"] = self.miss(self.d["prof"][i])
        o["pend"] = False
        return o

    def touch(self, m):
        if not m.get("t"):
            return
        for key, lst in (("t", "tire"), ("s", "tube"), ("w", "rim"), ("h", "hub")):
            v = (m.get(key) or "").strip()
            if v and not any(i["n"].lower() == v.lower() for i in self.d[lst]):
                self.d[lst].append({"n": v, "e": m.get("r", "")} if lst == "tire" else {"n": v})
        k = [(m.get(x) or "").strip().lower() for x in ("t", "s", "w", "h")]
        if not any([p["t"].lower(), p["s"].lower(), p["r"].lower(), p["h"].lower()] == k for p in self.d["prof"]):
            self.d["prof"].append({"t": m["t"], "s": m.get("s", ""), "r": m.get("w", ""), "h": m.get("h", ""), "a1": None, "b1": None,
                                   "u": None, "m": None, "j": None, "bar": None, "run": "", "ep": 0,
                                   "bd": None, "bl": None, "bn": None, "bt": None})

    def op(self, q):
        g = lambda k: q.get(k, [""])[0]
        op, lst = g("op"), self.L.get(g("l"), "tire")
        fk = {"tire": "t", "tube": "s", "rim": "r", "hub": "h"}[lst]
        items = self.d[lst]
        find = lambda n: next((i for i in items if i["n"].lower() == n.strip().lower()), None)
        if op == "ren":
            it = find(g("o"))
            if not it: return {"err": "Not found"}
            if find(g("n")) and find(g("n")) is not it: return {"err": "Name exists"}
            for p in self.d["prof"]:
                if p[fk].lower() == it["n"].lower(): p[fk] = g("n").strip()
            it["n"] = g("n").strip()
        elif op == "del":
            it = find(g("n"))
            if not it: return {"err": "Not found"}
            if any(p[fk].lower() == it["n"].lower() for p in self.d["prof"]): return {"err": "Used by a profile"}
            items.remove(it)
        elif op == "etrto":
            it = next((i for i in self.d["tire"] if i["n"] == g("n")), None)
            if not it: return {"err": "Not found"}
            it["e"] = g("e")
        elif op == "attr":
            n = g("n").strip(); lst2 = "tube" if g("l") == "s" else "rim"
            it = next((i for i in self.d[lst2] if i["n"].lower() == n.lower()), None)
            if not it:
                it = {"n": n}; self.d[lst2].append(it)
            it["a"] = [g("a%d" % k) for k in range(3 if lst2 == "tube" else 4)]
        elif op == "hub":
            n = g("n").strip()
            it = next((i for i in self.d["hub"] if i["n"].lower() == n.lower()), None)
            if not it:
                it = {"n": n}; self.d["hub"].append(it)
            it.update({"sp": int(g("sp") or 0), "ax": g("ax"), "br": g("br"), "dr": int(g("dr")) if g("dr") else -1, "co": g("co")})
        elif op == "pset":
            i = int(g("i") or -1)
            num = lambda k: float(g(k)) if g(k) else None
            t, sv, r, h = (g(k).strip() for k in ("t", "s", "r", "h"))
            if not t: return {"err": "Tire empty"}
            key = [t.lower(), sv.lower(), r.lower(), h.lower()]
            for j, p in enumerate(self.d["prof"]):
                if j != i and [p["t"].lower(), p["s"].lower(), p["r"].lower(), p["h"].lower()] == key: return {"err": "Profile exists"}
            self.touch({"t": t, "s": sv, "w": r, "h": h, "r": g("e")})
            tire = next(x for x in self.d["tire"] if x["n"].lower() == t.lower()); tire["e"] = g("e") or tire.get("e", "")
            if i < 0:
                p = self.d["prof"][-1] if [self.d["prof"][-1][k].lower() for k in ("t", "s", "r", "h")] == key else None
                if p is None: return {"err": "fake: touch failed"}
            else:
                p = self.d["prof"][i]
                if [p[k].lower() for k in ("t", "s", "r", "h")] != key:
                    p.update({"a1": None, "b1": None, "bar": None, "u": None, "run": "", "ep": 0})
                p.update({"t": t, "s": sv, "r": r, "h": h})
            p.update({"m": num("m"), "bd": num("bd"), "bl": num("bl"), "bn": num("bn"), "bt": num("bt"), "j": num("j")})
        elif op in ("pdel", "pclr"):
            i = int(g("i") or -1)
            if not 0 <= i < len(self.d["prof"]): return {"err": "Not found"}
            if op == "pdel": del self.d["prof"][i]
            else: self.d["prof"][i].update({"a1": None, "b1": None, "bar": None, "run": "", "ep": 0})
        else:
            return {"err": "Unknown operation"}
        return self.out()


PROFILES = Profiles()


def ticker():
    last = time.time()
    while True:
        time.sleep(0.05)
        now = time.time()
        with LOCK:
            STAND.tick((now - last) * STAND.fast)
        last = now


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--sd", choices=list(SD_STATES), default="ready")
    ap.add_argument("--fast", type=float, default=1.0, help="speed-up of the run clock")
    a = ap.parse_args()
    KEYS = sorted(set(re.findall(r'\\"(\w+)\\":', STATEJSON.read_text())))
    STAND = Stand(a.sd, a.fast)
    threading.Thread(target=ticker, daemon=True).start()
    print(f"fake stand on http://localhost:{a.port}/  (SD {a.sd})")
    ThreadingHTTPServer(("", a.port), H).serve_forever()
