#!/usr/bin/env python3
"""Collect macOS release evidence without converting missing evidence into PASS.

python3 tools/ValidateMacos.py --plan
python3 tools/ValidateMacos.py --execute --output /absolute/new/evidence-directory
"""
import argparse
import hashlib
import json
import math
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tarfile
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / "tools/macos-validation.json").read_text(encoding="utf-8"))


@dataclass
class Step:
    name: str
    gate: str
    command: list
    requires: list = field(default_factory=list)
    timeout: int = 1200
    validation: bool = False


def plan(output, baseline=None):
    builds = {c: ROOT / "out" / ("macos-validation-" + output.name) / c for c in ("debug", "release", "core", "profile")}
    builds["baseline"] = ROOT / "out" / ("macos-validation-" + output.name) / "baseline"
    steps = []
    def add(name, gate, command, requires=(), timeout=1200, validation=False):
        steps.append(Step(name, gate, list(map(str, command)), list(requires), timeout, validation))
    add("metadata", "G0", ["cmake", "-DEXPECTED_VERSION=" + POLICY["release"], "-P", "cmake/VerifyReleaseVersion.cmake"])
    for config in ("debug", "release", "core", "profile"):
        configure = ["cmake", "-S", ROOT, "-B", builds[config], "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + ("Debug" if config in ("debug", "core") else "Release"),
                     "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + POLICY["deploymentTarget"], "-DBUILD_TESTING=ON"]
        if config == "core":
            configure += ["-DMINVEX_BUILD_RENDERER=OFF", "-DMINVEX_BUILD_EXAMPLES=OFF", "-DMINVEX_BUILD_EDITOR=OFF"]
        else:
            configure += ["-DMINVEX_ENABLE_RENDER_SMOKE_TEST=ON", "-DMINVEX_BUILD_BENCHMARKS=ON", "-DMINVEX_ENABLE_FRAME_PROFILING=" + ("ON" if config == "profile" else "OFF")]
        gate = "DIAGNOSTIC" if config == "profile" else "G2" if config == "core" else "G1"
        add(config + "-configure", gate, configure)
        build = ["cmake", "--build", builds[config], "--parallel", "4"]
        if config == "profile": build += ["--target", "minvex_ui_frame_probe"]
        add(config + "-build", gate, build, [config + "-configure"])
        if config != "profile":
            add(config + "-tests", "G2" if config == "core" else "G1", ["ctest", "--test-dir", builds[config], "--parallel", "1", "--no-tests=error", "--output-on-failure", "--output-junit", output / (config + ".xml")], [config + "-build"])
        if config in ("debug", "release"):
            binaries = [builds[config] / "bin" / name for name in ("minvex_editor", "minvex_standalone", "minvex_embedded_host")]
            add(config + "-architecture", "G0", ["file"] + binaries, [config + "-build"])
            add(config + "-deployment", "G0", ["otool", "-l"] + binaries, [config + "-build"])
            add(config + "-gizmo", "G1", ["ctest", "--test-dir", builds[config], "-R", "^integration.editor$", "--repeat", "until-fail:5", "--no-tests=error", "--output-on-failure"], [config + "-build"])
            add(config + "-metal", "G3", ["ctest", "--test-dir", builds[config], "-R", "^(lighting.pixels|optimization.gpu|ui.pixels|native.lifecycle|integration.embedded)$", "--parallel", "1", "--no-tests=error", "--verbose"], [config + "-build"], validation=True)
            add(config + "-variants", "G3", [builds[config] / "minvex_lighting_pixels", "--shader-variants", output / (config + "-variants.json")], [config + "-build"], validation=True)
            add(config + "-sdk", "G9", ["cmake", "-DBUILD_DIR=" + str(builds[config]), "-DCONFIG=" + config.title(), "-DPACKAGE=ON", "-DRUN_DESKTOP=ON", "-DRUN_BENCHMARK=ON", "-P", "cmake/VerifySdk.cmake"], [config + "-build"])
    release = builds["release"]
    baseline_requires = []
    if baseline is None:
        baseline = builds["baseline"]
        source = baseline.parent / "baseline-source"
        add("baseline-archive", "G7", ["git", "archive", POLICY["baselineRevision"], "--output=" + str(output / "baseline.tar")])
        add("baseline-extract", "G7", [sys.executable, Path(__file__).resolve(), "--extract-baseline", output / "baseline.tar", source], ["baseline-archive"])
        add("baseline-configure", "G7", ["cmake", "-S", source, "-B", baseline, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + POLICY["deploymentTarget"], "-DBUILD_TESTING=ON", "-DMINVEX_BUILD_EDITOR=OFF", "-DMINVEX_BUILD_EXAMPLES=OFF"], ["baseline-extract"])
        add("baseline-build", "G7", ["cmake", "--build", baseline, "--target", "minvex_gpu_efficiency_tests", "--parallel", "4"], ["baseline-configure"])
        baseline_requires = ["baseline-build"]
    probe = release / "benchmarks/Minvex/minvex_ui_frame_probe"
    for mode in ("none", "static", "dynamic", "dense-static", "dense-dynamic"):
        for run in range(1, POLICY["performance"]["repetitions"] + 1):
            add("p01-" + mode + "-" + str(run), "G7", [probe, "cube", mode, POLICY["performance"]["frames"], output / ("p01-" + mode + "-" + str(run) + ".json")], ["release-build"])
    add("p01-continuous", "G7", [probe, "cube", "dense-dynamic", POLICY["performance"]["continuousFrames"], output / "p01-continuous.json"], ["release-build"])
    add("p01-trace", "DIAGNOSTIC", [builds["profile"] / "benchmarks/Minvex/minvex_ui_frame_probe", "cube", "dense-dynamic", "6000", output / "p01-trace.json"], ["profile-build"])
    for run in range(1, 4):
        add("p02-cubes-" + str(run), "G7", [release / "benchmarks/Minvex/minvex_benchmark", "--objects", "1000", "--warmup", "120", "--frames", "6000", "--output", output / ("p02-cubes-" + str(run) + ".json")], ["release-build"])
        add("p02-equipment-" + str(run), "G7", [release / "bin/minvex_equipment", "--exercise", "--quality", "low", "--no-vsync", "--target-fps", "0", "--frames", "6030", "--profile", output / ("p02-equipment-" + str(run) + ".json")], ["release-build"])
        add("p02-morph-" + str(run), "G7", [probe, ROOT / "tests/Fixtures/MorphRibbon.gltf", "none", "6000", output / ("p02-morph-" + str(run) + ".json"), "--animate"], ["release-build"])
        add("p03-lighting-" + str(run), "G7", [release / "minvex_lighting_pixels", output / ("p03-lighting-" + str(run) + ".json")], ["release-build"])
        for mode in ("near", "far-lod", "overdraw", "instancing"):
            add("p04-" + mode + "-" + str(run), "G7", [release / "minvex_gpu_efficiency_tests", mode, output / ("p04-" + mode + "-" + str(run) + ".json")], ["release-build"])
            add("baseline-" + mode + "-" + str(run), "G7", [baseline / "minvex_gpu_efficiency_tests", mode, output / ("baseline-" + mode + "-" + str(run) + ".json")], baseline_requires)
    add("g8-lifecycle", "G8", [release / "minvex_native_lifecycle", "--cycles", POLICY["stability"]["cycles"], "--soak-seconds", POLICY["stability"]["soakSeconds"], "--output", output / "g8-lifecycle.json"], ["release-build"], timeout=3600)
    return steps, builds


def capture(command):
    try:
        result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False, timeout=90)
        return {"command": command, "exitCode": result.returncode, "output": result.stdout}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"command": command, "exitCode": None, "output": str(error)}


def fingerprint():
    paths = subprocess.check_output(["git", "ls-files", "-co", "--exclude-standard", "-z"], cwd=ROOT).split(b"\0")
    result = {}
    for raw in paths:
        if not raw: continue
        name = os.fsdecode(raw)
        path = ROOT / name
        if path.is_file() and not name.lower().endswith(".md"):
            result[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def clean_source():
    changed = subprocess.check_output(["git", "diff", "--name-only", "HEAD", "-z"], cwd=ROOT)
    untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT)
    # Markdown remains a local draft under AGENTS.md; it is not candidate code.
    return not any(raw and not os.fsdecode(raw).lower().endswith(".md") for raw in (changed + b"\0" + untracked).split(b"\0"))


def stop_process(process):
    if process is None or process.poll() is not None: return
    posix = platform.system() != "Windows"
    if posix: os.killpg(process.pid, signal.SIGTERM)
    else: process.terminate()
    try: process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        if posix: os.killpg(process.pid, signal.SIGKILL)
        else: process.kill()
        process.wait()


def run_step(step, output, prior):
    if any(prior.get(name, {}).get("status") != "PASS" for name in step.requires):
        return {"status": "PENDING", "reason": "prerequisite failed or did not run", "command": step.command}
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    for name in list(env):
        if name.startswith(("MTL_DEBUG_LAYER", "MTL_SHADER_VALIDATION")) or name in ("MINVEX_FRAME_TRACE", "METAL_DEVICE_WRAPPER_TYPE"):
            env.pop(name, None)
    if step.validation:
        env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1", MTL_SHADER_VALIDATION_REPORT_TO_STDERR="1", MTL_SHADER_VALIDATION_ABORT_ON_FAULT="1")
    if step.name == "p01-trace": env["MINVEX_FRAME_TRACE"] = str(output / "frame-trace.csv")
    started = time.monotonic()
    observed = set()
    result = {"command": step.command, "validationRequested": step.validation}
    process = None
    try:
        if step.command[0] == "ctest":
            folder = step.command[step.command.index("--test-dir") + 1]
            inventory = capture(["ctest", "--test-dir", folder, "--show-only=json-v1"])
            required = {"integration.editor"} if step.name.endswith("-gizmo") else {"phase2.asset", "phase2.scene", "phase3.result"} if step.name == "core-tests" else {"lighting.pixels", "optimization.gpu", "ui.pixels", "native.lifecycle", "integration.embedded"}
            if step.name in ("debug-tests", "release-tests"): required.add("integration.editor")
            try: registered = {t["name"] for t in json.loads(inventory["output"])["tests"]}
            except (ValueError, KeyError, TypeError): registered = set()
            if inventory["exitCode"] != 0 or not required <= registered:
                return dict(result, status="FAIL", reason="Required tests absent: " + ", ".join(sorted(required - registered)))
        with (output / (step.name + ".log")).open("w", encoding="utf-8") as log:
            process = subprocess.Popen(step.command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=platform.system() != "Windows")
            result["pid"] = process.pid
            while process.poll() is None:
                elapsed = time.monotonic() - started
                if elapsed > step.timeout:
                    stop_process(process)
                    result.update(status="FAIL", reason="timeout", exitCode=process.returncode)
                    break
                observation = "late" if elapsed >= POLICY["stability"]["soakSeconds"] - 30 else "early" if elapsed >= 60 else None
                if step.name == "g8-lifecycle" and observation and observation not in observed:
                    observed.add(observation)
                    evidence = {"leaks": capture(["leaks", str(process.pid)]), "thermal": capture(["pmset", "-g", "therm"])}
                    evidence.update(elapsedSeconds=elapsed,phase=observation)
                    (output / ("g8-observation.json" if observation == "late" else "g8-observation-early.json")).write_text(json.dumps(evidence, indent=2), encoding="utf-8")
                time.sleep(.5)
            else:
                result.update(status="PASS" if process.returncode == 0 else "FAIL", exitCode=process.returncode)
        if result.get("status") == "PASS" and step.name.endswith("-architecture"):
            content = (output / (step.name + ".log")).read_text(encoding="utf-8", errors="replace")
            if len(re.findall(r"Mach-O.*arm64", content)) != 3 or "x86_64" in content:
                result.update(status="FAIL", reason="Expected three native arm64 executables")
        if result.get("status") == "PASS" and step.name.endswith("-deployment"):
            content = (output / (step.name + ".log")).read_text(encoding="utf-8", errors="replace")
            versions = re.findall(r"\bminos\s+([\d.]+)", content)
            def version(value):
                numbers = tuple(map(int, value.split(".")))
                return numbers + (0,) * (3 - len(numbers))
            if len(versions) != 3 or any(version(v) != version(POLICY["deploymentTarget"]) for v in versions):
                result.update(status="FAIL", reason="Executable deployment target differs from policy")
    except OSError as error:
        stop_process(process)
        result.update(status="FAIL", reason=str(error))
    except KeyboardInterrupt:
        stop_process(process)
        raise
    result["elapsedSeconds"] = time.monotonic() - started
    return result


def read_json(path):
    try: return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError): return None


def extract_baseline(archive, destination):
    destination = destination.resolve()
    if destination.exists(): raise ValueError("Baseline destination must be new")
    with tarfile.open(archive) as source:
        members = source.getmembers()
        for member in members:
            relative = Path(member.name)
            target = (destination / relative).resolve()
            if relative.is_absolute() or ".." in relative.parts or destination not in target.parents or not (member.isfile() or member.isdir()):
                raise ValueError("Unsafe baseline archive member: " + member.name)
        destination.mkdir(parents=True)
        for member in members:
            target = destination / member.name
            if member.isdir(): target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as stream, target.open("wb") as output:
                    shutil.copyfileobj(stream, output)


def warning_summary(output, results):
    patterns = {
        "unused-uniform-binding": r"unused.*(?:vertex|fragment).*bind|(?:vertex|fragment).*bind.*(?:unused|not used)",
        "redundant-state": r"redundant.*(?:stencil|depthbias|cull|blend|winding|depthstencil)",
        "redundant-buffer": r"redundant.*buffer",
        "unused-viewport-scissor": r"(?:viewport|scissor).*(?:unused|not used)",
        "validation-dontcare-clear": r"dontcare.*(?:clear|magenta)",
        "missing-field-initializers": r"Wmissing-field-initializers",
        "duplicate-libraries": r"ignoring duplicate librar",
        "unused-parameter": r"Wunused-parameter",
        "missing-braces": r"Wmissing-braces",
        "range-loop-copy": r"Wrange-loop-construct",
        "enum-conversion": r"Wdeprecated.*enum.*conversion",
    }
    summary = {"apiValidationSeen": False, "gpuValidationSeen": False, "categories": {k: 0 for k in patterns}, "unclassifiedCount": 0, "unclassifiedExamples": [], "requiresSourceAttributionReview": True}
    for name, result in results.items():
        path = output / (name + ".log")
        if not path.is_file(): continue
        with path.open(encoding="utf-8", errors="replace") as log:
            for line in log:
                if result.get("validationRequested"):
                    summary["apiValidationSeen"] |= bool(re.search(r"Metal API Validation.*Enabled", line, re.I))
                    summary["gpuValidationSeen"] |= bool(re.search(r"Metal (?:GPU|Shader) Validation.*Enabled", line, re.I))
                if not re.search(r"warning|unused.*bind|redundant|dontcare.*clear", line, re.I): continue
                matches = [k for k, regex in patterns.items() if re.search(regex, line, re.I)]
                for key in matches: summary["categories"][key] += 1
                if not matches:
                    summary["unclassifiedCount"] += 1
                    if len(summary["unclassifiedExamples"]) < 40: summary["unclassifiedExamples"].append({"log": path.name, "line": line.strip()})
    return summary


def assess(output, results, minimum_environment, clean_source, unchanged, manual, external_baseline=False):
    def manual_status(names):
        rows = [manual.get(name, {}) for name in names]
        if any(row.get("status") == "FAIL" for row in rows): return "FAIL"
        return "PASS" if all(row.get("status") == "PASS" and row.get("evidence") and row.get("assessor") and row.get("date") for row in rows) else "PENDING"
    def positive(value):
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value > 0
    gates = {}
    expected, _ = plan(output, Path(".") if external_baseline else None)
    for gate in ("G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8", "G9", "G10"):
        required = {s.name for s in expected if s.gate == gate}
        statuses = [results.get(name, {}).get("status", "PENDING") for name in required]
        statuses += [v["status"] for name, v in results.items() if v.get("gate") == gate and name not in required]
        gates[gate] = "FAIL" if "FAIL" in statuses else "PASS" if statuses and all(s == "PASS" for s in statuses) else "PENDING"
    if gates["G0"] != "FAIL" and (not minimum_environment or not clean_source): gates["G0"] = "PENDING"
    if unchanged is False: gates["G0"] = "FAIL"
    elif unchanged is None and gates["G0"] != "FAIL": gates["G0"] = "PENDING"
    if not minimum_environment and gates["G9"] == "PASS": gates["G9"] = "PENDING"
    variants = [read_json(output / (c + "-variants.json")) for c in ("debug", "release")]
    if any(v and v.get("failures") for v in variants): gates["G3"] = "FAIL"
    elif gates["G3"] != "FAIL" and not all(v and v.get("caseCount") == 160 and len(v.get("cases", [])) == 160 and all(c.get("status") == "PASS" for c in v["cases"]) for v in variants): gates["G3"] = "PENDING"
    # Validation must be observed in logs, and warning attribution is a review.
    review = manual_status(["G3-validation-review"])
    if review == "FAIL": gates["G3"] = "FAIL"
    elif gates["G3"] == "PASS":
        warnings = warning_summary(output, results)
        gates["G3"] = review if warnings["apiValidationSeen"] and warnings["gpuValidationSeen"] else "PENDING"
    pixel_steps = [results.get(c + "-metal", {}).get("status") for c in ("debug", "release")]
    gates["G4"] = "FAIL" if "FAIL" in pixel_steps else "PASS" if all(s == "PASS" for s in pixel_steps) else "PENDING"
    gates["G5"] = manual_status(["U03", "U04"])
    gates["G6"] = manual_status(["G6-debug", "G6-release"])
    if gates["G5"] == "PASS" and not all(s == "PASS" for s in pixel_steps): gates["G5"] = "PENDING"
    if gates["G6"] == "PASS" and not all(results.get(c + "-tests", {}).get("status") == "PASS" for c in ("debug", "release")): gates["G6"] = "PENDING"
    for name in [n for n in results if n.startswith("p01-") and n != "p01-trace"]:
        if results[name]["status"] != "PASS": continue
        report = read_json(output / (name + ".json"))
        if not report or not report.get("frames"): gates["G7"] = "FAIL"; continue
        expected = POLICY["performance"]["continuousFrames"] if name == "p01-continuous" else POLICY["performance"]["frames"]
        maximum = report.get("maxMs")
        if report["frames"] != expected or report.get("over33ms", 1) != 0 or not positive(maximum) or maximum > POLICY["performance"]["uiMaximumMs"]: gates["G7"] = "FAIL"
    for name in [n for n in results if n.startswith("p02-")]:
        if results[name]["status"] != "PASS": continue
        report = read_json(output / (name + ".json"))
        mean = (report or {}).get("meanMs", (report or {}).get("cadenceMeanMs", (report or {}).get("meanFrameMs")))
        p95 = (report or {}).get("p95Ms", (report or {}).get("cadenceP95Ms", (report or {}).get("p95FrameMs")))
        frames = (report or {}).get("frames", (report or {}).get("measuredFrames", (report or {}).get("samples")))
        shape = report and report.get("width") == 1280 and report.get("height") == 720
        mode = not name.startswith("p02-morph-") or report and report.get("animation") is True
        low = not name.startswith("p02-equipment-") or report and report.get("quality") == "low" and report.get("vsync") is False and report.get("targetFps") == 0
        if frames != POLICY["performance"]["frames"] or not shape or not mode or not low or not positive(mean) or not positive(p95) or 1000 / mean < 30 or p95 > POLICY["performance"]["p95LimitMs"]: gates["G7"] = "FAIL"
    comparisons = {}
    for mode in ("near", "far-lod", "overdraw", "instancing"):
        target = [read_json(output / ("p04-" + mode + "-" + str(i) + ".json")) for i in (1, 2, 3)]
        baseline = [read_json(output / ("baseline-" + mode + "-" + str(i) + ".json")) for i in (1, 2, 3)]
        if not all(target + baseline): continue
        comparable = all(a.get(k) == b.get(k) for a, b in zip(target, baseline) for k in ("format", "version", "mode", "width", "height", "frames", "objects"))
        if not comparable or not all(positive(v.get("completedMeanMs")) for v in target + baseline): comparisons[mode] = {"status": "PENDING", "reason": "baseline contract differs or timing is invalid"}; continue
        ratio = statistics.median(v["completedMeanMs"] for v in target) / statistics.median(v["completedMeanMs"] for v in baseline)
        comparisons[mode] = {"status": "PASS" if ratio <= POLICY["performance"]["gpuRegressionRatio"] else "FAIL", "ratio": ratio}
    if any(v["status"] == "FAIL" for v in comparisons.values()): gates["G7"] = "FAIL"
    elif gates["G7"] != "FAIL" and (len(comparisons) != 4 or any(v["status"] != "PASS" for v in comparisons.values()) or manual_status(["P01-foreground", "P05"]) != "PASS" or not minimum_environment): gates["G7"] = "PENDING"
    lifecycle = read_json(output / "g8-lifecycle.json")
    observation = read_json(output / "g8-observation.json")
    if gates["G8"] != "FAIL":
        complete = lifecycle and lifecycle.get("cycles", 0) >= 100 and lifecycle.get("soakSeconds", 0) >= 1800 and lifecycle.get("elapsedSeconds", 0) >= 1800 and not lifecycle.get("failures")
        rss = [v.get("rssBytes") for v in (lifecycle or {}).get("samples", []) if v.get("phase") == "soak"]
        cycles_rss = [v.get("rssBytes") for v in (lifecycle or {}).get("samples", []) if v.get("phase") == "after-shutdown"]
        soak_samples = [v for v in (lifecycle or {}).get("samples", []) if v.get("phase") == "soak"]
        known_rss = len(rss) >= int(POLICY["stability"]["soakSeconds"] * .8) and all(positive(v) for v in rss)
        if known_rss: known_rss = soak_samples[-1].get("seconds", 0) - soak_samples[0].get("seconds", 0) >= POLICY["stability"]["soakSeconds"] - 5
        known_cycles = len(cycles_rss) >= 100 and all(positive(v) for v in cycles_rss)
        excessive = any(statistics.median(values[-10:]) - statistics.median(values[:10]) > POLICY["stability"]["rssGrowthAllowanceBytes"] for values, known in ((rss, known_rss), (cycles_rss, known_cycles)) if known)
        if excessive: gates["G8"] = "FAIL"
        elif not complete or not known_rss or not known_cycles or not observation or observation.get("phase") != "late" or observation.get("elapsedSeconds", 0) < POLICY["stability"]["soakSeconds"] - 60 or observation.get("leaks", {}).get("exitCode") != 0 or not re.search(r"\b0 leaks for\b", observation.get("leaks", {}).get("output", "")) or manual_status(["G8-thermal"]) != "PASS": gates["G8"] = "PENDING"
    gates["G10"] = "N/A"
    review = manual_status(["release-review"])
    conclusion = "FAIL" if "FAIL" in gates.values() or review == "FAIL" else "PENDING" if "PENDING" in gates.values() or review != "PASS" else "PASS"
    return {"gates": gates, "comparisons": comparisons, "releaseReview": review, "conclusion": conclusion, "auxiliaryReason": POLICY["auxiliaryGate"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--assess", type=Path, help="reassess collected evidence after completing manual-results.json")
    parser.add_argument("--extract-baseline", nargs=2, type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, default=ROOT / "out" / ("macos-evidence-" + time.strftime("%Y%m%d-%H%M%S")))
    parser.add_argument("--baseline-dir", type=Path, help="matching baseline GPU probe executable directory on this same Mac")
    args = parser.parse_args(argv)
    if args.extract_baseline:
        extract_baseline(*args.extract_baseline); return 0
    if args.assess:
        output = args.assess.resolve()
        environment = read_json(output / "environment.json")
        results = read_json(output / "exit-codes.json")
        if not environment or not results: parser.error("Collected environment.json and exit-codes.json are required.")
        minimum = environment.get("minimumEnvironment", False)
        unchanged = None if "sourceUnchanged" not in environment else environment["sourceUnchanged"] and environment.get("policy") == POLICY and environment.get("sourceHashes") == fingerprint()
        report = assess(output, results, minimum, environment.get("sourceClean", False), unchanged, read_json(output / "manual-results.json") or {}, environment.get("baseline", {}).get("external", False))
        report.update(assessor="Automated reassessment of collected evidence and named manual reviews",recordedAtUtc=datetime.now(timezone.utc).isoformat())
        (output / "warning-summary.json").write_text(json.dumps(warning_summary(output, results), indent=2), encoding="utf-8")
        (output / "approval.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report, indent=2)); return 0 if report["conclusion"] == "PASS" else 1
    output = args.output.resolve()
    steps, builds = plan(output, args.baseline_dir.resolve() if args.baseline_dir else None)
    if args.plan or not args.execute:
        print(json.dumps({"policy": POLICY, "steps": [asdict(s) for s in steps]}, indent=2)); return 0
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("Execution requires a native Apple Silicon macOS desktop. --plan works on any platform.")
    if output.exists() or any(p.exists() for p in builds.values()): parser.error("Use new output/build directories; previous evidence must not be overwritten.")
    missing = [name for name in ("git", "cmake", "ninja", "xcrun", "leaks", "file", "otool") if not shutil.which(name)]
    if missing: parser.error("Missing required tools: " + ", ".join(missing))
    output.mkdir(parents=True)
    source = fingerprint()
    environment = {name: capture(command) for name, command in {
        "version": ["sw_vers"], "model": ["sysctl", "-n", "hw.model"], "ram": ["sysctl", "-n", "hw.memsize"], "rosetta": ["sysctl", "-in", "sysctl.proc_translated"], "power": ["pmset", "-g", "batt"], "powerSettings": ["pmset", "-g", "custom"], "display": ["system_profiler", "SPDisplaysDataType", "-json"], "thermal": ["pmset", "-g", "therm"], "xcode": ["xcodebuild", "-version"], "clang": ["xcrun", "clang", "--version"], "cmake": ["cmake", "--version"], "ninja": ["ninja", "--version"], "git": ["git", "rev-parse", "HEAD"], "changes": ["git", "status", "--porcelain"]}.items()}
    environment.update(architecture=platform.machine(),osVersion=platform.mac_ver()[0],sourceHashes=source,sourceClean=clean_source(),policy=POLICY)
    baseline_directory=args.baseline_dir.resolve() if args.baseline_dir else builds["baseline"]
    environment["baseline"]={"directory":str(baseline_directory),"revision":POLICY["baselineRevision"] if not args.baseline_dir else "user-supplied; review provenance","external":bool(args.baseline_dir)}
    (output / "environment.json").write_text(json.dumps(environment, indent=2), encoding="utf-8")
    (output / "source.patch").write_bytes(subprocess.check_output(["git", "diff", "--binary", "HEAD"], cwd=ROOT))
    manual = {name: {"status": "PENDING", "evidence": "", "assessor": "", "date": "", "instruction": text} for name, text in POLICY["manualChecks"].items()}
    (output / "manual-results.json").write_text(json.dumps(manual, indent=2), encoding="utf-8")
    results = {}
    for step in steps:
        print(step.name, flush=True)
        results[step.name] = dict(run_step(step, output, results), gate=step.gate)
        (output / "exit-codes.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    os_version = environment["osVersion"].split(".")
    minimum = environment["model"]["output"].strip() == POLICY["minimumMachine"]["model"] and environment["ram"]["output"].strip() == str(POLICY["minimumMachine"]["memoryBytes"]) and os_version[:2] == [str(POLICY["minimumMachine"]["osMajor"]), str(POLICY["minimumMachine"]["osMinor"])] and environment["rosetta"]["output"].strip() != "1"
    environment.update(minimumEnvironment=minimum,sourceUnchanged=source == fingerprint())
    binary=baseline_directory/"minvex_gpu_efficiency_tests"
    environment["baseline"]["probeSha256"]=hashlib.sha256(binary.read_bytes()).hexdigest() if binary.is_file() else None
    (output / "environment.json").write_text(json.dumps(environment, indent=2), encoding="utf-8")
    report = assess(output, results, minimum, environment["sourceClean"], environment["sourceUnchanged"], read_json(output / "manual-results.json") or {}, bool(args.baseline_dir))
    (output / "warning-summary.json").write_text(json.dumps(warning_summary(output, results), indent=2), encoding="utf-8")
    report.update(assessor="ValidateMacos automated evidence and named manual reviews",recordedAtUtc=datetime.now(timezone.utc).isoformat(),minimumEnvironment=minimum)
    (output / "approval.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["conclusion"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
