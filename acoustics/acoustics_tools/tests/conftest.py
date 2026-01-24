from __future__ import annotations

from dataclasses import dataclass, field
from statistics import mean, median
from typing import Any

import pytest


@dataclass
class FullstackPerfRecorder:
    direction_errors_deg: list[float] = field(default_factory=list)
    position_errors_deg: list[float] = field(default_factory=list)

    # Per-test staging area; committed only when the test passes.
    _pending: dict[str, tuple[float | None, float | None]] = field(default_factory=dict, init=False, repr=False)

    # Track which nodeid produced each direction error (for min/max reporting)
    direction_error_nodeids: list[str] = field(default_factory=list)
    position_error_nodeids: list[str] = field(default_factory=list)

    def stage(self, nodeid: str, *, direction_error_deg: float | None, position_error_deg: float | None) -> None:
        self._pending[nodeid] = (
            float(direction_error_deg) if direction_error_deg is not None else None,
            float(position_error_deg) if position_error_deg is not None else None,
        )

    def commit_if_present(self, nodeid: str) -> None:
        if nodeid not in self._pending:
            return
        direction_error_deg, position_error_deg = self._pending.pop(nodeid)
        if direction_error_deg is not None:
            self.direction_errors_deg.append(direction_error_deg)
            self.direction_error_nodeids.append(nodeid)
        if position_error_deg is not None:
            self.position_errors_deg.append(position_error_deg)
            self.position_error_nodeids.append(nodeid)


@pytest.fixture(scope="session")
def fullstack_perf_recorder(pytestconfig: pytest.Config) -> FullstackPerfRecorder:
    rec = getattr(pytestconfig, "_fullstack_perf_recorder", None)
    if rec is None:
        rec = FullstackPerfRecorder()
        setattr(pytestconfig, "_fullstack_perf_recorder", rec)
    return rec


def _fmt_stats(values: list[float]) -> str:
    if not values:
        return "n=0"
    return (
        f"n={len(values)}, mean={mean(values):.2f}°, median={median(values):.2f}°, "
        f"max={max(values):.2f}°"
    )


def pytest_terminal_summary(terminalreporter: Any, exitstatus: int, config: pytest.Config) -> None:
    rec: FullstackPerfRecorder | None = getattr(config, "_fullstack_perf_recorder", None)
    if rec is None:
        return

    terminalreporter.write_sep("=", "Fullstack performance")
    terminalreporter.write_line(f"Direction error: {_fmt_stats(rec.direction_errors_deg)}")
    terminalreporter.write_line(f"Position error:  {_fmt_stats(rec.position_errors_deg)}")

    # Report which config had min/max direction error
    if rec.direction_errors_deg:
        min_idx = int(min(range(len(rec.direction_errors_deg)), key=lambda i: rec.direction_errors_deg[i]))
        max_idx = int(max(range(len(rec.direction_errors_deg)), key=lambda i: rec.direction_errors_deg[i]))
        min_err = rec.direction_errors_deg[min_idx]
        max_err = rec.direction_errors_deg[max_idx]
        min_id = rec.direction_error_nodeids[min_idx]
        max_id = rec.direction_error_nodeids[max_idx]
        # Extract config id from nodeid, e.g. ...[config_10] -> config_10
        def _extract_config_id(nodeid: str) -> str:
            import re
            m = re.search(r'\[([^\[\]]+)\]$', nodeid)
            return m.group(1) if m else nodeid
        min_cfg = _extract_config_id(min_id)
        max_cfg = _extract_config_id(max_id)
        terminalreporter.write_sep("=", "Direction estimation performance")
        terminalreporter.write_line(f"Best (min) direction error: {min_err:.2f}° [{min_cfg}]")
        terminalreporter.write_line(f"Worst (max) direction error: {max_err:.2f}° [{max_cfg}]")

    if rec.position_errors_deg:
        min_idx = int(min(range(len(rec.position_errors_deg)), key=lambda i: rec.position_errors_deg[i]))
        max_idx = int(max(range(len(rec.position_errors_deg)), key=lambda i: rec.position_errors_deg[i]))
        min_err = rec.position_errors_deg[min_idx]
        max_err = rec.position_errors_deg[max_idx]
        min_id = rec.position_error_nodeids[min_idx]
        max_id = rec.position_error_nodeids[max_idx]
        # Extract config id from nodeid, e.g. ...[config_10] -> config_10
        def _extract_config_id(nodeid: str) -> str:
            import re
            m = re.search(r'\[([^\[\]]+)\]$', nodeid)
            return m.group(1) if m else nodeid
        min_cfg = _extract_config_id(min_id)
        max_cfg = _extract_config_id(max_id)
        terminalreporter.write_sep("=", "Position estimation performance")
        terminalreporter.write_line(f"Best (min) position error: {min_err:.2f}° [{min_cfg}]")
        terminalreporter.write_line(f"Worst (max) position error: {max_err:.2f}° [{max_cfg}]")


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo):
    # Let pytest generate the report first
    outcome = yield
    rep: pytest.TestReport = outcome.get_result()

    # Only commit metrics for tests that actually passed (exclude failures, skips, xfail).
    if rep.when != "call":
        return
    if rep.outcome != "passed":
        return
    if getattr(rep, "wasxfail", False):
        return

    config = item.config
    rec: FullstackPerfRecorder | None = getattr(config, "_fullstack_perf_recorder", None)
    if rec is None:
        return
    rec.commit_if_present(item.nodeid)
