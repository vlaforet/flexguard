import abc

import pandas as pd
from utils import hash_dict_sha256


class BenchmarkCore:
    """Benchmark class. All benchmarks will inherit from this class."""

    name = "core"
    registry = {}

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.name = cls.__name__.replace("Benchmark", "").lower()
        BenchmarkCore.registry[cls.name] = cls

    def __init__(self, base_dir: str, temp_dir: str):
        self.base_dir = base_dir
        self.temp_dir = temp_dir

    @abc.abstractmethod
    def estimate_runtime(self, **kwargs) -> float | None:
        """Using the `kwargs` from the `run` function, this method should
        return the estimated runtime. Return `None` if cannot be estimated."""
        raise NotImplementedError

    def get_run_hash(self, **kwargs) -> str:
        """Using the `kwargs` from the `run` function, this method should
        return the hash of this run. Raises Exception if run cannot be hashed."""
        try:
            return hash_dict_sha256(kwargs)
        except Exception as e:
            raise Exception(f"Failed to produce run hash: {e}")

    def init(self, **kwargs) -> None:
        """This method can be used to initialize the benchmark before a
        run with the arguments passed as `kwargs`."""
        return None

    @abc.abstractmethod
    def run(self, **kwargs) -> pd.DataFrame | None:
        """This method should run the benchmark with the arguments passed as `kwargs`."""
        raise NotImplementedError
