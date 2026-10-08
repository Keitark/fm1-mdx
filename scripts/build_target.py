"""Offline MDX-only WL82 link. Uses an external pinned SDK, never a stock dump/device."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
USB=ROOT/'firmware/usb-diag'
BOARD=ROOT/'firmware/nes'
MDX=ROOT/'firmware/mdx'
sys.path[:0]=[str(BOARD),str(USB)]
from build_env import SDK,SDK_PIN,TC,MAKE,make_list,sdk_path
from audit_boot import audit
import vendor_overlay
from private_song import select_private_song,generate_private_song

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path);p.add_argument('--usb-audio',action='store_true')
    p.add_argument('--lcd-rgb444',action='store_true')
    p.add_argument('--lcd-spi',type=int,choices=(12,15,30),default=12,help='Nominal MHz with60MHz LSB;30MHz is a bench experiment')
    p.add_argument('--default-mdx',type=Path);p.add_argument('--default-pdx',type=Path)
    p.add_argument('--default-mdx-sha256');p.add_argument('--default-pdx-sha256')
    a=p.parse_args();suffix=('-lcd%d%s'%(a.lcd_spi,'-444' if a.lcd_rgb444 else '-565')) if a.lcd_spi!=12 or a.lcd_rgb444 else ''
    private=select_private_song(a.default_mdx,a.default_pdx,a.default_mdx_sha256,a.default_pdx_sha256)
    if private is not None and a.out is None:p.error('Private defaults require an explicit NEW --out path')
    out=(a.out or ROOT/(('build/target-audio' if a.usb_audio else 'build/target')+suffix)).resolve()
    sample_source=generate_private_song(private,out,ROOT) if private is not None else MDX/'samples/demo.c'
    if private is None:out.mkdir(parents=True,exist_ok=True)
    default_song=private.metadata if private is not None else {'kind':'original_demo','title':'FM1 original karaoke demo'}
    manifest=out/'build-manifest.json'
    manifest.write_text(json.dumps({'status':'BUILDING_OR_FAILED','flashable':False,'default_kind':default_song['kind'],'default_song':default_song})+'\n')
    with (out/'build.log').open('w',encoding='utf8') as log:
        def run(args):
            r=subprocess.run(list(map(str,args)),cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            log.write(' '.join(map(str,args))+'\n'+r.stdout);log.flush()
            if r.returncode:raise RuntimeError(r.stdout[-8000:])
            return r.stdout
        if run(['git','-C',SDK,'rev-parse','HEAD']).strip()!=SDK_PIN:raise ValueError('Wrong SDK revision')
        if run(['git','-C',SDK,'status','--porcelain']).strip():raise ValueError('SDK is dirty')
        for exe in ('clang.exe','pi32v2-lto-wrapper.exe','llvm-objcopy.exe','llvm-nm.exe','llvm-objdump.exe'):
            if not (TC/exe).is_file():raise ValueError('Missing tool '+str(TC/exe))
        make=MAKE.read_text(encoding='utf8');flags=make_list(make,'CFLAGS')
        defines=make_list(make,'DEFINES')+['-DFM1_USB_CONTROLLER=0','-DFM1_PERIPHERAL_TESTS=1',
            '-DFM1_LCD_STOCK_FILL=1','-DFM1_LCD_STOCK_DMA=1','-DFM1_LCD_STOCK_SEQUENCE=1',
            '-DFM1_MDX_PLAYER=1','-DFM1_TARGET_PI32V2=1','-DFM1_KEYSCAN_DMA2=1',
            '-DFM1_KEYSCAN_IRQ=1','-DFM1_KEYSCAN_PACED=1','-DFM1_KEYSCAN_CLOCK_QUANTUM_US=10000',
            '-DFM1_KEYSCAN_LED_BAUD=119']
        includes=['-I'+str(MDX/n) for n in ('include','vendor/retrofm','vendor/mdxtools')]
        if private is not None:defines+=['-DFM1_MDX_PRIVATE_DEFAULT=1']
        if a.lcd_rgb444:defines+=['-DFM1_MDX_LCD_RGB444=1']
        if a.lcd_spi!=12:defines+=['-DFM1_MDX_LCD_BAUD='+str({15:3,30:1}[a.lcd_spi])]
        if a.usb_audio:
            defines+=['-DFM1_USB_AUDIO=1'];includes+=['-I'+str(ROOT/'firmware/usb-audio')]
        includes+=['-I'+str(n) for n in (USB,BOARD/'boot',BOARD/'include',SDK/'apps/common',SDK/'apps/common/usb',SDK/'apps/common/usb/device')]
        includes+=['-I'+str(sdk_path(s[2:])) for s in make_list(make,'INCLUDES')]
        sources=[sdk_path(s) for field in ('c_SRC_FILES','S_SRC_FILES') for s in make_list(make,field)]
        sources=[s for s in sources if s.name not in ('app_main.c','board.c','cpp_run_init.c')]
        sources+=[BOARD/'boot'/n for n in ('board.c','boot_compat.c','boot_trace.c','board_power.c')]
        sources+=[USB/n for n in ('app_main.c','protocol.c','descriptors.c','usb_policy.c','dma.c','packet.c','rx_channel.c','boot_entry.c','peripheral_logic.c')]
        sources+=[BOARD/'boot/display_test.c',BOARD/'src/fm1_wl82_keyscan.c',BOARD/'src/fm1_stock_keys.c']
        fast=list((MDX/'src').glob('*.c'))+list((MDX/'vendor/retrofm').glob('*.c'))+list((MDX/'vendor/mdxtools').glob('*.c'))
        fast+=[sample_source,BOARD/'src/fm1_volume.c',BOARD/'src/fm1_audio_queue.c']
        if a.usb_audio:fast+=[ROOT/'firmware/usb-audio'/n for n in ('bridge.c','profile.c','target.c')]
        sources+=fast+[SDK/'apps/common/usb/usb_config.c']
        overlays={}
        for name,transform in (('cdc.c',vendor_overlay.cdc),('usb_device.c',vendor_overlay.device),('msd_upgrade.c',vendor_overlay.boot_entry)):
            original=SDK/'apps/common/usb/device'/name;target=out/('fm1-'+name)
            target.write_text(transform(original.read_text(encoding='utf8')),encoding='utf8')
            sources.append(target);overlays[str(original)]=hashlib.sha256(original.read_bytes()).hexdigest()
        for source,name in ((SDK/'cpu/wl82/sdk_ld.c','sdk.ld'),(SDK/'cpu/wl82/sdk_used_list.c','sdk.used')):
            run([TC/'clang.exe',*flags,*defines,*includes,'-D__LD__','-E','-P',source,'-o',out/name])
        used=out/'sdk.used'
        used.write_text(used.read_text()+'\nmemory_init\nfm1_usb_task\ncdc_read_data\ncdc_write_data\nfm1_cdc_ready\nfm1_diag_feed\nfm1_usb_device_descriptor\nfm1_usb_config_descriptor\nfm1_usb_rx_irq\ngo_mask_usb_updata\nnvram_set_boot_state\nfm1_mdx_load\nfm1_mdx_render\nfm1_mdx_usb_command\nfm1_wl82_keyscan_async_raw\n')
        if a.usb_audio:
            used.write_text(used.read_text()+'\nfm1_usb_audio_dac\nfm1_usb_audio_stop\nfm1_uac_desc_config\nfm1_uac_descriptor\n')
        ld=(out/'sdk.ld').read_text()
        for old,new in (('*(.data)','*(.data .data.*)'),('*(.bss)','*(.bss .bss.*)')):ld=vendor_overlay.once(ld,old,new)
        for section in ('.syscfg.2.ops','.syscfg.1.ops'):ld=vendor_overlay.once(ld,'*('+section+')','/* no persistent cfg repair */')
        ld+='\nSECTIONS { /DISCARD/ : { *(.syscfg.2.ops) *(.syscfg.1.ops) } }\n'
        (out/'sdk.ld').write_text(ld)
        objects=[]
        for i,source in enumerate(sources):
            obj=out/(str(i)+'-'+source.name+'.o');objects.append(obj)
            f=[x for x in flags if x not in ('-O0','-O1','-O2','-O3','-Os','-Oz','-Ofast')]+['-O2'] if source in fast else flags
            run([TC/'clang.exe',*f,*defines,*includes,'-c',source,'-o',obj])
        libs=[SDK/'include_lib/newlib/pi32v2-lib'/n for n in ('libm.a','libc.a','libcompiler_rt.a')]
        libs+=[SDK/'cpu/wl82/liba'/n for n in ('cpu.a','event.a','system.a','cfg_tool.a','fs.a','common_lib.a','update.a')]
        elf=out/'fm1-mdx.elf'
        run([TC/'pi32v2-lto-wrapper.exe','-o',elf,*objects,'--start-group',*libs,'--end-group',
             '-T'+str(out/'sdk.ld'),'-M='+str(out/'fm1-mdx.map'),'--wrap=boot_info_init','--wrap=memory_init',
             '--undefined=memory_init','--plugin-opt=mcpu=r3','--plugin-opt=-mattr=+fprev1',
             '--plugin-opt=-pi32v2-large-program=true','--plugin-opt=-used-symbol-file='+str(used)])
        parts=[]
        for section in ('.text','.data','.dynamic_data','.ram0_data','.cache_ram_data'):
            file=out/(section[1:]+'.bin');run([TC/'llvm-objcopy.exe','-O','binary','-j',section,elf,file]);parts.append(file.read_bytes())
        app=out/'fm1-mdx.app.bin';app.write_bytes(b''.join(parts))
        (out/'symbols.txt').write_text(run([TC/'llvm-nm.exe','-n','-S',elf]))
        (out/'disassembly.asm').write_text(run([TC/'llvm-objdump.exe','-d','-mcpu=r3',elf]))
        report=audit(elf.read_bytes(),app.read_bytes(),usb_only=True,usb_peripheral_tests=True,usb_mdx=True,usb_audio=a.usb_audio,require_boot_trace=True,require_board_power=True)
        (out/'static-audit.json').write_text(json.dumps(report,indent=2)+'\n')
        inputs=list(sources)+list(ROOT.rglob('*.h'))+[Path(__file__),Path(__file__).with_name('private_song.py'),Path(__file__).with_name('build.py'),BOARD/'audit_boot.py',BOARD/'audit_power.py',BOARD/'audit_pre_os.py',BOARD/'audit_usb_packet.py',BOARD/'build_env.py',USB/'vendor_overlay.py']
        if private is not None:inputs += [private.mdx_path,private.pdx_path];private.verify_sources()
        result={'status':'LINKED_MDX_KARAOKE_UNTESTED','usb_audio':a.usb_audio,'flashable':False,'device_operations_performed':False,
                'default_kind':default_song['kind'],'default_song':default_song,
                'lcd_spi_mhz':a.lcd_spi,'lcd_wire_bpp':12 if a.lcd_rgb444 else 16,
                'sdk_commit':SDK_PIN,'application_bytes':len(app.read_bytes()),'application_sha256':hashlib.sha256(app.read_bytes()).hexdigest(),
                'sample_storage':'read-only flash','upload_storage':'192KiB RAM, volatile','static_audit':report,
                'source_sha256':{str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs},
                'archive_sha256':{str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in libs},'vendor_overlay_sources':overlays}
        if private is not None:private.verify_sources()
        manifest.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:result[k] for k in ('status','application_bytes','application_sha256','flashable','device_operations_performed')},indent=2))
if __name__=='__main__':main()
