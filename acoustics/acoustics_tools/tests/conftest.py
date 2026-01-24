from __future__ import annotations

from dataclasses import dataclass, field
from statistics import mean, median
from typing import Any

import pytest


@dataclass
class FullstackPerfRecorder:
    direction_errors_deg: list[float] = field(default_factory=list)
    position_errors_deg: list[float] = field(default_factory=list)

    def record(self, *, direction_error_deg: float | None, position_error_deg: float | None) -> None:
        if direction_error_deg is not None:
            self.direction_errors_deg.append(float(direction_error_deg))
        if position_error_deg is not None:
            self.position_errors_deg.append(float(position_error_deg))


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
