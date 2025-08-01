# FlexGuard SOSP'25 Artifact

This document describes the artifact for our SOSP'25 paper on FlexGuard, a systematic approach to switch from busy-waiting to blocking precisely when a lock-holding thread is preempted.

This guide is located in ARTIFACT-EVALUATION.md on the [repository](https://gitlab.inria.fr/flexguard/flexguard).

## Scope

All main results in the paper are in the scope of the artifact evaluation. Two secondary contributions are out-of-scope:

- Figure 5a (Runnable threads over time) because this figure requires handling several gigabytes of traces;
- Figure 5c (Busy-waiting time) because counting POSIX spinloop iterations requires patching, installing and uninstalling a custom glibc version which is prone to errors and crashes.

## Artifact Evaluation Committee

If you are not an evaluator, follow the instructions in the README.md file to install dependencies.

If you are an evaluator for the SOSP'25 AEC, we provide access to the machines that we used as well as images with all dependencies pre-installed.

The Intel machines we used are part of the [Paradoxe cluster](https://www.grid5000.fr/w/Rennes:Hardware#paradoxe) on the [Grid5000 project](https://grid5000.fr) (a French equivalent to CloudLab). One of the AEC chairs (Sanidhya Kashyap) was given access to Grid5000. Please send him an email with your ssh public key.

### Getting a reservation (15 human-minutes & 15 compute-minutes)

Once you have access, connect to the Rennes site. Don't forget to replace `username` twice with the username of the Grid5000 account.

```shell
ssh -J username@access.grid5000.fr username@rennes
```

Open a tmux

```shell
tmux
```

You are now in a tmux terminal which will run even if your ssh connection gets disconnected. Click `CTRL+B` then the key `D` to exit the tmux without stopping it. To re-attach to the tmux use `tmux a`.

Continue in the tmux:

Reserve a machine for X hours (1 should be enough to build and test the code).

```shell
oarsub -q default -p paradoxe -t deploy -I
```

You may have to wait for a few minutes for a machine to start. Once the shell returns (after the message `# Starting...`), install the image:

```shell
kadeploy3 -a http://public.rennes.grid5000.fr/~vlaforet/debian12-hybridlock.dsc
```

The last line of the previous command is the hostname of your machine (`paradoxe-XX.rennes.grid5000.fr`). Connect to your machine:

```shell
ssh paradoxe-XX.rennes.grid5000.fr
```

Follow the rest of the instructions in this terminal.

Once you are done with this reservation, you can release the machine by typing `exit` twice (once to exit the ssh connection to the machine and once to exit the job).

## Getting Started Instructions (~7 human-minutes)

### 1. Clone the repository (~1 minute)

Clone the FlexGuard repository and update submodules:

```shell
git clone -b aec git@gitlab.inria.fr:flexguard/flexguard.git
cd flexguard
git submodule update --init --recursive
```

### 2. Build all locks (~1 minute)

Build all variations of all locks using a single command.

```shell
./scripts/make_all.sh
```

The script will output a list of all locks built. No errors should be reported and the line `All builds completed successfully.` should be displayed.

### 3. Build benchmark applications (~5 minutes)

To build all applications at once:

```shell
./scripts/build_benchmarks.sh clean
```

Some compilation warnings and errors are to be expected and do not impact the success of the build. All builds ended succcessfully if `All builds completed successfully.` is displayed at the end of the execution.

You can now exit your machine reservation.

## Running Experiments (10 human-minutes & ~12 compute-hours)

Our experiments can take several hours to complete. To make it easier, we provide a script that will reserve, install the image and run the experiments.

Connect to the Rennes Grid5000 site (not a machine) and execute this command. Replace the dates with the next 7pm date and next 8:55am date. (Ex: Today is August 1st 2025 => Replace with `"2025-08-01 19:00:00,2025-08-02 8:55:00"`). If you are executing this after 7pm, please use the next day's date. Don't forget to replace username with the username of your Grid5000 account.
```shell
oarsub -q default -p paradoxe -l host=6 -t deploy -r "2025-08-01 19:00:00,2025-08-02 8:55:00" "/home/username/flexguard/scripts/scheduler.py 4 2"
```

## Generate Figures (~1 human-hour)

This section explains how to reproduce the figures present in the paper from the results computed in the previous section. Plots are available in svg or png formats. We recommend going through the section once, running the scripts one by one, and then downloading the entire `results/figs` folder to your computer for analysis. From your computer, execute (Don't forget to replace username with your Grid5000 username):

```shell
scp -r -o 'ProxyJump username@access.grid5000.fr' username@rennes:flexguard/results/figs .
```

Get a new machine reservation using the previous instructions from [Getting a reservation](#artifact-evaluation-committee). Execute the following scripts on the reserved machine.

To ensure all of the data CSVs have been generated properly, run this command first:
```shell
./scripts/scheduler.py --export 0 0
```

### Figure 1

Figure 1 can be reproduced by executing the script at

```shell
./scripts/figures/ideal.py
```

This script should output a table of results that correspond to the table of Figure 1b (Pure Blocking lock absolute values).

The plot is available in `results/figs/ideal-paradoxe.png` or `results/figs/ideal-paradoxe.svg`.

#### Expected results:

MCS (red) should be the best performing lock until 104 threads where MCS collapses and the best performing lock is the Pure Blocking lock (blue).

### Figure 2

Figure 2 can be reproduced by executing the script at

```shell
./scripts/figures/microbench.py
```

The plot is available in `results/figs/microbench-paradoxe.png` or `results/figs/microbench-paradoxe.svg`.

#### Expected results:

MCS (red) and FlexGuard (green) should be the best performing locks until 104 threads where MCS collapses and the best performing lock is FlexGuard, followed closely by FlexGuard w/ timeslice extension (dark green). The opposite happens when the thread count returns below 104 threads.

### Figure 3a

Figures 3a can be reproduced by executing the script at

```shell
./scripts/figures/buckets.py
```

The plot is available in `results/figs/buckets-paradoxe.png` or `results/figs/buckets-paradoxe.svg`.

#### Expected results:

MCS (red) and FlexGuard (green) should be the best performing locks until 104 threads where MCS collapses and the best performing lock is FlexGuard, followed closely by FlexGuard w/ timeslice extension (dark green) and the Spinlock w/ timeslice extension (brown).

### Figure 3b

Figures 3b can be reproduced by executing the script at

```shell
./scripts/figures/concbuckets.py
```

The plot is available in `results/figs/concbuckets-paradoxe.png` or `results/figs/concbuckets-paradoxe.svg`.

#### Expected results:

MCS (red), FlexGuard (green), FlexGuard w/ timeslice extension (dark green) and Spinlock w/ timeslice extension (brown) should be the best performing locks until 104 threads where MCS collapses and FlexGuard joins the Pure Blocking lock (blue).

### Figure 3c

Figures 3c can be reproduced by executing the script at

```shell
./scripts/figures/index.py
```

The plot is available in `results/figs/index-paradoxe.png` or `results/figs/index-paradoxe.svg`.

#### Expected results:

FlexGuard (green) and FlexGuard w/ timeslice extension (dark green) should be the best performing locks until 104 threads with a maximum throughput around 16 threads. FlexGuard's performance does not drop below the Pure Blocking lock (blue).

### Figure 3d

Figures 3d can be reproduced by executing the script at

```shell
./scripts/figures/concindex.py
```

The plot is available in `results/figs/concindex-paradoxe.png` or `results/figs/concindex-paradoxe.svg`.

#### Expected results:

FlexGuard (green) and FlexGuard w/ timeslice extension (dark green) should be the best performing locks (about 2.5x POSIX) until 104 threads with a dip between 104 and 150 threads. FlexGuard's performance does not drop below the Pure Blocking lock (blue).

### Figure 3e

Figures 3e can be reproduced by executing the script at

```shell
./scripts/figures/dedup.py
```

The plot is available in `results/figs/dedup-paradoxe.png` or `results/figs/dedup-paradoxe.svg`.

#### Expected results:

FlexGuard w/ timeslice extension (dark green) should be the best performing locks (about 2x POSIX) starting from 8 threads and FlexGuard (green) is between 1.2-1.5x faster than POSIX. FlexGuard's performance does not drop below the Pure Blocking lock (blue).

### Figure 3f

Figures 3f can be reproduced by executing the script at

```shell
./scripts/figures/concdedup.py
```

The plot is available in `results/figs/concdedup-paradoxe.png` or `results/figs/concdedup-paradoxe.svg`.

#### Expected results:

FlexGuard w/ timeslice extension (dark green) should be the best performing locks (about 2x POSIX and up to 7x POSIX) and FlexGuard (green) is between 1.2-1.5x faster than POSIX. FlexGuard's performance does not drop below the Pure Blocking lock (blue).

### Figure 3g

Figures 3g can be reproduced by executing the script at

```shell
./scripts/figures/raytrace.py
```

The plot is available in `results/figs/raytrace-paradoxe.png` or `results/figs/raytrace-paradoxe.svg`.

#### Expected results:

FlexGuard (green)'s speedup over POSIX (light cyan) increases with the number of threads (up to 1.2x POSIX). FlexGuard's performance never drops below POSIX.

### Figure 3h

Figures 3h can be reproduced by executing the script at

```shell
./scripts/figures/concraytrace.py
```

The plot is available in `results/figs/concraytrace-paradoxe.png` or `results/figs/concraytrace-paradoxe.svg`.

#### Expected results:

FlexGuard w/ timeslice extension (dark green) should be the best performing locks (up to 3x POSIX) and FlexGuard (green) is performs quite similarly to POSIX (light cyan). FlexGuard's performance does not drop significantly below POSIX.

### Figure 4

Figures 4 can be reproduced by executing the scripts at

```shell
./scripts/figures/leveldb.py
./scripts/figures/concleveldb.py
```

The plots are available in

- `results/figs/leveldb-readrandom-paradoxe.png` or `results/figs/leveldb-readrandom-paradoxe.svg` for Figure 4a
- `results/figs/concleveldb-readrandom-paradoxe.png` or `results/figs/concleveldb-readrandom-paradoxe.svg` for Figure 4b
- `results/figs/leveldb-fillrandom-paradoxe.png` or `results/figs/leveldb-fillrandom-paradoxe.svg` for Figure 4a
- `results/figs/concleveldb-fillrandom-paradoxe.png` or `results/figs/concleveldb-fillrandom-paradoxe.svg` for Figure 4b
- `results/figs/leveldb-overwrite-paradoxe.png` or `results/figs/leveldb-overwrite-paradoxe.svg` for Figure 4a
- `results/figs/concleveldb-overwrite-paradoxe.png` or `results/figs/concleveldb-overwrite-paradoxe.svg` for Figure 4b

#### Expected results:
FlexGuard (green) and FlexGuard w/ timeslice extension (dark green) are generally the best performing locks overall (with less threads than cores and with more threads than cores). Other spinlocks sometimes keep up in the non-oversubscribed case (less threads than cores) but their performance collapses as soon as over-subscription happens (more threads than cores).

## FAQs

### Lots of missing data or errors due to missing CSVs
Ensure you have executed this command on a paradoxe machine:
```shell
./scripts/scheduler.py --export 0 0
```

If this does not solve the issue, this is probably due to a too short run time. Try running the experiments again. The scheduler will resume work without restarting from the beginning.