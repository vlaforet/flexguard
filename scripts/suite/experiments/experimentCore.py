import abc

import pandas as pd


class ExperimentCore:
    """Experiment class. All experiments will inherit from this class."""

    name = "core"
    registry = {}

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.name = cls.__name__.replace("Experiment", "").lower()
        ExperimentCore.registry[cls.name] = cls

    def __init__(self, with_debugging=False):
        self.with_debugging = with_debugging

    @abc.abstractmethod
    def report(self, results: pd.DataFrame, exp_dir: str):
        """This method should report on the benchmark results."""
        raise NotImplementedError
