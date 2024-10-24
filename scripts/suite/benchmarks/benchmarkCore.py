import abc

import pandas as pd


class BenchmarkCore:
    """Benchmark class. All benchmarks will inherit from this class."""

    name = "core"
    registry = {}

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.name = cls.__name__.replace("Benchmark", "").lower()
        BenchmarkCore.registry[cls.name] = cls

    def __init__(self, base_dir: str):
        self.base_dir = base_dir

    @abc.abstractmethod
    def estimate_runtime(self, **kwargs) -> float | None:
        """Using the `kwargs` from the `run` function, this method should
        return the estimated runtime. Return `None` if cannot be estimated."""
        raise NotImplementedError

    @abc.abstractmethod
    def run(self, **kwargs) -> pd.DataFrame | None:
        """This method should run the benchmark with the arguments passed as `kwargs`."""
        raise NotImplementedError
