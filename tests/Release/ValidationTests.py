"""Regression checks for evidence failures and incomplete release gates."""
import json
import io
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import RunTests as validation


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="minvex-validation-")
        self.output = Path(self.temp.name).resolve()
        self.assertEqual(self.output.parent, Path(tempfile.gettempdir()).resolve())

    def tearDown(self):
        self.assertEqual(self.output.parent, Path(tempfile.gettempdir()).resolve())
        self.temp.cleanup()

    def write(self, name, value):
        (self.output / name).write_text(json.dumps(value), encoding="utf-8")

    def assess(self, results, **kwargs):
        return validation.assess(self.output, results, kwargs.get("minimum", True), True, kwargs.get("unchanged", True), kwargs.get("manual", {}))

    def test_plan_contains_every_automatic_followup(self):
        steps, _ = validation.plan(self.output)
        by_name = {s.name: s for s in steps}
        self.assertIn("--no-tests=error", by_name["debug-tests"].command)
        self.assertTrue(by_name["debug-variants"].validation)
        self.assertIn("18000", by_name["p01-continuous"].command)
        self.assertIn("1800", by_name["g8-lifecycle"].command)
        self.assertIn("100", by_name["g8-lifecycle"].command)
        self.assertTrue(by_name["p02-morph-1"].command[-1] == "--animate")
        self.assertEqual(sum(s.name.startswith("p01-") and s.name not in ("p01-trace", "p01-continuous") for s in steps), 15)

    def test_three_suites_cover_the_full_plan_on_both_platforms(self):
        for system in ("Darwin", "Windows"):
            complete, _ = validation.plan(self.output, system=system)
            divided = [validation.plan(self.output, system=system, suite=suite)[0] for suite in ("contracts", "graphics", "release")]
            self.assertEqual({s.name for s in complete}, {s.name for suite in divided for s in suite})
            for suite in divided:
                available = set()
                for step in suite:
                    self.assertTrue(set(step.requires) <= available, step.name)
                    available.add(step.name)
            self.assertNotIn("profile-configure", {s.name for s in complete})

    def test_windows_plan_contains_no_macos_tools_or_flags(self):
        steps, _ = validation.plan(self.output, system="Windows")
        for step in steps:
            self.assertNotIn(step.command[0], ("file", "otool", "xcrun", "leaks"))
            self.assertFalse(any("CMAKE_OSX" in arg for arg in step.command))
            self.assertFalse(step.validation)
        by_name = {s.name: s for s in steps}
        self.assertTrue(by_name["release-variants"].command[0].endswith(".exe"))
        self.assertIn("--inspect-pe", by_name["debug-architecture"].command)

    def test_cpu_and_desktop_tests_are_disjoint(self):
        steps, _ = validation.plan(self.output)
        by_name = {s.name: s for s in steps}
        self.assertIn("-LE", by_name["debug-tests"].command)
        self.assertIn("-L", by_name["debug-metal"].command)
        self.assertEqual(by_name["debug-metal"].command[by_name["debug-metal"].command.index("--parallel") + 1], "1")

    def test_mac_minimum_machine_is_not_a_windows_requirement(self):
        self.assertEqual(validation.POLICY["architectures"], {"Windows": "x64", "Darwin": "arm64"})
        steps, _ = validation.plan(self.output, system="Windows")
        results = {s.name: {"gate": s.gate, "status": "PASS"} for s in steps}
        report = validation.assess(self.output, results, False, True, True, {}, system="Windows")
        self.assertEqual(report["gates"]["G0"], "PASS")
        self.assertEqual(report["gates"]["G9"], "PASS")

    def test_failure_does_not_stop_independent_work(self):
        failed = validation.run_step(validation.Step("bad", "G1", [sys.executable, "-c", "raise SystemExit(7)"]), self.output, {})
        self.assertEqual(failed["status"], "FAIL")
        passed = validation.run_step(validation.Step("good", "G2", [sys.executable, "-c", "print('independent')"]), self.output, {"bad": failed})
        blocked = validation.run_step(validation.Step("blocked", "G1", [sys.executable], ["bad"]), self.output, {"bad": failed})
        self.assertEqual(passed["status"], "PASS")
        self.assertEqual(blocked["status"], "PENDING")

    def test_missing_registered_test_cannot_pass(self):
        with patch.object(validation, "capture", return_value={"exitCode": 0, "output": '{"tests": []}' } ):
            result = validation.run_step(validation.Step("debug-metal", "G3", ["ctest", "--test-dir", "unused"]), self.output, {})
        self.assertEqual(result["status"], "FAIL")

    def test_unexecuted_checks_stay_pending(self):
        report = self.assess({})
        self.assertEqual(report["conclusion"], "PENDING")
        self.assertEqual(report["gates"]["G8"], "PENDING")
        self.assertEqual(report["gates"]["G10"], "N/A")

    def test_partial_success_does_not_complete_a_gate(self):
        report = self.assess({"debug-tests": {"gate": "G1", "status": "PASS"}})
        self.assertEqual(report["gates"]["G1"], "PENDING")

    def test_metadata_failure_is_not_downgraded_by_missing_machine(self):
        report = self.assess({"metadata": {"gate": "G0", "status": "FAIL"}}, minimum=False)
        self.assertEqual(report["gates"]["G0"], "FAIL")
        self.assertEqual(report["conclusion"], "FAIL")

    def test_source_changes_fail_even_if_programs_pass(self):
        report = self.assess({"metadata": {"gate": "G0", "status": "PASS"}}, unchanged=False)
        self.assertEqual(report["gates"]["G0"], "FAIL")

    def test_stall_is_not_erased_by_focus_event(self):
        self.write("p01-dense-dynamic-1.json", {"frames": 6000, "over33ms": 3, "maxMs": 1043.513, "inputInterruptions": 1})
        report = self.assess({"p01-dense-dynamic-1": {"gate": "G7", "status": "PASS"}}, manual={"P01-foreground": {"status": "PASS", "evidence": "operator"}})
        self.assertEqual(report["gates"]["G7"], "FAIL")

    def test_invalid_or_missing_performance_data_fails(self):
        self.write("p02-morph-1.json", {"frames": 6000, "meanMs": float("nan"), "p95Ms": 2})
        report = self.assess({"p02-morph-1": {"gate": "G7", "status": "PASS"}})
        self.assertEqual(report["gates"]["G7"], "FAIL")

    def test_short_probe_does_not_complete_long_stability_gate(self):
        self.write("g8-lifecycle.json", {"cycles": 100, "soakSeconds": 2, "elapsedSeconds": 30, "failures": 0})
        report = self.assess({"g8-lifecycle": {"gate": "G8", "status": "PASS"}})
        self.assertEqual(report["gates"]["G8"], "PENDING")

    def test_metal_step_failure_survives_missing_variant_report(self):
        report = self.assess({"debug-metal": {"gate": "G3", "status": "FAIL"}})
        self.assertEqual(report["gates"]["G3"], "FAIL")

    def test_baseline_extraction_rejects_traversal_and_links(self):
        for name, link in (("../escape", False), ("link", True)):
            archive = self.output / "unsafe.tar"
            with tarfile.open(archive, "w") as tar:
                member = tarfile.TarInfo(name)
                if link: member.type = tarfile.SYMTYPE; member.linkname = "../escape"
                else: member.size = 1
                tar.addfile(member, None if link else io.BytesIO(b"x"))
            with self.assertRaises(ValueError): validation.extract_baseline(archive, self.output / "extracted")
            self.assertFalse((self.output / "extracted").exists())

    def test_complete_evidence_can_pass_after_human_review(self):
        steps, _ = validation.plan(self.output)
        results = {s.name: {"gate": s.gate, "status": "PASS", "validationRequested": s.validation} for s in steps}
        for step in steps:
            name = step.name
            if name.startswith("p01-") and name != "p01-trace": self.write(name + ".json", {"frames": 18000 if name == "p01-continuous" else 6000, "over33ms": 0, "maxMs": 20})
            if name.startswith("p02-"): self.write(name + ".json", {"frames": 6000, "width": 1280, "height": 720, "meanMs": 5, "p95Ms": 7, "animation": True, "quality": "low", "vsync": False, "targetFps": 0})
        for config in ("debug", "release"):
            self.write(config + "-variants.json", {"caseCount": 160, "unsupported": 0, "failures": 0, "cases": [{"status": "PASS"} for _ in range(160)]})
            (self.output / (config + "-metal.log")).write_text("Metal API Validation Enabled\nMetal GPU Validation Enabled\n", encoding="utf-8")
        for mode in ("near", "far-lod", "overdraw", "instancing"):
            for run in (1, 2, 3):
                for prefix in ("p04-", "baseline-"): self.write(prefix + mode + "-" + str(run) + ".json", {"format": "MinvexGpuCacheProbe", "version": 2, "mode": mode, "width": 1280, "height": 720, "frames": 1200 if mode == "instancing" else 6000, "objects": 1000 if mode == "instancing" else 1, "completedMeanMs": 1})
        samples = [{"phase": "after-shutdown", "rssBytes": 1000000, "seconds": i / 4} for i in range(100)]
        samples += [{"phase": "soak", "rssBytes": 1000000, "seconds": 25 + i} for i in range(1800)]
        self.write("g8-lifecycle.json", {"cycles": 100, "soakSeconds": 1800, "elapsedSeconds": 1826, "failures": 0, "samples": samples})
        self.write("g8-observation.json", {"phase": "late", "elapsedSeconds": 1800, "leaks": {"exitCode": 0, "output": "0 leaks for 0 total leaked bytes"}})
        manual = {name: {"status": "PASS", "evidence": "fixture evidence", "assessor": "test operator", "date": "2026-11-16"} for name in validation.POLICY["manualChecks"]}
        report = self.assess(results, manual=manual)
        self.assertEqual(report["conclusion"], "PASS", report)


if __name__ == "__main__":
    unittest.main()
