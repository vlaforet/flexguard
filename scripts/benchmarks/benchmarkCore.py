import abc

import pandas as pd


class BenchmarkCore:
    """Benchmark class. All benchmarks will inherit from this class."""

    def __init__(self, base_dir: str):
        self.base_dir = base_dir

    @abc.abstractmethod
    def combine_replications(self, *results: pd.DataFrame) -> pd.DataFrame:
        """This method should combine the `pd.DataFrame`s returned by `run`
        for the different replications"""
        raise NotImplementedError

    @abc.abstractmethod
    def estimate_runtime(self, **kwargs) -> float | None:
        """Using the `kwargs` from the `run` function, this method should
        return the estimated runtime. Return `None` if cannot be estimated."""
        raise NotImplementedError

    @abc.abstractmethod
    def run(self, **kwargs) -> pd.DataFrame | None:
        """This method should run the benchmark with the arguments passed as `kwargs`."""
        raise NotImplementedError
