class ExperimentCore:
    """Experiment class. All experiments will inherit from this class."""

    registry = []

    """Contains all of the tests to run for this experiment.
    The `__init__` method can be used to populate the list."""
    tests = []

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        ExperimentCore.registry.append(cls)

    def __init__(self, with_debugging=False):
        self.name = self.__class__.__name__.replace("Experiment", "").lower()
        self.with_debugging = with_debugging
