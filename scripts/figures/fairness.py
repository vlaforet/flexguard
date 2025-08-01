#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def main():
    results = pd.read_csv(os.path.join(common.results_dir, "fairness/fairness.csv"))

    throughput_columns = [
        col for col in results.columns if col.startswith("throughput_t")
    ]
    filtered_results = results[
        (results["non-critical-cycles"] <= 10**5) & (results["duration"] == 10000)
    ]

    # Computing the fairness factor
    def get_fairness_factor(row):
        throughput_values = pd.Series(row[throughput_columns]).dropna().values
        total_throughput = np.sum(throughput_values)
        sorted_values = np.sort(throughput_values)
        return np.sum(sorted_values[len(sorted_values) // 2 :]) / total_throughput

    filtered_results["fairness_factor"] = filtered_results.apply(
        get_fairness_factor, axis=1
    )
    pd.set_option("display.max_rows", None)
    print(
        "Fairness factor:\n",
        filtered_results.groupby(["lock", "num-threads", "non-critical-cycles"])[
            "fairness_factor"
        ].mean(),
    )

    avg_results = (
        filtered_results.groupby(
            ["lock", "duration", "non-critical-cycles", "num-threads"]
        )[throughput_columns + ["fairness_factor"]]
        .mean()
        .reset_index()
    )

    avg_results["CV"] = (
        avg_results[throughput_columns].std(axis=1)
        / avg_results[throughput_columns].mean(axis=1)
    ) * 100

    plt.rcParams.update({"font.size": 28})
    #    fig, axes = plt.subplots(2, 1, figsize=(16, 6), sharex=True, sharey=False, gridspec_kw={"hspace": 0.3})
    fig, axes = plt.subplots(
        3, 1, figsize=(10, 6), sharex=True, sharey=False, gridspec_kw={"hspace": 0.3}
    )

    style = common.get_style(results["lock"].unique())

    sns.lineplot(
        data=avg_results[(avg_results["num-threads"] == 52)],
        x="non-critical-cycles",
        y="fairness_factor",
        style="lock",
        hue="lock",
        hue_order=style["zorder"],
        markers=style["markers"],
        markersize=10,
        linewidth=3,
        palette=style["palette"],
        dashes=style["dashes"],
        style_order=style["zorder"],
        errorbar=None,
        ax=axes[0],
    )
    axes[0].set_title("Non-oversubscribed fairness (0.5×\\#hw_contexts)", size=26)

    sns.lineplot(
        data=avg_results[(avg_results["num-threads"] == 104)],
        x="non-critical-cycles",
        y="fairness_factor",
        style="lock",
        hue="lock",
        hue_order=style["zorder"],
        markers=style["markers"],
        markersize=10,
        linewidth=3,
        palette=style["palette"],
        dashes=style["dashes"],
        style_order=style["zorder"],
        errorbar=None,
        ax=axes[1],
    )
    axes[1].set_title("Fullysubscribed fairness (1×\\#hw_contexts)", size=26)

    sns.lineplot(
        data=avg_results[(avg_results["num-threads"] == 208)],
        x="non-critical-cycles",
        y="fairness_factor",
        style="lock",
        hue="lock",
        hue_order=style["zorder"],
        markers=style["markers"],
        markersize=10,
        linewidth=3,
        palette=style["palette"],
        dashes=style["dashes"],
        style_order=style["zorder"],
        errorbar=None,
        ax=axes[2],
    )
    axes[2].set_title("Oversubscribed fairness (2×\\#hw_contexts)", size=26)

    fig.supylabel("Fairness Factor (0.5=Fair)", x=0.025, size=28)
    fig.supxlabel("Time between critical sections (cycles)", y=-0, size=28)

    # Settings
    for i in range(3):
        axes[i].set_xlabel("")
        axes[i].set_ylabel("")
        axes[i].grid(True)
        axes[i].legend().remove()

    plt.xscale("symlog")
    axes[0].set_ylim(0.5, 0.6)
    axes[1].set_ylim(0.5, 1)
    axes[2].set_ylim(0.5, 1)
    plt.xlim(0, avg_results["non-critical-cycles"].max())

    axes[0].set_yticklabels(["0.5", "", "0.6"])

    #    common.legend(axes[0], style, ncol=3, columnspacing=3, borderaxespad=0.3, bbox_to_anchor=(0.45, 1.1))
    plt.gcf().subplots_adjust(top=0.93, bottom=0.135, right=0.97)
    common.save_fig(plt, "fairness")


if __name__ == "__main__":
    main()
