#!/usr/bin/env python3
"""Shared Windows/macOS test runner. Use the three scripts in tests/<platform>."""
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
POLICY = json.loads((ROOT / "tests/Acceptance.json").read_text(encoding="utf-8"))


@dataclass
class Step:
    name: str
    gate: str
    command: list
    requires: list = field(default_factory=list)
    timeout: int = 1200
    validation: bool = False


def plan(output, baseline=None, system="Darwin", suite="all", trace=False):
    mac = system == "Darwin"
    prefix = "macos" if mac else "windows"
    suffix = "" if mac else ".exe"
    builds = {c: ROOT / "out/build" / (prefix + "-" + c) for c in ("debug", "release", "core", "profile")}
    builds["baseline"] = output / "baseline-build"
    steps = []
    def add(name, gate, command, requires=(), timeout=1200, validation=False):
        steps.append(Step(name, gate, list(map(str, command)), list(requires), timeout, validation))
    add("metadata", "G0", ["cmake", "-DEXPECTED_VERSION=" + POLICY["release"], "-P", "cmake/VerifyReleaseVersion.cmake"])
    for config in ("debug", "release", "core") + (("profile",) if trace else ()):
        configure = ["cmake", "-S", ROOT, "-B", builds[config], "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + ("Debug" if config in ("debug", "core") else "Release"),
                     "-DBUILD_TESTING=ON"]
        if mac: configure += ["-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + POLICY["deploymentTarget"]]
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
            add(config + "-tests", "G2" if config == "core" else "G1", ["ctest", "--test-dir", builds[config], "-LE", "desktop", "--parallel", "4", "--no-tests=error", "--output-on-failure", "--output-junit", output / (config + ".xml")], [config + "-build"])
        if config in ("debug", "release"):
            binaries = [builds[config] / "bin" / (name + suffix) for name in ("minvex_editor", "minvex_standalone", "minvex_embedded_host")]
            add(config + "-architecture", "G0", (["file"] if mac else [sys.executable, Path(__file__).resolve(), "--inspect-pe"]) + binaries, [config + "-build"])
            if mac: add(config + "-deployment", "G0", ["otool", "-l"] + binaries, [config + "-build"])
            add(config + "-metal", "G3", ["ctest", "--test-dir", builds[config], "-L", "desktop", "--parallel", "1", "--no-tests=error", "--verbose", "--output-junit", output / (config + "-desktop.xml")], [config + "-build"], validation=mac)
            add(config + "-variants", "G3", [builds[config] / ("minvex_lighting_pixels" + suffix), "--shader-variants", output / (config + "-variants.json")], [config + "-build"], validation=mac)
            add(config + "-sdk", "G9", ["cmake", "-DBUILD_DIR=" + str(builds[config]), "-DCONFIG=" + config.title(), "-DPACKAGE=ON", "-DRUN_DESKTOP=ON", "-DRUN_BENCHMARK=ON", "-P", "cmake/VerifySdk.cmake"], [config + "-build"])
    release = builds["release"]
    baseline_requires = []
    if baseline is None:
        baseline = builds["baseline"]
        source = output / "baseline-source"
        add("baseline-archive", "G7", ["git", "archive", POLICY["baselineRevision"], "--output=" + str(output / "baseline.tar")])
        add("baseline-extract", "G7", [sys.executable, Path(__file__).resolve(), "--extract-baseline", output / "baseline.tar", source], ["baseline-archive"])
        add("baseline-configure", "G7", ["cmake", "-S", source, "-B", baseline, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON", "-DMINVEX_BUILD_EDITOR=OFF", "-DMINVEX_BUILD_EXAMPLES=OFF"] + (["-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + POLICY["deploymentTarget"]] if mac else []), ["baseline-extract"])
        add("baseline-build", "G7", ["cmake", "--build", baseline, "--target", "minvex_gpu_efficiency_tests", "--parallel", "4"], ["baseline-configure"])
        baseline_requires = ["baseline-build"]
    probe = release / ("benchmarks/Minvex/minvex_ui_frame_probe" + suffix)
    for mode in ("none", "static", "dynamic", "dense-static", "dense-dynamic"):
        for run in range(1, POLICY["performance"]["repetitions"] + 1):
            add("p01-" + mode + "-" + str(run), "G7", [probe, "cube", mode, POLICY["performance"]["frames"], output / ("p01-" + mode + "-" + str(run) + ".json")], ["release-build"])
    add("p01-continuous", "G7", [probe, "cube", "dense-dynamic", POLICY["performance"]["continuousFrames"], output / "p01-continuous.json"], ["release-build"])
    if trace: add("p01-trace", "DIAGNOSTIC", [builds["profile"] / ("benchmarks/Minvex/minvex_ui_frame_probe" + suffix), "cube", "dense-dynamic", "6000", output / "p01-trace.json"], ["profile-build"])
    for run in range(1, 4):
        add("p02-cubes-" + str(run), "G7", [release / ("benchmarks/Minvex/minvex_benchmark" + suffix), "--objects", "1000", "--warmup", "120", "--frames", "6000", "--output", output / ("p02-cubes-" + str(run) + ".json")], ["release-build"])
        add("p02-equipment-" + str(run), "G7", [release / ("bin/minvex_equipment" + suffix), "--exercise", "--quality", "low", "--no-vsync", "--target-fps", "0", "--frames", "6030", "--profile", output / ("p02-equipment-" + str(run) + ".json")], ["release-build"])
        add("p02-morph-" + str(run), "G7", [probe, ROOT / "tests/Fixtures/MorphRibbon.gltf", "none", "6000", output / ("p02-morph-" + str(run) + ".json"), "--animate"], ["release-build"])
        add("p03-lighting-" + str(run), "G7", [release / ("minvex_lighting_pixels" + suffix), output / ("p03-lighting-" + str(run) + ".json")], ["release-build"])
        for mode in ("near", "far-lod", "overdraw", "instancing"):
            add("p04-" + mode + "-" + str(run), "G7", [release / ("minvex_gpu_efficiency_tests" + suffix), mode, output / ("p04-" + mode + "-" + str(run) + ".json")], ["release-build"])
            add("baseline-" + mode + "-" + str(run), "G7", [baseline / ("minvex_gpu_efficiency_tests" + suffix), mode, output / ("baseline-" + mode + "-" + str(run) + ".json")], baseline_requires)
    add("g8-lifecycle", "G8", [release / ("minvex_native_lifecycle" + suffix), "--cycles", POLICY["stability"]["cycles"], "--soak-seconds", POLICY["stability"]["soakSeconds"], "--output", output / "g8-lifecycle.json"], ["release-build"], timeout=3600)
    if suite != "all":
        common = {c + s for c in ("debug", "release") for s in ("-configure", "-build")}
        def included(step):
            if step.name in common: return True
            if suite == "contracts": return step.name == "metadata" or step.name.startswith("core-") or step.name in ("debug-tests", "release-tests")
            if suite == "graphics": return step.name.endswith(("-architecture", "-deployment", "-metal", "-variants"))
            return step.name == "metadata" or step.name.endswith("-sdk") or step.gate in ("G7", "G8", "DIAGNOSTIC")
        steps = [step for step in steps if included(step)]
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
            selection = ["-L", "desktop"] if "-L" in step.command else ["-LE", "desktop"]
            inventory = capture(["ctest", "--test-dir", folder, "--show-only=json-v1"] + selection)
            required = {"phase2.asset", "phase2.scene", "phase3.result"} if step.name.endswith("-tests") else {"lighting.pixels", "optimization.gpu", "ui.pixels", "native.lifecycle", "integration.embedded", "integration.editor", "ui.editor.advanced"}
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
                if platform.system() == "Darwin" and step.name == "g8-lifecycle" and observation and observation not in observed:
                    observed.add(observation)
                    evidence = {"leaks": capture(["leaks", str(process.pid)]), "thermal": capture(["pmset", "-g", "therm"])}
                    evidence.update(elapsedSeconds=elapsed,phase=observation)
                    (output / ("g8-observation.json" if observation == "late" else "g8-observation-early.json")).write_text(json.dumps(evidence, indent=2), encoding="utf-8")
                time.sleep(.5)
            else:
                result.update(status="PASS" if process.returncode == 0 else "FAIL", exitCode=process.returncode)
        if result.get("status") == "PASS" and step.name.endswith("-architecture") and step.command[0] == "file":
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


def lock_builds():
    """OS lock is released even if a test process is interrupted or crashes."""
    directory = ROOT / "out"
    directory.mkdir(exist_ok=True)
    handle = (directory / ".minvex-tests.lock").open("a+b")
    if not handle.tell(): handle.write(b"0"); handle.flush()
    handle.seek(0)
    try:
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        handle.close()
        raise RuntimeError("Another Minvex test suite is using the shared build directories.")
    return handle


def extract_baseline(archive, destination):
    destination = destination.resolve()
    with tarfile.open(archive) as source:
        members = source.getmembers()
        for member in members:
            relative = Path(member.name)
            target = (destination / relative).resolve()
            if relative.is_absolute() or ".." in relative.parts or destination not in target.parents or not (member.isfile() or member.isdir()):
                raise ValueError("Unsafe baseline archive member: " + member.name)
        if destination.exists():
            expected = {m.name.rstrip("/") for m in members if m.isfile()}
            actual = {p.relative_to(destination).as_posix() for p in destination.rglob("*") if p.is_file()}
            if actual != expected: raise ValueError("Existing baseline source differs from archive")
            for member in members:
                if member.isfile() and (destination / member.name).read_bytes() != source.extractfile(member).read():
                    raise ValueError("Existing baseline file differs: " + member.name)
            return
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


def assess(output, results, minimum_environment, clean_source, unchanged, manual, external_baseline=False, system="Darwin"):
    def manual_status(names):
        rows = [manual.get(name, {}) for name in names]
        if any(row.get("status") == "FAIL" for row in rows): return "FAIL"
        return "PASS" if all(row.get("status") == "PASS" and row.get("evidence") and row.get("assessor") and row.get("date") for row in rows) else "PENDING"
    def positive(value):
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value > 0
    gates = {}
    expected, _ = plan(output, Path(".") if external_baseline else None, system)
    for gate in ("G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8", "G9", "G10"):
        required = {s.name for s in expected if s.gate == gate}
        statuses = [results.get(name, {}).get("status", "PENDING") for name in required]
        statuses += [v["status"] for name, v in results.items() if v.get("gate") == gate and name not in required]
        gates[gate] = "FAIL" if "FAIL" in statuses else "PASS" if statuses and all(s == "PASS" for s in statuses) else "PENDING"
    if gates["G0"] != "FAIL" and (not clean_source or system == "Darwin" and not minimum_environment): gates["G0"] = "PENDING"
    if unchanged is False: gates["G0"] = "FAIL"
    elif unchanged is None and gates["G0"] != "FAIL": gates["G0"] = "PENDING"
    if system == "Darwin" and not minimum_environment and gates["G9"] == "PASS": gates["G9"] = "PENDING"
    variants = [read_json(output / (c + "-variants.json")) for c in ("debug", "release")]
    if any(v and v.get("failures") for v in variants): gates["G3"] = "FAIL"
    elif gates["G3"] != "FAIL" and not all(v and v.get("caseCount") == 160 and len(v.get("cases", [])) == 160 and all(c.get("status") == "PASS" for c in v["cases"]) for v in variants): gates["G3"] = "PENDING"
    # Validation must be observed in logs, and warning attribution is a review.
    review = manual_status(["G3-validation-review"])
    if review == "FAIL": gates["G3"] = "FAIL"
    elif gates["G3"] == "PASS":
        warnings = warning_summary(output, results)
        gates["G3"] = review if system != "Darwin" or warnings["apiValidationSeen"] and warnings["gpuValidationSeen"] else "PENDING"
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
    elif gates["G7"] != "FAIL" and (len(comparisons) != 4 or any(v["status"] != "PASS" for v in comparisons.values()) or manual_status(["P01-foreground", "P05"]) != "PASS" or system == "Darwin" and not minimum_environment): gates["G7"] = "PENDING"
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
        elif not complete or not known_rss or not known_cycles or manual_status(["G8-thermal"]) != "PASS": gates["G8"] = "PENDING"
        elif system == "Darwin" and (not observation or observation.get("phase") != "late" or observation.get("elapsedSeconds", 0) < POLICY["stability"]["soakSeconds"] - 60 or observation.get("leaks", {}).get("exitCode") != 0 or not re.search(r"\b0 leaks for\b", observation.get("leaks", {}).get("output", ""))): gates["G8"] = "PENDING"
    gates["G10"] = "N/A"
    review = manual_status(["release-review"])
    conclusion = "FAIL" if "FAIL" in gates.values() or review == "FAIL" else "PENDING" if "PENDING" in gates.values() or review != "PASS" else "PASS"
    return {"gates": gates, "comparisons": comparisons, "releaseReview": review, "conclusion": conclusion, "auxiliaryReason": POLICY["auxiliaryGate"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--suite", choices=("contracts", "graphics", "release", "all"), default="all")
    parser.add_argument("--platform", choices=("windows", "macos"), help="target platform (also checked before execution)")
    parser.add_argument("--trace", action="store_true", help="add a separate instrumented build; never replace normal performance evidence")
    parser.add_argument("--assess", type=Path, help="reassess collected evidence after completing manual-results.json")
    parser.add_argument("--extract-baseline", nargs=2, type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--inspect-pe", nargs="+", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, help="shared evidence directory for all three suites; defaults to a source-specific directory")
    parser.add_argument("--baseline-dir", type=Path, help="matching baseline GPU probe executable directory on this same Mac")
    args = parser.parse_args(argv)
    if args.extract_baseline:
        extract_baseline(*args.extract_baseline); return 0
    if args.inspect_pe:
        for path in args.inspect_pe:
            data = path.read_bytes()
            offset = int.from_bytes(data[60:64], "little")
            if data[:2] != b"MZ" or data[offset:offset + 6] != b"PE\0\0\x64\x86": parser.error("Expected Windows x64 PE: " + str(path))
            print(str(path) + ": PE x64")
        return 0
    if args.assess:
        output = args.assess.resolve()
        environment = read_json(output / "environment.json")
        results = read_json(output / "exit-codes.json")
        if not environment or not results: parser.error("Collected environment.json and exit-codes.json are required.")
        minimum = environment.get("minimumEnvironment", False)
        unchanged = None if "sourceUnchanged" not in environment else environment["sourceUnchanged"] and environment.get("policy") == POLICY and environment.get("sourceHashes") == fingerprint()
        report = assess(output, results, minimum, environment.get("sourceClean", False), unchanged, read_json(output / "manual-results.json") or {}, environment.get("baseline", {}).get("external", False), environment.get("system", "Darwin"))
        report.update(assessor="Automated reassessment of collected evidence and named manual reviews",recordedAtUtc=datetime.now(timezone.utc).isoformat())
        (output / "warning-summary.json").write_text(json.dumps(warning_summary(output, results), indent=2), encoding="utf-8")
        (output / "approval.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report, indent=2)); return 0 if report["conclusion"] == "PASS" else 1
    system = {"windows": "Windows", "macos": "Darwin"}.get(args.platform, platform.system())
    if system not in ("Windows", "Darwin"): parser.error("Select --platform windows or macos to inspect a plan on another host.")
    source = fingerprint()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip()
    source_id = hashlib.sha256(json.dumps([head, source], sort_keys=True).encode()).hexdigest()[:16]
    platform_name = "macos" if system == "Darwin" else "windows"
    output = (args.output or ROOT / "out/test-results" / platform_name / source_id).resolve()
    steps, builds = plan(output, args.baseline_dir.resolve() if args.baseline_dir else None, system, args.suite, args.trace)
    if args.plan or not args.execute:
        print(json.dumps({"platform": platform_name, "suite": args.suite, "output": str(output), "policy": POLICY, "steps": [asdict(s) for s in steps]}, indent=2)); return 0
    if platform.system() != system or platform.machine().lower() not in (("arm64",) if system == "Darwin" else ("amd64", "x86_64")):
        parser.error("Execution requires native Windows x64 or Apple Silicon macOS; --plan works on either host.")
    required_tools = ("git", "cmake", "ctest", "ninja") + (("xcrun", "leaks", "file", "otool") if system == "Darwin" else ("cl",))
    missing = [name for name in required_tools if not shutil.which(name)]
    if missing: parser.error("Missing required tools: " + ", ".join(missing))
    try: build_lock = lock_builds()
    except RuntimeError as error: parser.error(str(error))
    output.mkdir(parents=True, exist_ok=True)
    old_environment = read_json(output / "environment.json")
    if old_environment and (old_environment.get("sourceHashes") != source or old_environment.get("system") != system or old_environment.get("head") != head or old_environment.get("policy") != POLICY):
        parser.error("Evidence belongs to different source/platform/policy. Choose a new --output.")
    if old_environment and (old_environment.get("host") != platform.node() or old_environment.get("osVersion") != (platform.mac_ver()[0] if system == "Darwin" else platform.version())):
        parser.error("Evidence belongs to another machine/OS. Choose a new --output.")
    if (output / (args.suite + ".json")).exists(): parser.error("This suite already ran. Preserve it and choose a new --output for a complete rerun.")
    results = read_json(output / "exit-codes.json") or {}
    commands = {
        "cmake": ["cmake", "--version"], "ninja": ["ninja", "--version"], "git": ["git", "rev-parse", "HEAD"], "changes": ["git", "status", "--porcelain"]}
    if system == "Darwin": commands.update({
        "version": ["sw_vers"], "model": ["sysctl", "-n", "hw.model"], "ram": ["sysctl", "-n", "hw.memsize"], "rosetta": ["sysctl", "-in", "sysctl.proc_translated"], "power": ["pmset", "-g", "batt"], "powerSettings": ["pmset", "-g", "custom"], "display": ["system_profiler", "SPDisplaysDataType", "-json"], "thermal": ["pmset", "-g", "therm"], "xcode": ["xcodebuild", "-version"], "clang": ["xcrun", "clang", "--version"], "cmake": ["cmake", "--version"], "ninja": ["ninja", "--version"], "git": ["git", "rev-parse", "HEAD"], "changes": ["git", "status", "--porcelain"]})
    environment = old_environment or {name: capture(command) for name, command in commands.items()}
    environment.update(system=system,host=platform.node(),head=head,architecture=platform.machine(),osVersion=platform.mac_ver()[0] if system == "Darwin" else platform.version(),sourceHashes=source,sourceClean=clean_source(),policy=POLICY)
    baseline_directory=args.baseline_dir.resolve() if args.baseline_dir else builds["baseline"]
    environment["baseline"]={"directory":str(baseline_directory),"revision":POLICY["baselineRevision"] if not args.baseline_dir else "user-supplied; review provenance","external":bool(args.baseline_dir)}
    (output / "environment.json").write_text(json.dumps(environment, indent=2), encoding="utf-8")
    (output / "source.patch").write_bytes(subprocess.check_output(["git", "diff", "--binary", "HEAD"], cwd=ROOT))
    manual = {name: {"status": "PENDING", "evidence": "", "assessor": "", "date": "", "instruction": text} for name, text in POLICY["manualChecks"].items()}
    if not (output / "manual-results.json").exists(): (output / "manual-results.json").write_text(json.dumps(manual, indent=2), encoding="utf-8")
    print("Evidence: " + str(output), flush=True)
    selected = {}
    for step in steps:
        print(step.name, flush=True)
        cached_build = step.name.endswith(("-configure", "-build")) and results.get(step.name, {}).get("status") == "PASS"
        if not cached_build: results[step.name] = dict(run_step(step, output, results), gate=step.gate)
        selected[step.name] = results[step.name]
        (output / "exit-codes.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    os_version = environment["osVersion"].split(".")
    minimum = system == "Darwin" and environment["model"]["output"].strip() == POLICY["minimumMachine"]["model"] and environment["ram"]["output"].strip() == str(POLICY["minimumMachine"]["memoryBytes"]) and os_version[:2] == [str(POLICY["minimumMachine"]["osMajor"]), str(POLICY["minimumMachine"]["osMinor"])] and environment["rosetta"]["output"].strip() != "1"
    environment.update(minimumEnvironment=minimum,sourceUnchanged=source == fingerprint())
    binary=baseline_directory/("minvex_gpu_efficiency_tests" + (".exe" if system == "Windows" else ""))
    environment["baseline"]["probeSha256"]=hashlib.sha256(binary.read_bytes()).hexdigest() if binary.is_file() else None
    (output / "environment.json").write_text(json.dumps(environment, indent=2), encoding="utf-8")
    report = assess(output, results, minimum, environment["sourceClean"], environment["sourceUnchanged"], read_json(output / "manual-results.json") or {}, bool(args.baseline_dir), system)
    (output / "warning-summary.json").write_text(json.dumps(warning_summary(output, results), indent=2), encoding="utf-8")
    report.update(assessor="Minvex automated evidence and named manual reviews",recordedAtUtc=datetime.now(timezone.utc).isoformat(),minimumEnvironment=minimum)
    (output / "approval.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    suite_status = "FAIL" if any(r["status"] == "FAIL" for r in selected.values()) or not environment["sourceUnchanged"] else "PENDING" if any(r["status"] != "PASS" for r in selected.values()) else "PASS"
    if args.suite in ("release", "all") and any(report["gates"][gate] == "FAIL" for gate in ("G7", "G8", "G9")): suite_status = "FAIL"
    if args.suite == "graphics" and any(report["gates"][gate] == "FAIL" for gate in ("G3", "G4")): suite_status = "FAIL"
    summary = {"suite": args.suite, "status": suite_status, "approval": report["conclusion"], "output": str(output), "steps": selected}
    (output / (args.suite + ".json")).write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print("Suite: " + suite_status + "; overall approval: " + report["conclusion"])
    build_lock.close()
    return 0 if suite_status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
