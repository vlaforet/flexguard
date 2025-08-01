#! /usr/bin/env python3

import common
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def locks_to_data(locks: list):
    return pd.DataFrame([{"x": 1, "y": 2, "lock": l} for l in locks])


def main():
    plt.figure(figsize=(18, 1.1))
    plt.rcParams.update({"font.size": 22})

    data = locks_to_data(
        [
            "futex",
            "mutex",
            "mcs",
            "mcstp",
            "shuffle",
            "malthusian",
            "uscl",
            "flexguard",
            "flexguardextend",
            "spinextend",
        ]
    )

    style = common.get_style(data["lock"].unique())

    ax = sns.lineplot(
        data=data,
        x="x",
        y="y",
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

    handles, labels = ax.get_legend_handles_labels()
    ax.remove()

    axes = plt.axes()

    axes.set_frame_on(False)
    axes.set_xticks([])
    axes.set_yticks([])

    axes.legend(
        loc="center",
        bbox_to_anchor=(0.487, 0.5),
        ncol=5,
        frameon=False,
        labels=style["legendlabels"],
        handles=[handles[labels.index(i)] for i in style["legendorder"]],
        labelspacing=0.3,
        columnspacing=2,
        borderaxespad=0.1,
    )

    common.save_fig(plt, "legend")


if __name__ == "__main__":
    main()
