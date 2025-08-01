#! /usr/bin/env python3

import os

import common
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import pandas as pd
import seaborn as sns


def make_plot(cluster_name, core_count, threads_cutoff):
    results = pd.read_csv(os.path.join(common.results_dir, "buckets.csv"))

    if core_count == 104:
        results = results[results["num-threads"] <= 256]

    results["normalized_throughput"] = results["throughput"] * results["num-threads"]

    # Show average improvement for FlexGuard compared to POSIX
    av_tp_no_sub = (
        results[results["num-threads"] <= core_count]
        .groupby("lock")["normalized_throughput"]
        .mean()
    )
    print(
        f"FlexGuard improves non-oversubscribed performance on average by {100*(av_tp_no_sub['flexguard']-av_tp_no_sub['mutex'])/av_tp_no_sub['mutex']}% over POSIX"
    )

    av_tp_sub = (
        results[results["num-threads"] > core_count]
        .groupby("lock")["normalized_throughput"]
        .mean()
    )
    print(
        f"FlexGuard improves oversubscribed performance on average by {100*(av_tp_sub['flexguard']-av_tp_sub['mutex'])/av_tp_sub['mutex']}% over POSIX"
    )

    fig, axes = plt.subplots(
        1, 2, figsize=(10, 6), sharex=False, sharey=False, gridspec_kw={"wspace": 0.01}
    )

    plt.rcParams.update({"font.size": 28})

    style = common.get_style(results["lock"].unique())

    sns.lineplot(
        data=results,
        x="num-threads",
        y="normalized_throughput",
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

    sns.lineplot(
        data=results,
        x="num-threads",
        y="normalized_throughput",
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

    # Settings
    for i in range(2):
        axes[i].set_xlabel("")
        axes[i].set_ylabel("")
        axes[i].grid(True)
        axes[i].legend().remove()
        axes[i].tick_params(axis="both", which="major", labelsize=28)
        axes[i].tick_params(axis="both", which="minor", labelsize=16)

    max_x = int(results["num-threads"].max())

    axes[0].set_ylim(bottom=0)
    axes[0].set_ylabel("Throughput ($10^8$ Ops/s)", size=28)
    y_ticks = axes[0].get_yticks()
    axes[0].set_yticks(y_ticks, [str(int(0) if t == 0 else t / 10**8) for t in y_ticks])
    axes[0].set_xlim(1, threads_cutoff)

    axes[0].set_xscale("log")
    xticks = (
        [1, 2, 4, 8, 16, 32, 64]
        if cluster_name == "paradoxe"
        else [1, 2, 8, 32, 128, 512]
    )
    axes[0].set_xticks(xticks, xticks)
    axes[0].xaxis.set_minor_locator(ticker.NullLocator())

    axes[1].set_yscale("log")
    axes[1].set_ylim(bottom=0.25, top=1000000000)
    axes[1].set_ylabel("Throughput (Ops/s)", size=28)
    axes[1].set_xlim(threads_cutoff, max_x)
    axes[1].yaxis.set_label_position("right")
    axes[1].yaxis.tick_right()
    axes[1].set_facecolor("#fce6e5")

    axes[1].set_xscale("log")
    xticks = [128, 256] if cluster_name == "paradoxe" else [576, 640, 704, 768]
    axes[1].set_xticks(xticks, xticks)
    axes[1].xaxis.set_minor_locator(ticker.NullLocator())

    # Define the custom formatter function
    def custom_y_formatter(y, pos):
        # Check if the value is very close to 10 (using tolerance for float comparison)
        if np.isclose(y, 10):
            return "10"  # Use plain '10' string
        else:
            # For other ticks, use default log formatting (powers of 10)
            # Check if it's an integer power of 10 to apply standard format
            power = np.log10(y)
            if np.isclose(power, round(power)) and y >= 1:  # Only format powers >= 10^0
                return f"$10^{{{int(round(power))}}}$"
            else:
                # Return empty string for auto-placed ticks that are not integer powers of 10
                return ""

    # Create the formatter object from the function
    formatter = ticker.FuncFormatter(custom_y_formatter)

    # Apply the formatter to the y-axis of axes[1]
    axes[1].yaxis.set_major_formatter(formatter)

    axes[0].text(
        s="Linear $y$-scale",
        x=1.1,
        y=axes[0].get_ylim()[1] * 0.98,
        fontsize=28,
        color="black",
        va="top",
        ha="left",
    )
    axes[1].text(
        s="Log. $y$-scale",
        x=(axes[1].get_xlim()[1] - threads_cutoff) * 0.02 + threads_cutoff,
        y=0.75,
        fontsize=28,
        color="red",
        va="center",
        ha="left",
    )

    fig.supxlabel("Number of benchmark threads", y=0.0285, size=28)

    axes[0].axvline(core_count, c=common.num_hw_contexts_color, lw=6, zorder=0)

    plt.gcf().subplots_adjust(top=0.95, bottom=0.15, right=0.88)
    common.save_fig(plt, f"buckets-{cluster_name}")


if __name__ == "__main__":
    print("Plotting Paradoxe")
    make_plot("paradoxe", 104, 110)
