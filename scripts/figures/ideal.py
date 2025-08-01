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
        os.path.join(common.results_dir, "scheduling.csv")
    )

    zero_rows = (results["threads"] == 0) & (results["value"] == 0)
    max_id_per_lock = results.groupby("lock")["id"].transform("max")
    id_of_max_threads = results[results["threads"] == results["threads"].max()][
        "id"
    ].min()

    clean_results = results[
        ~(
            zero_rows
            | (results["id"] == max_id_per_lock)
            | (results["id"] > id_of_max_threads)
            | (results["lock"] == "flexguard")
            | (results["lock"] == "flexguardextend")
            | (results["lock"] == "mcsblock")
        )
    ]

    agg_results = clean_results.groupby(["lock", "id"], as_index=False).agg(
        {
            **{c: "first" for c in clean_results.columns},
            "value": "mean",
        }
    )

    mutex_values = agg_results.loc[agg_results["lock"] == "futex"].set_index("id")[
        "value"
    ]
    agg_results["normalized_value"] = agg_results["value"] / agg_results["id"].map(
        mutex_values
    )

    ideal_results = agg_results.loc[
        agg_results.groupby("threads")["value"].idxmin().reset_index(drop=True)
    ]
    ideal_results["lock"] = "ideal"

    full_results = pd.concat([agg_results, ideal_results], ignore_index=True)

    print(
        full_results.loc[
            full_results[
                (full_results["lock"] == "shuffle") & (full_results["threads"] > 120)
            ]["normalized_value"].idxmin()
        ]
    )

    for i in [1, 2, 30, 60, 90, 120, 140]:
        print(
            "Pure blocking lock, "
            + str(i)
            + " threads: "
            + (
                full_results[
                    (full_results["id"] == i - 1)
                    & (full_results["lock"] == "futex")
                    & (full_results["threads"] == i)
                ]["value"]
                * 1000
            )
            .astype("str")
            .to_list()[0]
            + "us"
        )

    plt.figure(figsize=(10, 7.5))
    plt.rcParams.update({"font.size": 22})

    style = common.get_style(full_results["lock"].unique())

    ax = sns.lineplot(
        data=full_results,
        x="threads",
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

    ax.lines[style["zorder"].index("ideal")].set_linewidth(10)

    plt.xlabel("Number of threads (varies over time)")
    plt.ylabel("Normalized CS execution time")
    plt.grid(True, which="both")
    plt.yscale("symlog", linthresh=1.2, linscale=10)
    plt.ylim(0.1, 1000000)

    max_id = 140  # full_results["threads"].max()
    plt.xlim(1, max_id)

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
        (np.arange(0.2, 1.3, 0.2), [10, 100, 1000, 10000, 100000, 1000000])
    )

    plt.yticks(
        yticks_main,
        np.concatenate(
            (
                np.array([str(round(x, 1)) for x in np.arange(0.2, 1.3, 0.2)]),
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

    legend = common.legend(
        ax, style, ncol=2, bbox_to_anchor=(0.48, 1.0), columnspacing=2.5
    )
    legend.get_lines()[style["legendorder"].index("ideal")].set_linewidth(10)

    plt.gcf().subplots_adjust(top=0.72, bottom=0.1, right=0.95, left=0.09)
    common.save_fig(plt, f"ideal-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104)
