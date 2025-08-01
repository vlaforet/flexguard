# FlexGuard SOSP'25 Artifact

This document describes the artifact for our SOSP'25 paper on FlexGuard, a systematic approach to switch from busy-waiting to blocking precisely when a lock-holding thread is preempted.

This guide is located in ARTIFACT-EVALUATION.md on the [repository](https://gitlab.inria.fr/flexguard/flexguard).

## Scope

All main results in the paper are in the scope of the artifact evaluation. Two secondary contributions are out-of-scope:

- Figure 4a (Runnable threads over time) because this figure requires handling several gigabytes of traces;
- POSIX results on Figure 4c (Busy-waiting time) because counting POSIX spinloop iterations requires patching, installing and uninstalling a custom glibc version which is prone to errors and crashes.

## Artifact Evaluation Committee

If you are not an evaluator, follow the instructions in the README.md file to install dependencies.

If you are an evaluator for the SOSP'25 AEC, we provide access to the machines that we used as well as images with all dependencies and automation scripts pre-installed.

The Intel machines we used are part of the [Paradoxe cluster](https://www.grid5000.fr/w/Rennes:Hardware#paradoxe) on the [Grid5000 project](https://grid5000.fr) (a French equivalent to CloudLab). One of the AEC chairs (Sanidhya Kashyap) was given access to Grid5000. Please send him an email with your ssh public key.

### Getting a reservation (X human-minutes & X compute-minutes)

Once you have access, connect to the frontend machine. Don't forget to replace `username` with the username of the Grid5000 account.

```shell
ssh username@access.grid5000.fr
```

Then connect to the Rennes site:

```shell
ssh rennes
```

Reserve a machine

```shell
oarsub -q default -p paradoxe -t deploy "kadeploy3 -a http://public.rennes.grid5000.fr/~vlaforet/debian12-hybridlock.dsc && sleep infinity"
```

## Getting Started Instructions (~7 human-minutes)

### 1. Clone the repository (~1 minute)

Clone the FlexGuard repository and update submodules:

```shell
git clone git@gitlab.inria.fr:flexguard/flexguard.git
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

## Running Experiments (X human-minutes & X compute-minutes)


## Generate Figures

## FAQs
