#!/usr/bin/env python3
"""Generate independent ngspice AC references for the selected Model T stack.

Usage: python tools/circuit_reference.py /path/to/ngspice
Does not import or evaluate the C++ implementation.
"""
import csv
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CONTROLS = [(0.5, 0.5, 0.5), (0.8, 0.2, 0.3), (0.2, 0.8, 0.7), (0., 0., 0.)]
FREQUENCIES = [80, 250, 1000, 3500, 7000]


def netlist(bass, mid, treble, directory):
    text = f"""Selected Model T passive network: reference component values
Vin in 0 dc 0 ac 1
Rslope in slope 56k
Ctreble in top 270p
Cbass slope bass 22n
Cmid slope middle 22n
Rbass bass midtop {max(1., bass * 1e6)}
Rmidtop midtop middle {max(1., (1. - mid) * 25000)}
Rmidbottom middle 0 {max(1., mid * 25000)}
Rtrebletop top out {max(1., (1. - treble) * 1e6)}
Rtreblebottom out bass {max(1., treble * 1e6)}
Rload out 0 1meg
.control
set numdgt=15
"""
    for f in FREQUENCIES:
        text += f"ac lin 1 {f} {f}\nwrdata {directory}/f{f}.txt v(out)\n"
    return text + "quit\n.endc\n.end\n"


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    rows = []
    for bass, mid, treble in CONTROLS:
        with tempfile.TemporaryDirectory(prefix="heavy-circuit-") as temp:
            directory = Path(temp)
            path = directory / "tone.cir"
            path.write_text(netlist(bass, mid, treble, directory))
            subprocess.run([sys.argv[1], "-b", str(path)], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            for f in FREQUENCIES:
                data = (directory / f"f{f}.txt").read_text().split()
                rows.append((bass, mid, treble, f, float(data[1]), float(data[2])))
    output = ROOT / "tests/fixtures/model_t_tone_ngspice.csv"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["bass", "mid", "treble", "hz", "real", "imag"])
        writer.writerows(rows)
    print(f"Generated {len(rows)} ngspice reference points: {output}")
    with tempfile.TemporaryDirectory(prefix="heavy-valve-") as temp:
        directory = Path(temp)
        path = directory / "valve.cir"
        path.write_text(f"""Independent Koren common-cathode stage
.options abstol=1e-12 reltol=1e-8
Vb supply 0 330
Vin source 0 dc 0 ac 1
Rg source grid 68k
Rp supply plate 100k
Rk cathode 0 680
Ck cathode 0 250u
Bval plate cathode I={{2/1060*pow(max(v(plate,cathode),1e-6)/600*ln(1+exp(600*(1/83.5+v(grid,cathode)/sqrt(300+v(plate,cathode)*v(plate,cathode))))),1.4)}}
.control
set numdgt=15
op
wrdata {directory}/op.txt v(plate) v(cathode)
ac lin 1 1000 1000
wrdata {directory}/ac.txt v(plate)
quit
.endc
.end
""")
        subprocess.run([sys.argv[1], "-b", str(path)], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        op = (directory / "op.txt").read_text().split()
        ac = (directory / "ac.txt").read_text().split()
        output = ROOT / "tests/fixtures/model_t_valve_ngspice.csv"
        with output.open("w", newline="") as stream:
            writer = csv.writer(stream, lineterminator="\n")
            writer.writerow(["plate_volts", "cathode_volts", "ac_hz", "gain_real", "gain_imag"])
            writer.writerow([float(op[1]), float(op[3]), 1000, float(ac[1]), float(ac[2])])
        print(f"Generated independent valve bias and AC reference: {output}")


if __name__ == "__main__":
    main()
