#! /usr/bin/python3
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import argparse
import numpy as np

parser = argparse.ArgumentParser(description="Chart data from buckets benchmark traces")
parser.add_argument(
    "-i",
    dest="input_file",
    type=str,
    default="results/buckets_traces.csv",
    help='Input CSV file (default="results/buckets_traces.csv")',
)
parser.add_argument(
    "-s",
    dest="simple_style",
    action="store_true",
    help="Should the style be simplified e.g. hybrid switches marks instead of rectangles (default=false)",
)
parser.add_argument(
    "--rtsp",
    dest="rtsp",
    action="store_true",
    help="Should the time use rtsp instead of ms (defautl=false)",
)
parser.add_argument(
    "-o",
    dest="output_file",
    type=str,
    default="out.png",
    help='Output file (default="out.png")',
)
args = parser.parse_args()


def load(filename: str):
    with open(filename, "r") as f:
        config = {}

        for line in f:
            if line[0] != "#":
                continue

            sp = line[1:].split(":")

            if len(sp) > 1:
                config[sp[0].strip()] = sp[1].strip()

        return (
            config,
            np.loadtxt(
                filename,
                delimiter=",",
                skiprows=8,
                comments="#",
                converters={0: lambda s: s.strip(), 1: int, 2: int},
                dtype=[("event", "U32"), ("rtsp", "i8"), ("value", "i8")],
            ),
        )


(config, events) = load(args.input_file)

if "Buckets" not in config:
    print("Missing bucket count information.")
    exit(1)
bucket_count = int(config["Buckets"])

if "Max value" not in config:
    print("Missing max value information")
max_value = int(config["Max value"])

if "TSC frequency" not in config:
    print("Missing TSC frequency information")
frequency = int(config["TSC frequency"]) if not args.rtsp else 1

events["rtsp"] -= np.min(events["rtsp"])
max_rtsp = np.max(events["rtsp"])

fig, ax = plt.subplots(layout="constrained")
ax.set_ylabel("Values")
ax.set_xlabel(f"Time ({'ms' if not args.rtsp else 'TSC cycles'})")
plt.suptitle("Buckets Traces")
ax.set_title(
    f"{config['Duration']}, {config['Threads']} threads, {bucket_count} buckets, {max_value} max value, {config['Offset changes']} offset changes, {config['Throughput']}",
    fontdict={"fontsize": 8},
)

### Plot accessed values
accessed_values = events[events["event"] == "accessed_value"]
ax.plot(
    accessed_values["rtsp"] / frequency,
    accessed_values["value"],
    ",c",
    label="Accessed values",
)

bucket_span = max_value // bucket_count

## Plot HybridV2 acquires
acquired_stolen = events[events["event"] == "acquired_stolen"]
acquired_spin = events[events["event"] == "acquired_spin"]
acquired_block = events[events["event"] == "acquired_block"]

ax.scatter(
    acquired_stolen["rtsp"] / frequency,
    acquired_stolen["value"] * bucket_span,
    label="Acquired Stolen",
    color="magenta",
    s=1.5,
    linewidths=[0],
)

ax.scatter(
    acquired_spin["rtsp"] / frequency,
    acquired_spin["value"] * bucket_span,
    label="Acquired Spin",
    color="orange",
    s=1.5,
    linewidths=[0],
)

ax.scatter(
    acquired_block["rtsp"] / frequency,
    acquired_block["value"] * bucket_span,
    label="Acquired Block",
    color="green",
    s=1.5,
    linewidths=[0],
)

### Plot hybrid switches
switches = events[
    np.logical_or(events["event"] == "switch_block", events["event"] == "switch_spin")
]
for i in range(bucket_count):
    bucket = switches[switches["value"] == i]
    spin = bucket[bucket["event"] == "switch_spin"]
    block = bucket[bucket["event"] == "switch_block"]

    if args.simple_style:
        if len(spin) > 0:
            ax.vlines(
                spin["rtsp"] / frequency,
                i * bucket_span,
                (i + 1) * bucket_span,
                label="Switch to spinning",
                colors=["red"],
            )
        if len(block) > 0:
            ax.vlines(
                block["rtsp"] / frequency,
                i * bucket_span,
                (i + 1) * bucket_span,
                label="Switch to blocking",
                colors=["magenta"],
            )
    else:
        for y in range(len(block)):
            x = block[y]["rtsp"]
            width = width = spin[y]["rtsp"] - x if len(spin) > y else max_rtsp - x

            ax.add_patch(
                patches.Rectangle(
                    (x / frequency, i * bucket_span),
                    width / frequency,
                    bucket_span,
                    color="grey",
                    alpha=0.2,
                    linewidth=0,
                    label="Bucket is blocking",
                )
            )

### Legend with no duplicates
handles, labels = ax.get_legend_handles_labels()
unique = [
    (h, l) for i, (h, l) in enumerate(zip(handles, labels)) if l not in labels[:i]
]
ax.legend(*zip(*unique), loc="upper center", ncols=3, bbox_to_anchor=(0.45, -0.11))
ax.set_xlim(0, max_rtsp / frequency)
ax.set_ylim(0, max_value)
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
