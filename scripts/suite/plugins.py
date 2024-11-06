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


def import_plugins(namespace, suffix):
    discovered = discover_plugins(namespace, suffix)
    for _, plugin in discovered.items():
        importlib.import_module(plugin)


experiments_instances = {}


def getExperiments(names: List[str], with_debugging=False) -> List[ExperimentCore]:
    import_plugins(experiments, "Experiment")

    for name in names:
        if name in experiments_instances:
            continue

        if name not in ExperimentCore.registry:
            print(f"Unknown experiment {name}")
            sys.exit(1)
        experiments_instances[name] = ExperimentCore.registry[name](with_debugging)

    return [experiments_instances[name] for name in names]


benchmark_instances = {}


def getBenchmark(name: str, base_dir, temp_dir) -> BenchmarkCore:
    if name not in benchmark_instances:
        import_plugins(benchmarks, "Benchmark")

        if name not in BenchmarkCore.registry:
            print(f"Unknown benchmark {name}")
            sys.exit(1)

        benchmark_instances[name] = BenchmarkCore.registry[name](base_dir, temp_dir)

    return benchmark_instances[name]
