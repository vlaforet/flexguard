#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def make_plot(cluster_name, core_count):
    results = pd.read_csv(os.path.join(common.results_dir, "index.csv"))

    if core_count == 104:
        results = results[results["threads"] <= 256]

    if "seconds" in results.columns:
        results["succeeded"] = results["succeeded"] / results["seconds"]

    results["lock"] = results["index"].map(lambda x: x.replace("btreelc_", ""))

    # Show average improvement for FlexGuard compared to POSIX
    av_tp_no_sub = (
        results[results["threads"] <= core_count].groupby("lock")["succeeded"].mean()
    )
    print(
        f"FlexGuard improves non-oversubscribed performance on average by {100*(av_tp_no_sub['flexguard']-av_tp_no_sub['mutex'])/av_tp_no_sub['mutex']}% over POSIX"
    )

    av_tp_sub = (
        results[results["threads"] > core_count].groupby("lock")["succeeded"].mean()
    )
    print(
        f"FlexGuard improves oversubscribed performance on average by {100*(av_tp_sub['flexguard']-av_tp_sub['mutex'])/av_tp_sub['mutex']}% over POSIX"
    )

    plt.figure(figsize=(10, 6))
    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    ax = sns.lineplot(
        data=results,
        x="threads",
        y="succeeded",
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
    plt.ylabel("Throughput ($10^5$ Ops/s)")
    plt.grid(True, which="both")

    _, ymax_data = ax.get_ylim()
    ymax_label = ymax_data / 1e5

    step = 0.5

    tick_labels_desired = np.arange(0, ymax_label + step / 2, step)
    tick_positions_data = tick_labels_desired * 1e5
    ax.set_yticks(tick_positions_data)
    ax.set_yticklabels([f"{t:.1f}" for t in tick_labels_desired])
    plt.ylim(bottom=0.0)

    plt.xscale("log", base=2)
    max_x = results["threads"].max()
    plt.xlim(1, max_x)
    xticks = [2**i for i in range(int(np.log2(max_x) + 1))]
    plt.xticks(xticks, xticks)

    plt.axvline(core_count, c=common.num_hw_contexts_color, lw=6, zorder=0)

    ax.get_legend().remove()
    plt.gcf().subplots_adjust(top=0.95, bottom=0.15, right=0.97)
    common.save_fig(plt, f"index-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104)
