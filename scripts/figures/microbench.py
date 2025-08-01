#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
from matplotlib.ticker import FixedLocator


def make_plot(cluster_name, core_count):
    results = pd.read_csv(
        os.path.join(common.results_dir, "scheduling/scheduling.csv")
    )

    zero_rows = (results["threads"] == 0) & (results["value"] == 0)
    max_id_per_lock = results.groupby("lock")["id"].transform("max")
    max_id_rows = results["id"] == max_id_per_lock

    clean_results = results[~(zero_rows | max_id_rows)]

    full_results = clean_results.groupby(["lock", "id"], as_index=False).agg(
        {
            **{c: "first" for c in clean_results.columns},
            "value": "mean",
        }
    )

    mutex_values = full_results.loc[full_results["lock"] == "futex"].set_index("id")[
        "value"
    ]
    full_results["normalized_value"] = full_results["value"] / full_results["id"].map(
        mutex_values
    )

    print("Filtering out MCS-Block")
    full_results = full_results[full_results["lock"] != "mcsblock"]

    plt.figure(figsize=(10, 7.5))
    plt.rcParams.update({"font.size": 22})

    style = common.get_style(full_results["lock"].unique())

    ax = sns.lineplot(
        data=full_results,
        x="id",
        y="normalized_value",
        style="lock",
        hue="lock",
        hue_order=style["zorder"],
        markers=False,
        markersize=10,
        linewidth=3,
        palette=style["palette"],
        dashes=style["dashes"],
        style_order=style["zorder"],
        errorbar=None,
    )

    plt.xlabel("Number of threads (varies over time)")
    plt.ylabel("Normalized CS execution time")
    plt.grid(True, which="both")
    plt.yscale("symlog", linthresh=1.2, linscale=10)
    plt.ylim(0, 100000)

    max_id = full_results["id"].max()
    plt.xlim(0, max_id)

    max_threads = full_results["threads"].max()
    max_threads_id = full_results[full_results["threads"] == max_threads]["id"].mean()

    first_face = np.floor(np.linspace(1, max_threads, 5, endpoint=False)).astype(int)
    first_ticks = [(max_threads_id * (label - 1)) / max_threads for label in first_face]
    second_ticks = [max_id - a for a in np.flip(first_ticks)]

    ax.set_xticks(np.concatenate((first_ticks, [max_threads_id], second_ticks)))
    ax.set_xticklabels(np.concatenate((first_face, [max_threads], np.flip(first_face))))

    plt.annotate(
        "Linear scale",
        xy=(max_id, 0.58),
        xytext=(max_id * (1 + (2.0 / 119)), 0.58),
        fontsize=22,
        color="black",
        rotation=90,
        va="center",
        ha="left",
    )

    plt.annotate(
        "Log. scale",
        xy=(max_id, 1000),
        xytext=(max_id * (1 + (2.0 / 119)), 1000),
        fontsize=22,
        color="red",
        rotation=90,
        va="center",
        ha="left",
    )

    plt.axhline(y=1.2, color="black", linestyle="-", linewidth=0.75)
    plt.axhspan(1.2, 1000000, color="red", alpha=0.1)

    yticks_main = np.concatenate(
        (np.arange(0, 1.3, 0.2), [10, 100, 1000, 10000, 100000, 1000000])
    )

    plt.yticks(
        yticks_main,
        np.concatenate(
            (
                np.array([str(round(x, 1)) for x in np.arange(0, 1.3, 0.2)]),
                ["10", "$10^2$", "$10^3$", "$10^4$", "$10^5$", "$10^6$"],
            )
        ),
    )

    ax.yaxis.set_major_locator(FixedLocator(yticks_main))
    ax.yaxis.set_minor_locator(
        FixedLocator(
            np.concatenate(
                (
                    np.arange(2, 11, 1),
                    np.arange(20, 101, 10),
                    np.arange(200, 1001, 100),
                    np.arange(2000, 10001, 1000),
                    np.arange(20000, 100001, 10000),
                    np.arange(200000, 1000001, 100000),
                )
            )
        )
    )

    plt.gcf().subplots_adjust(top=0.72, bottom=0.1, right=0.95, left=0.09)
    common.legend(ax, style, ncol=2, bbox_to_anchor=(0.48, 1.0), columnspacing=2)
    common.save_fig(plt, f"microbench-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104)
