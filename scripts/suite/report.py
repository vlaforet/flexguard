import os
from typing import List

import pandas as pd
from experiments.experimentCore import ExperimentCore


class ReportCommand:
    def __init__(
        self,
        base_dir: str,
        results_dir: str,
        experiments: List[ExperimentCore],
    ):
        self.base_dir = base_dir
        self.results_dir = results_dir
        self.experiments = experiments

    def run(self):
        for exp in self.experiments:
            print(f"Reporting experiment {exp.name}")
            exp_dir = os.path.join(self.results_dir, exp.name)
            os.makedirs(exp_dir, exist_ok=True)

            results = pd.read_csv(os.path.join(exp_dir, f"{exp.name}.csv"))
            exp.report(results, exp_dir)
