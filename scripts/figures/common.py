import os
import random

import matplotlib
import matplotlib.pyplot as plt

"""
Lock styles. 
lockname: (label, color, dashes, marker style, zorder back to front, legend order top to bottom)
"""

styles = {
    "malthusian": ("Malthusian", "gray", (4, 2), "^", 1, 6),
    "shuffle": ("Shuffle lock", "black", (4, 4), "*", 2, 5),
    "mcstp": ("MCS-TP", "#ff8800", (3, 1, 1, 1, 1, 1), "D", 4, 4),
    "mcs": ("MCS", "red", (6, 2, 2, 2), "X", 5, 3),
    "mutex": ("POSIX", "#008080", (1, 1.5), "s", 6, 2),
    "futex": ("Pure blocking lock", "blue", "", "P", 7, 1),
    "spinextend": (
        "Spinlock w/ timeslice extension",
        "brown",
        (1, 1, 10, 1),
        "p",
        9,
        6.8,
    ),
    "flexguard": ("FlexGuard", "#00cc00", "", "o", 10, 7),
    "mcstas": ("MCS/TAS", "#9e0cac", "", "<", 5, 3.1),
    "ideal": ("Ideal hybrid lock", "#60ff60", "", "", 0, 7),
    "mcsblock": ("MCS-Block", "purple", (2, 1), ">", 5, 4.2),
    "uscl": ("u-SCL", "#cc6cd9", (8, 4), "X", 5, 6.999),
    "flexguardextend": (
        "FlexGuard w/timeslice extension",
        "#577557",
        (1, 1, 1, 1, 4, 1),
        "o",
        9.8,
        6.9,
    ),
}

base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
results_dir = os.path.join(base_dir, "results")


def get_style(locks):
    missing = [l for l in locks if l not in styles.keys()]

    return {
        "zorder": [
            l for l, _ in sorted(styles.items(), key=lambda x: x[1][4]) if l in locks
        ]
        + missing,
        "palette": dict(
            {l: a[1] for l, a in styles.items() if l in locks},
            **{l: f"#{random.randint(0, 0xFFFFFF):06x}" for l in missing},
        ),
        "dashes": dict(
            {l: a[2] for l, a in styles.items() if l in locks},
            **{l: "" for l in missing},
        ),
        "markers": dict(
            {l: a[3] for l, a in styles.items() if l in locks},
            **{l: "X" for l in missing},
        ),
        "legendlabels": [
            a[0] for l, a in sorted(styles.items(), key=lambda x: x[1][5]) if l in locks
        ]
        + missing,
        "legendorder": [
            l for l, _ in sorted(styles.items(), key=lambda x: x[1][5]) if l in locks
        ]
        + missing,
    }


def legend(
    ax,
    style,
    ncol=4,
    bbox_to_anchor=(0.5, 1.0),
    borderaxespad=0.1,
    columnspacing=0.6,
    **kwargs,
):
    handles, labels = ax.get_legend_handles_labels()
    return ax.legend(
        loc="lower center",
        bbox_to_anchor=bbox_to_anchor,
        ncol=ncol,
        frameon=False,
        labels=style["legendlabels"],
        handles=[handles[labels.index(i)] for i in style["legendorder"]],
        labelspacing=0.3,
        columnspacing=columnspacing,
        borderaxespad=borderaxespad,
        **kwargs,
    )


def save_fig(fig, name):
    figs_path = os.path.join(results_dir, "figs")
    if not os.path.exists(figs_path):
        os.makedirs(figs_path)
    output_path = os.path.join(figs_path, name)
    #    fig.savefig(f"{output_path}.svg", dpi=600, bbox_inches="tight")
    #    fig.savefig(f"{output_path}.pgf", dpi=600, bbox_inches="tight")
    fig.savefig(f"{output_path}.svg", dpi=600, metadata={"Date": ""})
    # fig.savefig(f"{output_path}.pgf", dpi=600)
    fig.savefig(f"{output_path}.png", dpi=600)
    print(f"Wrote full plot to {output_path}.svg and {output_path}.png")


matplotlib.rcParams.update(
    {
        "font.family": "serif",
        "pgf.rcfonts": False,
        "svg.hashsalt": "fixed-salt",
    }
)

num_hw_contexts_color = "#444444"
