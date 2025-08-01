#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns


def main():
    results = pd.read_csv(os.path.join(common.results_dir, "pause/pause.csv"))

    results = results[(results["lock"] != "spinpark")]

    #    plt.figure(figsize=(16, 6))
    plt.figure(figsize=(10, 6))
    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    ax = sns.lineplot(
        data=results,
        x="num-threads",
        y="pauses",
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
        legend=False,
    )

    plt.xlabel("Number of threads")
    plt.ylabel("Spinloop iterations ($10^8$ Ops)")
    plt.grid(True, which="both")

    y_ticks = ax.get_yticks()
    ax.set_yticks(y_ticks, [str("0" if t == 0 else f"{t/10**8:.1f}") for t in y_ticks])
    plt.ylim(8 * -(10**5), 1.2 * 10**8)

    plt.xlim(1, results["num-threads"].max())

    plt.axvline(104, c=common.num_hw_contexts_color, zorder=0, lw=6)

    #    common.legend(ax, style, bbox_to_anchor=(0.45, 1.0), columnspacing=3, ncol=3)
    plt.gcf().subplots_adjust(top=0.93, bottom=0.15, right=0.97)
    common.save_fig(plt, "pause")


if __name__ == "__main__":
    main()
