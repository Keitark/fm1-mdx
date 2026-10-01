"""FM1 MDX CDC loader/control. Song RAM only; no flash or firmware updates."""
import argparse
from pathlib import Path
import struct
import re
import sys
import time
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/jieli-wl82'))
from usb_diag_client import exchange,IDENTITY
LIMIT=192*1024
HELLO='FM1DIAG/1 MDX-KARAOKE/1 RAM-UPLOAD UBOOT=SERIAL COMMIT=BLOCKED'

# Exact RGB565 renderer palette; expand with bit replication to PNG RGB.
SCREEN_PALETTE=[(4,4,16),(10,10,30),(55,55,100),(20,20,45),(30,30,70),
                (210,210,235),(150,150,180),(210,210,245),(245,245,255),
                (45,30,110),(70,70,140),(110,110,180),(150,150,210),
                (82,82,173),(150,170,230),(235,175,80)]
def screenshot_png(data,bits=2):
    if bits not in (2,4) or len(data)!=240*240*bits//8:raise ValueError('Unexpected screenshot size/depth')
    if bits==2 and any((b>>shift)&3==3 for b in data for shift in (6,4,2,0)):
        raise ValueError('Screenshot palette index rejected')
    def chunk(kind,payload):
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
    width=240*bits//8;rows=b''.join(b'\0'+data[y*width:(y+1)*width] for y in range(240))
    colors=bytes([8,8,8,0,255,255,255,255,0])
    if bits==4:
        colors=bytes(c for rgb in SCREEN_PALETTE for c in (((rgb[0]>>3)<<3)|((rgb[0]>>3)>>2),
                     ((rgb[1]>>2)<<2)|((rgb[1]>>2)>>4),((rgb[2]>>3)<<3)|((rgb[2]>>3)>>2)))
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',240,240,bits,3,0,0,0))+
            chunk(b'PLTE',colors)+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b''))

def capture_screenshot(port):
    header=exchange(port,'MDX SHOT BEGIN')
    match=re.fullmatch(r'OK MDX SHOT ([0-9a-f]{8}) 240 240 I([24]) ([0-9a-f]{8}) frame=([0-9]+)',header)
    if not match:raise RuntimeError('Unexpected screenshot format')
    token,bits,crc,frame=match.groups();bits=int(bits);data=bytearray()
    try:
        for offset in range(0,240*240*bits//8,96):
            answer=exchange(port,f'MDX SHOT READ {token} {offset:08x}')
            prefix=f'MDX SHOT DATA {token} {offset:08x} '
            if not answer.startswith(prefix) or len(answer)!=len(prefix)+192:
                raise RuntimeError('Screenshot token/offset/length mismatch')
            encoded=answer[len(prefix):]
            if not re.fullmatch(r'[0-9a-f]{192}',encoded):raise RuntimeError('Malformed screenshot pixels')
            data.extend(bytes.fromhex(encoded))
        if zlib.crc32(data)!=int(crc,16):raise RuntimeError('Screenshot CRC mismatch; image not written')
        png=screenshot_png(data,bits)
    except BaseException:
        try:exchange(port,f'MDX SHOT END {token}')
        except Exception:pass
        raise
    if exchange(port,f'MDX SHOT END {token}')!='OK MDX SHOT END':
        raise RuntimeError('Unexpected screenshot completion')
    return png,int(frame)

def confirm_identity(port):
    # HELLO is read-only; retry only this handshake while CDC RX settles.
    for attempt in range(3):
        try:
            answer=exchange(port,'HELLO',expected_prefix='FM1DIAG/1 ')
        except TimeoutError:
            if attempt==2:raise
            time.sleep(.25)
            continue
        if answer!=HELLO:raise RuntimeError('MDX firmware identity mismatch')
        return

def bundle(mdx,pdx=b''):
    if not mdx or len(mdx)+len(pdx)+12>LIMIT:
        raise ValueError('MDX + PDX +12-byte header must fit192KiB')
    return b'FM1M'+struct.pack('<II',len(mdx),len(pdx))+mdx+pdx

def wait_stopped(port):
    end=time.monotonic()+5
    while time.monotonic()<end:
        answer=exchange(port,'MDX STATUS',expected_prefix='MDX running=')
        if 'running=0 ' in answer and 'pending=0 ' in answer:return
        time.sleep(.02)
    raise TimeoutError('Player did not stop; no song bytes sent')

def control_reply(port,command):
    action=command.split()[1]
    prefix={'STATUS':'MDX running=','AUDIO':'MDX AUDIO ','USB':'MDX USB ',
            'TIMING':'MDX TIMING ','DISPLAY':'MDX DISPLAY ','VOLUME':'MDX VOLUME ','SCAN':'MDX SCAN ','INPUT':'MDX INPUT '}.get(action,'OK MDX QUEUED')
    return exchange(port,command,expected_prefix=prefix)

def send(port,data):
    confirm_identity(port)
    exchange(port,'MDX STOP');wait_stopped(port)
    if exchange(port,f'MDX BEGIN {len(data):08x} {zlib.crc32(data):08x}')!='OK MDX BEGIN RAM':
        raise RuntimeError('Unexpected upload acknowledgement')
    try:
        for offset in range(0,len(data),120):
            chunk=data[offset:offset+120]
            answer=exchange(port,f'MDX DATA {offset:08x} {chunk.hex()}')
            if answer!=f'OK MDX DATA {offset+len(chunk):08x}':raise RuntimeError('Offset mismatch')
        if exchange(port,'MDX END')!='OK MDX STORED RAM VOLATILE':raise RuntimeError('File not accepted')
    except BaseException:
        try:exchange(port,'MDX ABORT')
        except Exception:pass
        raise
    return 'Song loaded into RAM; lost on power-off. Use play to start.'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('list','upload','screenshot','status','input','audio','usb','timing','display','volume','scan','guide','play','stop','demo','select','mute','note'))
    p.add_argument('--port');p.add_argument('--mdx',type=Path);p.add_argument('--pdx',type=Path)
    p.add_argument('--output',type=Path,help='PNG path for screenshot (default: timestamped current-directory file)')
    p.add_argument('--track',type=int,help='1..8 for select;1..16 for mute')
    p.add_argument('--note',type=int,help='MIDI13..108');p.add_argument('--on',type=int,choices=(0,1),default=1)
    p.add_argument('--enable',type=int,choices=(0,1),help='Enable/disable guide; omit to read guide status')
    a=p.parse_args();data=None
    if a.action=='upload':
        if not a.mdx:p.error('--mdx is required')
        if a.mdx.stat().st_size>LIMIT:p.error('MDX too large')
        if a.pdx and a.pdx.stat().st_size>LIMIT:p.error('PDX too large')
        data=bundle(a.mdx.read_bytes(),a.pdx.read_bytes() if a.pdx else b'')
    if a.action in ('select','mute') and (a.track is None or not 1<=a.track<=(8 if a.action=='select' else 16)):
        p.error('--track must be1..8 (select) or1..16 (mute)')
    if a.action=='note' and (a.note is None or not 13<=a.note<=108):p.error('--note must be13..108')
    import serial
    from serial.tools import list_ports
    ports=[x for x in list_ports.comports() if (x.vid,x.pid)==IDENTITY]
    if a.action=='list':
        for x in ports:print(x.device,x.description)
        return
    if not a.port or sum(x.device.casefold()==a.port.casefold() for x in ports)!=1:
        p.error('Specify the FM1 CDC port with --port; no port opened')
    with serial.Serial(a.port,115200,timeout=.1,write_timeout=2,rtscts=False,dsrdtr=False) as port:
        port.dtr=True;port.rts=False
        # Allow the CDC session/RX rearm to settle after DTR before HELLO.
        # Hardware can otherwise discard the first command after port opening.
        time.sleep(1)
        confirm_identity(port)
        if a.action=='screenshot':
            png,frame=capture_screenshot(port)
            path=(a.output or Path(time.strftime('fm1-screen-%Y%m%d-%H%M%S.png'))).resolve()
            path.write_bytes(png);print(f'Saved240x240 frame {frame} to {path}');return
        if data is not None:print(send(port,data));return
        command='MDX '+a.action.upper()
        if a.action=='select':command+=f' {a.track-1}'
        elif a.action=='mute':command+=f' {a.track-1:02x} {a.on}'
        elif a.action=='note':command+=f' {a.note:02x} {a.on}'
        elif a.action=='guide' and a.enable is not None:command+=f' {a.enable}'
        print(control_reply(port,command))
        if a.action=='stop':wait_stopped(port)
        print(control_reply(port,'MDX STATUS'))
if __name__=='__main__':main()
