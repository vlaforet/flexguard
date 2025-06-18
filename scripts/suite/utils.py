import hashlib
import json
import subprocess

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
