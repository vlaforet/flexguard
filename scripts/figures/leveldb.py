#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def make_plot(cluster_name, bench_name, core_count, power_of_10=6):
    results = pd.read_csv(os.path.join(common.results_dir, "leveldb.csv"))

    if core_count == 104:
        results = results[results["threads"] <= 256]

    results = results[results["benchmarks"] == bench_name]

    results["ops_per_sec"] = results["threads"] / results[f"latency_{bench_name}"] * 1e6

    # Show average improvement for FlexGuard compared to POSIX
    av_tp_no_sub = (
        results[results["threads"] <= core_count].groupby("lock")["ops_per_sec"].mean()
    )
    print(
        f"FlexGuard improves non-oversubscribed performance on average by {100*(av_tp_no_sub["flexguard"]-av_tp_no_sub["mutex"])/av_tp_no_sub["mutex"]}% over POSIX"
    )

    av_tp_sub = (
        results[results["threads"] > core_count].groupby("lock")["ops_per_sec"].mean()
    )
    print(
        f"FlexGuard improves oversubscribed performance on average by {100*(av_tp_sub["flexguard"]-av_tp_sub["mutex"])/av_tp_sub["mutex"]}% over POSIX"
    )

    plt.figure(figsize=(10, 6))
    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    ax = sns.lineplot(
        data=results,
        x="threads",
        y="ops_per_sec",
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
    plt.ylabel(f"Throughput ($10^{power_of_10}$ Ops/s)")
    plt.grid(True, which="both")
    plt.xscale("log", base=2)

    y_ticks = ax.get_yticks()
    ax.set_yticks(
        y_ticks, [str("0" if t == 0 else f"{t / 10**power_of_10:.1f}") for t in y_ticks]
    )
    plt.ylim(bottom=0.0)

    max_x = int(results["threads"].max())
    plt.xlim(1, max_x)
    xticks = [2**i for i in range(int(np.log2(max_x) + 1))]
    plt.xticks(xticks, xticks)

    plt.axvline(core_count, c=common.num_hw_contexts_color, lw=6, zorder=0)

    ax.get_legend().remove()
    plt.gcf().subplots_adjust(top=0.95, bottom=0.15, right=0.97)
    common.save_fig(plt, f"leveldb-{bench_name}-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", "readrandom", 104)
    make_plot("paradoxe", "fillrandom", 104)
    make_plot("paradoxe", "fillseq", 104)
    make_plot("paradoxe", "readseq", 104)
    make_plot("paradoxe", "overwrite", 104)
