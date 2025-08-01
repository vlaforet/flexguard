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

Connect to the Rennes Grid5000 site (not a machine) and execute this command:





## Generate Figures

This section explains how to reproduce the figures present in the paper from the results computed in the previous section. We recommend going through the section once, running the scripts one by one, and then downloading the entire `` folder to your computer for analysis. 

Get a new machine reservation using the previous instructions from [Getting a reservation](#artifact-evaluation-committee).

### Figure 1
Figure 1 can be reproduced by executing the script at
```shell
./scripts/figures/ideal.py
```

This script should output a table of results that correspond to the table of Figure 1b.

The plot is available in `results/`

### Figure 2
Figure 2 can be reproduced by executing the script at
```shell
./scripts/figures/microbench.py
```

The plot is available in ```





## FAQs
