"""Explicit external dependency locations; no device operations."""
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ.get('FM1_SDK_DIR', ROOT/'.deps/ac79-sdk')).resolve()
CORE = Path(os.environ.get('FM1_NES_CORE_DIR', ROOT/'.deps/peak-nes')).resolve()
TC = Path(os.environ.get('FM1_TOOLCHAIN_DIR', 'C:/JL/pi32/bin')).resolve()
SDK_PIN = 'e30b1ee375d1f2993fc23bf92c8b99006a6e5f9d'
CORE_PIN = '68bdfc8de570264c0e84f73766f1a1ed3591066f'
MAKE = SDK/'apps/demo/demo_hello/board/wl82/Makefile'

def make_list(text, name):
    match = re.search(r'^'+re.escape(name)+r'\s*:=\s*\\\n((?:.*\\\n)*)', text, re.M)
    if not match:raise ValueError('Missing SDK make list: '+name)
    return match.group(1).replace('\\\n', ' ').split()

def sdk_path(value):
    return (MAKE.parent/value).resolve()
