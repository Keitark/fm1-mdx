"""Build MDX host checks or an offline FM1 application; no device I/O."""
from pathlib import Path
import argparse
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
def run(args):subprocess.run(list(map(str,args)),cwd=ROOT,check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=('host','firmware'));p.add_argument('--usb-audio',action='store_true');a=p.parse_args()
    if a.target=='firmware':run([sys.executable,ROOT/'scripts/build_target.py']+(['--usb-audio'] if a.usb_audio else []));return
    if a.usb_audio:p.error('--usb-audio applies to firmware; host tests cover both profiles')
    run([sys.executable,ROOT/'firmware/mdx/samples/make_demo.py'])
    folder=ROOT/'firmware/mdx/build/host'
    run(['cmake','-S',ROOT/'firmware/mdx','-B',folder])
    run(['cmake','--build',folder,'--config','Release','--parallel','4'])
    run(['ctest','--test-dir',folder,'-C','Release','--output-on-failure'])
    run([sys.executable,ROOT/'firmware/mdx/tests/test_client.py','-v'])
    run([sys.executable,ROOT/'firmware/mdx/tests/test_screenshot.py','-v'])
    run([sys.executable,ROOT/'firmware/usb-audio/test_profile.py','-v'])
if __name__=='__main__':main()
