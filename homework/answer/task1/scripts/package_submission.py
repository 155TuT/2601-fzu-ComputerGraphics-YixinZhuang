"""Compatibility entry point for the single canonical PowerShell packager."""
from pathlib import Path
import shutil
import subprocess

if __name__ == '__main__':
    shell = shutil.which('powershell.exe') or shutil.which('pwsh')
    if shell is None:
        raise SystemExit('PowerShell is required to package this Windows submission.')
    raise SystemExit(subprocess.call([
        shell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        str(Path(__file__).with_suffix('.ps1')),
    ]))
