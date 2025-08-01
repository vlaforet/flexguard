#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
from matplotlib.ticker import MultipleLocator


def make_plot(cluster_name, core_count):
    results = pd.read_csv(os.path.join(common.results_dir, "dedup.csv"))

    if core_count == 104:
        results = results[results["threads"] <= 256]

    results = results[results["compression_type"] == "gzip"]
    results = results[results["compress"] == True]

    # Show average improvement for FlexGuard compared to POSIX
    av_tp_no_sub = (
        results[results["threads"] <= core_count].groupby("lock")["run_time"].mean()
    )
    print(
        f"FlexGuard improves non-oversubscribed performance on average by {100*(av_tp_no_sub["flexguard"]-av_tp_no_sub["mutex"])/av_tp_no_sub["mutex"]}% over POSIX"
    )

    av_tp_sub = (
        results[results["threads"] > core_count].groupby("lock")["run_time"].mean()
    )
    print(
        f"FlexGuard improves oversubscribed performance on average by {100*(av_tp_sub["flexguard"]-av_tp_sub["mutex"])/av_tp_sub["mutex"]}% over POSIX"
    )

    posix_time = (
        results[results["lock"] == "mutex"].groupby("threads")["run_time"].mean()
    )
    results["speedup"] = results["threads"].map(posix_time) / results["run_time"]

    plt.figure(figsize=(10, 6))
    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    ax = sns.lineplot(
        data=results,
        x="threads",
        y="speedup",
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
    )

    plt.xlabel("Number of benchmark threads")
    plt.ylabel("Speedup over POSIX")
    plt.grid(True, which="both")
    plt.xscale("log", base=2)

    plt.ylim(bottom=0)
    ax.yaxis.set_major_locator(MultipleLocator(0.5))

    max_x = int(results["threads"].max())
    plt.xlim(1, max_x)
    xticks = [2**i for i in range(int(np.log2(max_x) + 1))]
    plt.xticks(xticks, xticks)

    plt.axvline(core_count, c=common.num_hw_contexts_color, lw=6, zorder=0)

    # common.legend(ax, style, bbox_to_anchor=(0.48, 0.98), columnspacing=0.9, ncol=3)
    plt.gca().get_legend().remove()
    plt.gcf().subplots_adjust(top=0.95, bottom=0.15, right=0.97)
    common.save_fig(plt, f"dedup-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104)
