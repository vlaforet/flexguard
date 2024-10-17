import importlib
import pkgutil
import sys
from typing import List

import benchmarks
import experiments
from benchmarks.benchmarkCore import BenchmarkCore
from experiments.experimentCore import ExperimentCore


def discover_plugins(namespace, suffix):
    return {
        name.replace(suffix, ""): f"{namespace.__name__}.{name}"
        for _, name, _ in pkgutil.iter_modules(namespace.__path__)
        if name.endswith(suffix)
    }


def getExperiments(names) -> List[ExperimentCore]:
    discovered = discover_plugins(experiments, "Experiment")

    filtered = {k: v for k, v in discovered.items() if names is None or k in names}
    for name, namespace in filtered.items():
        print(f"Loading {name}")
        importlib.import_module(namespace)

    return [cls() for cls in ExperimentCore.registry]


benchmark_instances = {}


def getBenchmark(name: str, base_dir) -> BenchmarkCore:
    if name not in benchmark_instances:
        discovered = discover_plugins(benchmarks, "Benchmark")

        if name not in discovered.keys():
            print(f"Unknown benchmark {name}")
            sys.exit(1)

        imported = importlib.import_module(discovered[name])
        cls = getattr(imported, f"{name.title()}Benchmark")
        benchmark_instances[name] = cls(base_dir)

    return benchmark_instances[name]
