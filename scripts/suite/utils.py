import hashlib
import json
import subprocess

import numpy as np
import psutil


def hash_dict_sha256(input_dict):
    """
    Compute the SHA-256 hash of a dictionary.

    Args:
        input_dict (dict): The dictionary to be hashed.

    Returns:
        str: The SHA-256 hash of the dictionary in hexadecimal format.
    """
    try:
        return hashlib.sha256(
            json.dumps(input_dict, ensure_ascii=True, sort_keys=True).encode("utf-8")
        ).hexdigest()
    except TypeError as e:
        raise Exception(f"The dictionary contains non-serializable items: {e}")


def sha256_hash_file(file_path):
    """
    Compute the SHA-256 hash of a file.

    Args:
        file_path (str): Path to the file to be hashed.

    Returns:
        str: The SHA-256 hash of the file in hexadecimal format.
    """
    sha256_hash = hashlib.sha256()
    try:
        with open(file_path, "rb") as file:
            for chunk in iter(lambda: file.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        return None
    except Exception as e:
        print(f"Error: {e}")
        return None


def execute_command(commands, timeout=None, kwargs={}):
    """
    Execute a shell command and return the output.

    Args:
        commands (list): List of command strings to be executed.
        timeout (int, optional): Timeout in seconds for the command execution.
        kwargs (dict, optional): Additional keyword arguments for subprocess.Popen.

    Returns:
        tuple: (returncode, stdout, stderr) from the command execution.
    """
    try:
        process = subprocess.Popen(
            commands,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            shell=False,
            **kwargs,
        )
        stdout, stderr = process.communicate(timeout=timeout)
        return (process.returncode, stdout, stderr)

    except subprocess.TimeoutExpired:
        psproc = psutil.Process(process.pid)
        for child in psproc.children(recursive=True):
            try:
                child.kill()
            except Exception as e:
                print(f"Failed to kill one child: {e}")

        psproc.kill()
        process.kill()
        process.wait()

        raise


def get_cpu_count():
    """
    Get the number of logical CPUs available on the system.

    Returns:
        int: The number of logical CPUs.
    """
    cpu_count = psutil.cpu_count(logical=True)
    if not cpu_count:
        raise ValueError("Unable to determine CPU count.")
    return cpu_count


def get_threads():
    """
    Get a list of thread counts that experiments should run with.

    Returns:
        list: A list of thread counts to use in experiments.
    """
    threads = [
        1,
        2,
        4,
        8,
        16,
        32,
        48,
        64,
        80,
        96,
        128,
        192,
        256,
        320,
        384,
        448,
        512,
        576,
        640,
        704,
        768,
        832,
        896,
        960,
        1024,
        1088,
        1152,
        1216,
        1280,
        1344,
        1408,
        1472,
        1536,
        1600,
        1664,
        1728,
        1792,
        1856,
        1920,
        1984,
        2048,
    ]

    cpu_count = get_cpu_count()
    max_threads = int(2 ** (np.round(np.log2(cpu_count) + 2) + 1))

    return sorted(
        set(
            filter(
                lambda x: x <= max_threads,
                threads + [cpu_count, cpu_count + 1, cpu_count - 1],
            )
        )
    ), int(cpu_count / 2)
