# This file is Copyright (c) 2018-2019 Florent Kermarrec <florent@enjoy-digital.fr>
# This file is Copyright (c) 2018-2019 David Shah <dave@ds0.me>
# This file is Copyright (c) 2018 William D. Jones <thor0505@comcast.net>
# License: BSD

import os
import subprocess
import sys

from migen.fhdl.structure import _Fragment

from litex.build.generic_platform import *
from litex.build import tools
from litex.build.lattice import common

# IO Constraints (.lpf) ----------------------------------------------------------------------------

def _format_constraint(c):
    if isinstance(c, Pins):
        return ("LOCATE COMP ", " SITE " + "\"" + c.identifiers[0] + "\"")
    elif isinstance(c, IOStandard):
        return ("IOBUF PORT ", " IO_TYPE=" + c.name)
    elif isinstance(c, Misc):
        return ("IOBUF PORT ", " " + c.misc)


def _format_lpf(signame, pin, others, resname):
    fmt_c = [_format_constraint(c) for c in ([Pins(pin)] + others)]
    lpf = []
    for pre, suf in fmt_c:
        lpf.append(pre + "\"" + signame + "\"" + suf + ";")
    return "\n".join(lpf)


def _build_lpf(named_sc, named_pc, build_name):
    lpf = []
    lpf.append("BLOCK RESETPATHS;")
    lpf.append("BLOCK ASYNCPATHS;")
    for sig, pins, others, resname in named_sc:
        if len(pins) > 1:
            for i, p in enumerate(pins):
                lpf.append(_format_lpf(sig + "[" + str(i) + "]", p, others, resname))
        else:
            lpf.append(_format_lpf(sig, pins[0], others, resname))
    if named_pc:
        lpf.append("\n\n".join(named_pc))
    tools.write_to_file(build_name + ".lpf", "\n".join(lpf))

# Yosys/Nextpnr Helpers/Templates ------------------------------------------------------------------

_yosys_template = [
    "verilog_defaults -push",
    "verilog_defaults -add -defer",
    "{read_files}",
    "verilog_defaults -pop",
    "attrmap -tocase keep -imap keep=\"true\" keep=1 -imap keep=\"false\" keep=0 -remove keep=0",
    "synth_ecp5 -abc9 {nwl} -json {build_name}.json -top {build_name}",
]

def _yosys_import_sources(platform):
    includes = ""
    reads = []
    for path in platform.verilog_include_paths:
        includes += " -I" + path
    for filename, language, library in platform.sources:
        reads.append("read_{}{} {}".format(
            language, includes, filename))
    return "\n".join(reads)

def _build_yosys(template, platform, nowidelut, build_name):
    ys = []
    for l in template:
        ys.append(l.format(
            build_name = build_name,
            nwl        = "-nowidelut" if nowidelut else "",
            read_files = _yosys_import_sources(platform)
        ))
    tools.write_to_file(build_name + ".ys", "\n".join(ys))

def nextpnr_ecp5_parse_device(device):
    device      = device.lower()
    family      = device.split("-")[0]
    size        = device.split("-")[1]
    speed_grade = device.split("-")[2][0]
    if speed_grade not in ["6", "7", "8"]:
       raise ValueError("Invalid speed grade {}".format(speed_grade))
    package     = device.split("-")[2][1:]
    if "256" in package:
        package = "CABGA256"
    elif "285" in package:
        package = "CSFBGA285"
    elif "381" in package:
        package = "CABGA381"
    elif "554" in package:
        package = "CABGA554"
    elif "756" in package:
        package = "CABGA756"
    else:
       raise ValueError("Invalid package {}".format(package))
    return (family, size, speed_grade, package)

nextpnr_ecp5_architectures = {
    "lfe5u-12f"   : "12k",
    "lfe5u-25f"   : "25k",
    "lfe5u-45f"   : "45k",
    "lfe5u-85f"   : "85k",
    "lfe5um-25f"  : "um-25k",
    "lfe5um-45f"  : "um-45k",
    "lfe5um-85f"  : "um-85k",
    "lfe5um5g-25f": "um5g-25k",
    "lfe5um5g-45f": "um5g-45k",
    "lfe5um5g-85f": "um5g-85k",
}

# Script -------------------------------------------------------------------------------------------

_build_template = [
    "yosys -l {build_name}.rpt {build_name}.ys",
    "nextpnr-ecp5 --json {build_name}.json --lpf {build_name}.lpf --textcfg {build_name}.config  \
    --{architecture} --package {package} --speed {speed_grade} {timefailarg} {ignoreloops} {seed}",
    "ecppack {build_name}.config --svf {build_name}.svf --bit {build_name}.bit"
]

def _build_script(source, build_template, build_name, architecture, package, speed_grade, timingstrict, ignoreloops, seed):
    if sys.platform in ("win32", "cygwin"):
        script_ext = ".bat"
        script_contents = "@echo off\nrem Autogenerated by LiteX / git: " + tools.get_litex_git_revision() + "\n\n"
        fail_stmt = " || exit /b"
    else:
        script_ext = ".sh"
        script_contents = "# Autogenerated by LiteX / git: " + tools.get_litex_git_revision() + "\nset -e\n"
        fail_stmt = ""

    for s in build_template:
        s_fail = s + "{fail_stmt}\n"  # Required so Windows scripts fail early.
        script_contents += s_fail.format(
            build_name      = build_name,
            architecture    = architecture,
            package         = package,
            speed_grade     = speed_grade,
            timefailarg     = "--timing-allow-fail" if not timingstrict else "",
            ignoreloops     = "--ignore-loops" if ignoreloops else "",
            fail_stmt       = fail_stmt,
            seed            = f"--seed {seed}" if seed is not None else "")

    script_file = "build_" + build_name + script_ext
    tools.write_to_file(script_file, script_contents, force_unix=False)

    return script_file

def _run_script(script):
    if sys.platform in ("win32", "cygwin"):
        shell = ["cmd", "/c"]
    else:
        shell = ["bash"]

    if subprocess.call(shell + [script]) != 0:
        raise OSError("Subprocess failed")

# LatticeTrellisToolchain --------------------------------------------------------------------------

class LatticeTrellisToolchain:
    attr_translate = {
        # FIXME: document
        "keep": ("keep", "true"),
        "no_retiming":      None,
        "async_reg":        None,
        "mr_ff":            None,
        "mr_false_path":    None,
        "ars_ff1":          None,
        "ars_ff2":          None,
        "ars_false_path":   None,
        "no_shreg_extract": None
    }

    special_overrides = common.lattice_ecp5_trellis_special_overrides

    def __init__(self):
        self.yosys_template   = _yosys_template
        self.build_template   = _build_template
        self.false_paths = set() # FIXME: use it

    def build(self, platform, fragment,
        build_dir      = "build",
        build_name     = "top",
        run            = True,
        nowidelut      = False,
        timingstrict   = False,
        ignoreloops    = False,
        seed           = None,
        **kwargs):

        # Create build directory
        os.makedirs(build_dir, exist_ok=True)
        cwd = os.getcwd()
        os.chdir(build_dir)

        # Finalize design
        if not isinstance(fragment, _Fragment):
            fragment = fragment.get_fragment()
        platform.finalize(fragment)

        # Generate verilog
        v_output = platform.get_verilog(fragment, name=build_name, **kwargs)
        named_sc, named_pc = platform.resolve_signals(v_output.ns)
        top_file = build_name + ".v"
        v_output.write(top_file)
        platform.add_source(top_file)

        # Generate design constraints file (.lpf)
        _build_lpf(named_sc, named_pc, build_name)

        # Generate Yosys script
        _build_yosys(self.yosys_template, platform, nowidelut, build_name)

        # Translate device to Nextpnr architecture/package/speed_grade
        (family, size, speed_grade, package) = nextpnr_ecp5_parse_device(platform.device)
        architecture = nextpnr_ecp5_architectures[(family + "-" + size)]

        # Generate build script
        script = _build_script(False, self.build_template, build_name, architecture, package,
            speed_grade, timingstrict, ignoreloops, seed)

        # Run
        if run:
            _run_script(script)

        os.chdir(cwd)

        return v_output.ns

    def add_period_constraint(self, platform, clk, period):
        platform.add_platform_command("""FREQUENCY PORT "{clk}" {freq} MHz;""".format(
            freq=str(float(1/period)*1000), clk="{clk}"), clk=clk)

    def add_false_path_constraint(self, platform, from_, to):
        from_.attr.add("keep")
        to.attr.add("keep")
        if (to, from_) not in self.false_paths:
            self.false_paths.add((from_, to))

def trellis_args(parser):
    parser.add_argument("--yosys-nowidelut", action="store_true",
                        help="pass '-nowidelut' to yosys synth_ecp5")
    parser.add_argument("--nextpnr-timingstrict", action="store_true",
                        help="fail if timing not met, i.e., do NOT pass '--timing-allow-fail' to nextpnr")
    parser.add_argument("--nextpnr-ignoreloops", action="store_true",
                        help="ignore combinational loops in timing analysis, i.e. pass '--ignore-loops' to nextpnr")

def trellis_argdict(args):
    return {
        "nowidelut":    args.yosys_nowidelut,
        "timingstrict": args.nextpnr_timingstrict,
        "ignoreloops":  args.nextpnr_ignoreloops,
    }
