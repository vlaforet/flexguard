import abc
from typing import Any, Dict, List, NotRequired, TypedDict

import pandas as pd


class Benchmark(TypedDict):
    id: str
    args: Dict[str, Any]


class Test(TypedDict):
    label: str
    name: str
    benchmark: Benchmark
    concurrent: NotRequired[Benchmark]


class ExperimentCore:
    """Experiment class. All experiments will inherit from this class."""

    name = "core"
    registry = {}

    tests: List[Test] = []

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.name = cls.__name__.replace("Experiment", "").lower()
        ExperimentCore.registry[cls.name] = cls

    def __init__(self):
        self.tests = []

    @abc.abstractmethod
    def report(self, results: pd.DataFrame, exp_dir: str):
        """This method should report on the benchmark results."""
        raise NotImplementedError
