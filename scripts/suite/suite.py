#!/usr/bin/env python3

import argparse
import os
import signal
import sys

import numpy as np
from plugins import getExperiments
from record import RecordCommand
from report import ReportCommand

np.seterr("raise")

base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(base_dir)


def recport_arguments(parser):
    parser.add_argument(
        "-e",
        dest="experiments",
        type=str,
        nargs="+",
        default=None,
        help="Experiments to run (default=None (all))",
    )
    parser.add_argument(
        "-o",
        dest="results_dir",
        type=str,
        default=os.path.join(base_dir, "results"),
        help=f"Results directory (default={os.path.join(base_dir, 'results')})",
    )
    parser.add_argument(
        "--with-debugging",
        dest="with_debugging",
        action="store_true",
        default=False,
        help="Run debugging experiments (default=True)",
    )


def record_arguments(parser):
    parser.add_argument(
        "-r",
        dest="replication",
        type=int,
        default=3,
        help="Number of replication of each test (default=3)",
    )
    parser.add_argument(
        "--no-cache",
        dest="cache",
        action="store_false",
        default=True,
        help="Ignore cache",
    )
    parser.add_argument(
        "--tmp",
        dest="temp_dir",
        type=str,
        default="/tmp",
        help=f"Temporary directory used by benchmarks. Ideally tmpfs. (default=/tmp)",
    )


def report_arguments(parser):
    pass


if __name__ == "__main__":
    signal.signal(signal.SIGINT, lambda _, __: sys.exit(0))

    parser = argparse.ArgumentParser(
        description="Run benchmarks and export in a standard way"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    record_parser = subparsers.add_parser(
        "record", help="Run and record benchmark results"
    )
    recport_arguments(record_parser)
    record_arguments(record_parser)

    report_parser = subparsers.add_parser("report", help="Report benchmark results")
    recport_arguments(report_parser)
    report_arguments(report_parser)

    recport_parser = subparsers.add_parser(
        "recport", help="Run, record and report benchmark results"
    )
    recport_arguments(recport_parser)
    record_arguments(recport_parser)
    report_arguments(recport_parser)

    args = parser.parse_args()

    experiments = getExperiments(args.experiments, args.with_debugging)
    if experiments is None:
        print("No experiments have been specified")
        sys.exit(1)

    if args.command == "record" or args.command == "recport":
        record = RecordCommand(
            base_dir,
            args.temp_dir,
            args.results_dir,
            experiments,
            args.replication,
            args.cache,
        )
        record.run()

    if args.command == "report" or args.command == "recport":
        report = ReportCommand(base_dir, args.results_dir, experiments)
        report.run()
