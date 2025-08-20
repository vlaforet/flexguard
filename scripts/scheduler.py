#!/usr/bin/env python3
import argparse
import hashlib
import os
import queue
import random
import subprocess
import sys
import threading
import time

path = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

tasks = [
    "scheduling",
    "buckets",
    "concbuckets",
    "leveldb",
    "concleveldb",
    "index",
    "concindex",
    "dedup",
    "concdedup",
    "raytrace",
    "concraytrace",
]

cmd = (
    lambda task: f'cd {path} && sudo ./scripts/suite.sh record -e {task} -r 1 --locks "mcs,futex,mutex,flexguard,mcstp,malthusian,shuffle,uscl"'
)

tasks_extend = tasks

cmd_extend = (
    lambda task: f'cd {path} && sudo ./scripts/suite.sh record -e {task} -r 1 --locks "flexguardextend,spinextend"'
)

cmd_export = (
    lambda task: f'cd {path} && sudo ./scripts/suite.sh record -e {task} -r 1 --locks "mcs,futex,mutex,flexguard,mcstp,malthusian,shuffle,uscl,flexguardextend,spinextend" --only-cache'
)

parser = argparse.ArgumentParser()
parser.add_argument("--export", "-e", action="store_true", help="Export all tasks")
parser.add_argument("normal_nodes", type=int)
parser.add_argument("extend_nodes", type=int)
parser.add_argument("--logs-dir", default="logs", help="Directory to store task logs")
args = parser.parse_args()

if args.export:
    for task in tasks:
        subprocess.run(cmd_export(task), text=True, check=False, shell=True)

    print("Exported tasks")
    sys.exit(0)

normal_nodes = args.normal_nodes
extend_nodes = args.extend_nodes
total_needed = normal_nodes + extend_nodes

logs_dir = os.path.abspath(args.logs_dir)
os.makedirs(logs_dir, exist_ok=True)

if total_needed == 0:
    print("No nodes requested → nothing to do.", file=sys.stderr)
    sys.exit(0)

nodes = []
with open(os.getenv("OAR_NODEFILE", ""), "r") as f:
    nodes = list({line.strip() for line in f if line.strip()})

if total_needed > len(nodes):
    print(
        f"Error: requested {total_needed} nodes but only {len(nodes)} provided.",
        file=sys.stderr,
    )
    sys.exit(2)

normal_pool = nodes[:normal_nodes]
extend_pool = nodes[normal_nodes : normal_nodes + extend_nodes]

normal_queue = queue.Queue()
extend_queue = queue.Queue()

for task in tasks:
    normal_queue.put(task)
for task in tasks_extend:
    extend_queue.put(task)


def safe_filename(task, node):
    return f"{task}_{node.split('.')[0]}.log"


def worker(node, task_queue, cmd_func, image):
    log_file = os.path.join(logs_dir, safe_filename("install", node))
    print(f"[{node}] Using image: {image} → {log_file}", flush=True)
    try:
        with open(log_file, "w") as f:
            f.write(f"[Node: {node} | image: {image}]\n\n")
            f.flush()
            subprocess.run(
                ["kadeploy3", "-a", image, "-m", node],
                stdout=f,
                stderr=f,
                text=True,
                check=False,
            )
        print(f"[{node}] Deployment completed successfully.", flush=True)
        time.sleep(60)
    except subprocess.CalledProcessError as e:
        print(f"[{node}] Deployment failed: {e}", file=sys.stderr)
        return

    while True:
        try:
            task = task_queue.get_nowait()
        except queue.Empty:
            break

        log_file = os.path.join(logs_dir, safe_filename(task, node))
        print(f"[{node}] Running: {task} → {log_file}", flush=True)

        with open(log_file, "a") as f:
            f.write(f"[Node: {node} | Task: {task}]\n\n")
            f.flush()

            try:
                while True:
                    result = subprocess.run(
                        [
                            "ssh",
                            "-o ConnectTimeout=10",
                            "-o BatchMode=yes",
                            "-o StrictHostKeyChecking=accept-new",
                            node,
                            cmd_func(task),
                        ],
                        stdout=f,
                        stderr=f,
                        text=True,
                        check=False,
                    )
                    if result.returncode != 0:
                        if result.returncode == 255:
                            f.write(
                                "\n[WARN] Connection probably timed out. Retrying...\n"
                            )
                            time.sleep(random.randint(1, 10))  # Wait before retrying
                            continue  # Retry the command

                        f.write(
                            f"\n[ERROR] Command exited with code {result.returncode}\n"
                        )
                    break
            except Exception as e:
                f.write(f"\n[EXCEPTION] {e}\n")

        task_queue.task_done()


# Launch worker threads for both pools
threads = []

for node in normal_pool:
    t = threading.Thread(
        target=worker,
        args=(
            node,
            normal_queue,
            cmd,
            "http://public.rennes.grid5000.fr/~vlaforet/debian12-hybridlock.dsc",
        ),
    )
    t.start()
    threads.append(t)

for node in extend_pool:
    t = threading.Thread(
        target=worker,
        args=(
            node,
            extend_queue,
            cmd_extend,
            "http://public.rennes.grid5000.fr/~vlaforet/hybridlock_extend_patch.dsc",
        ),
    )
    t.start()
    threads.append(t)

# Wait for all threads to finish
for t in threads:
    t.join()

print("All tasks completed successfully.")
