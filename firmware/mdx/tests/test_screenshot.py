from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import usb_client as client
from test_client import Pipe

class ScreenshotTests(unittest.TestCase):
    def test_real_parser_and_png(self):
        with Pipe() as port:
            png,frame=client.capture_screenshot(port)
        self.assertEqual(frame,7);self.assertEqual(png[:8],b'\x89PNG\r\n\x1a\n')
        chunks={};offset=8
        while offset<len(png):
            n=struct.unpack_from('>I',png,offset)[0];kind=png[offset+4:offset+8];payload=png[offset+8:offset+8+n]
            self.assertEqual(struct.unpack_from('>I',png,offset+8+n)[0],zlib.crc32(kind+payload))
            chunks[kind]=payload;offset+=12+n
        self.assertEqual(offset,len(png));self.assertEqual(list(chunks),[b'IHDR',b'PLTE',b'IDAT',b'IEND'])
        self.assertEqual(struct.unpack('>IIBBBBB',chunks[b'IHDR']),(240,240,2,3,0,0,0))
        rows=zlib.decompress(chunks[b'IDAT']);self.assertEqual(len(rows),14640)
        self.assertTrue(all(rows[y*61]==0 for y in range(240)))
        self.assertGreater(sum(b!=0 for b in rows[:24*61]),0)
        self.assertEqual(set(rows[144*61:]),{0})

    def test_corruption_and_token_rejected(self):
        for mode in ('crc','token','length','hex'):
            with self.subTest(mode=mode),Pipe() as port:
                def exchange(p,command):
                    answer=original(p,command)
                    if command.startswith('MDX SHOT READ'):
                        if mode=='crc':answer=answer[:-2]+'01'
                        elif mode=='token':answer=answer.replace('00000001','00000002',1)
                        elif mode=='length':answer=answer[:-2]
                        else:answer=answer[:-1]+'x'
                    return answer
                original=client.exchange
                with patch.object(client,'exchange',side_effect=exchange):
                    with self.assertRaises(RuntimeError):client.capture_screenshot(port)

    def test_png_palette_and_size_bounds(self):
        with self.assertRaises(ValueError):client.screenshot_png(bytes(14399))
        with self.assertRaises(ValueError):client.screenshot_png(bytes([255])*14400)

if __name__=='__main__':unittest.main()
