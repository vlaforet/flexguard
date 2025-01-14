#!/usr/bin/env python3

import os
import signal
import sys
from typing import Annotated, List, Optional

import numpy as np
import plugins
import typer
from record import RecordCommand
from report import ReportCommand

np.seterr("raise")

app = typer.Typer()
base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(base_dir)


def getExperiments(experiments: Optional[List[str]], **kwargs):
    exps = plugins.getExperiments(experiments or [], **kwargs)
    if exps is None:
        typer.echo("No experiments have been specified")
        raise typer.Exit(1)
    return exps


ReplicationOption = typer.Option(3, "-r", help="Number of replications of each test")
CacheOption = typer.Option(True, "--no-cache", is_flag=True, help="Ignore cache")
TempDirOption = typer.Option(
    "/tmp", "--tmp", help="Temporary directory used by benchmarks"
)
ExperimentsOption = typer.Option(None, "-e", help="Experiments to run")
ResultsDirOption = typer.Option(
    os.path.join(base_dir, "results"),
    "-o",
    help=f"Results directory",
)


@app.command()
def record(
    replication: int = ReplicationOption,
    cache: bool = CacheOption,
    temp_dir: str = TempDirOption,
    experiments: Optional[List[str]] = ExperimentsOption,
    results_dir: str = ResultsDirOption,
):
    """Run and record benchmark results."""
    exps = getExperiments(experiments)
    record = RecordCommand(base_dir, temp_dir, results_dir, exps, replication, cache)
    record.run()


@app.command()
def report(
    experiments: Optional[List[str]] = ExperimentsOption,
    results_dir: str = ResultsDirOption,
):
    """Report benchmark results."""
    exps = getExperiments(experiments)
    report = ReportCommand(base_dir, results_dir, exps)
    report.run()


if __name__ == "__main__":
    signal.signal(signal.SIGINT, lambda _, __: sys.exit(0))
    app()
