#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

exp_dir = os.path.join(common.base_dir, "results", "concindex")


def make_plot(cluster_name, core_count):
    results = pd.read_csv(
        os.path.join(common.results_dir, "concindex.csv")
    )

    if core_count == 104:
        results = results[results["concurrent_num-threads"] <= 256]

    benchmark_threads = results["threads"].max()

    if "seconds" in results.columns:
        results["succeeded"] = results["succeeded"] / results["seconds"]

    results["lock"] = results["index"].map(lambda x: x.replace("btreelc_", ""))

    # Show average improvement for FlexGuard compared to POSIX
    av_tp_no_sub = (
        results[results["concurrent_num-threads"] <= core_count - benchmark_threads]
        .groupby("lock")["succeeded"]
        .mean()
    )
    print(
        f"FlexGuard improves non-oversubscribed performance on average by {100*(av_tp_no_sub["flexguard"]-av_tp_no_sub["mutex"])/av_tp_no_sub["mutex"]}% over POSIX"
    )

    av_tp_sub = (
        results[results["concurrent_num-threads"] > core_count - benchmark_threads]
        .groupby("lock")["succeeded"]
        .mean()
    )
    print(
        f"FlexGuard improves oversubscribed performance on average by {100*(av_tp_sub["flexguard"]-av_tp_sub["mutex"])/av_tp_sub["mutex"]}% over POSIX"
    )

    plt.figure(figsize=(10, 6))
    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    ax = sns.lineplot(
        data=results,
        x="concurrent_num-threads",
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

    plt.xlabel(f"\\# concurrent threads (with {benchmark_threads} benchmark threads)")
    plt.ylabel("Throughput ($10^5$ Ops/s)")
    plt.grid(True, which="both")

    y_ticks = ax.get_yticks()
    ax.set_yticks(y_ticks)
    ax.set_yticklabels(str("0" if t == 0 else (t / 10**5)) for t in y_ticks)
    plt.ylim(bottom=0.0)

    plt.xscale("log", base=2)
    max_x = results["concurrent_num-threads"].max()
    plt.xlim(1, max_x)
    xticks = [2**i for i in range(int(np.log2(max_x) + 1))]
    plt.xticks(xticks, xticks)

    plt.axvline(
        core_count - benchmark_threads, c=common.num_hw_contexts_color, lw=6, zorder=0
    )

    ax.get_legend().remove()
    plt.gcf().subplots_adjust(top=0.95, bottom=0.15, right=0.97)
    common.save_fig(plt, f"concindex-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104)
