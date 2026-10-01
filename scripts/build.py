"""Build MDX host checks or an offline FM1 application; no device I/O."""
from pathlib import Path
import argparse
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
def run(args):subprocess.run(list(map(str,args)),cwd=ROOT,check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=('host','firmware'));p.add_argument('--usb-audio',action='store_true')
    p.add_argument('--lcd-rgb444',action='store_true');p.add_argument('--lcd-spi',type=int,choices=(12,15,30),default=12);a=p.parse_args()
    if a.target=='firmware':
        run([sys.executable,ROOT/'scripts/build_target.py','--lcd-spi',str(a.lcd_spi)]+(['--usb-audio'] if a.usb_audio else [])+(['--lcd-rgb444'] if a.lcd_rgb444 else []));return
    if a.lcd_rgb444 or a.lcd_spi!=12:p.error('LCD profile flags apply to firmware; host tests cover both pixel formats')
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
