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


def getExperiments(experiments: Optional[List[str]], locks):
    exps = plugins.getExperiments(experiments or [], locks=locks)
    if exps is None:
        typer.echo("No experiments have been specified")
        raise typer.Exit(1)
    return exps


def parse_locks_option(value: str) -> dict:
    """
    Parse locks and labels into a dictionary.
    The input format should be label1=lock1,label2=lock2,...
    """
    try:
        return dict(item.split("=") for item in value.split(","))
    except ValueError:
        raise typer.BadParameter(
            "Locks must be in the format label1=lock1,label2=lock2,..."
        )


ReplicationOption = typer.Option(3, "-r", help="Number of replications of each test")
CacheOption = typer.Option(True, "--no-cache", is_flag=True, help="Ignore cache")
OnlyCacheOption = typer.Option(
    False, "--only-cache", is_flag=True, help="Only retrieve cached results"
)
TempDirOption = typer.Option(
    "/tmp", "--tmp", help="Temporary directory used by benchmarks"
)
ExperimentsOption = typer.Option(None, "-e", help="Experiments to run")
ResultsDirOption = typer.Option(
    os.path.join(base_dir, "results"),
    "-o",
    help=f"Results directory",
)
LocksOption = typer.Option(
    "FlexGuard=flexguard,MCS-TP=mcstp,MCS-TP Extend=mcstpextend,MCS=mcs,Spin Extend Time Slice=spinextend,MCS Extend Time Slice=mcsextend,POSIX=mutex,Pure blocking lock=futex,MCS-TP=mcstp,Shfllock no Shuffle=shufflenoshuffle,Shfllock=shuffle,Shfllock Extend=shuffleextend,Malthusian=malthusian",
    callback=parse_locks_option,
    help="Locks and their labels in the format label1=lock1,label2=lock2,...",
)


@app.command()
def record(
    replication: int = ReplicationOption,
    cache: bool = CacheOption,
    only_cache: bool = OnlyCacheOption,
    temp_dir: str = TempDirOption,
    experiments: Optional[List[str]] = ExperimentsOption,
    results_dir: str = ResultsDirOption,
    locks: str = LocksOption,
):
    """Run and record benchmark results."""
    exps = getExperiments(experiments, locks)
    record = RecordCommand(
        base_dir, temp_dir, results_dir, exps, replication, cache, only_cache
    )
    record.run()


@app.command()
def report(
    experiments: Optional[List[str]] = ExperimentsOption,
    results_dir: str = ResultsDirOption,
):
    """Report benchmark results."""
    exps = getExperiments(experiments, {})
    report = ReportCommand(base_dir, results_dir, exps)
    report.run()


if __name__ == "__main__":
    signal.signal(signal.SIGINT, lambda _, __: sys.exit(0))
    app()
