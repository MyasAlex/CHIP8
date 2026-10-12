"""Build the offline catalog from pinned, locally downloaded source snapshots.

Copies the complete classic collection without changing its bytes or notices.
CC0 community ROMs, metadata and original cover assets keep their provenance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def text(value):
    return json.dumps(str(value), ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    pins = json.loads((ROOT/'roms/catalog/SOURCES.json').read_text())
    for name in ['community','classics','database']:
        archive = args.sources/(name+'.zip')
        if archive.is_file():
            assert hashlib.sha256(archive.read_bytes()).hexdigest()==pins[name]['archive_sha256'], f'{name}: archive differs from pinned snapshot'
            with zipfile.ZipFile(archive) as package:
                assert package.comment.decode()==pins[name]['commit'], f'{name}: commit differs'
    community = args.sources/'community/chip8Archive-master'
    classics = args.sources/'classics/chip8-roms-master'
    database = args.sources/'database/chip-8-database-master'
    out = ROOT/'roms/catalog'
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(classics, out/'classics', dirs_exist_ok=True)
    (out/'community').mkdir(exist_ok=True)
    (out/'covers').mkdir(exist_ok=True)
    shutil.copyfile(community/'Readme.md', out/'community/UPSTREAM-README.md')
    upstream = json.loads((community/'programs.json').read_text())
    db = json.loads((database/'database/programs.json').read_text())
    platforms = {p['id']:p for p in json.loads((database/'database/platforms.json').read_text())}
    upstream_db = ROOT/'third_party/chip8-database'
    upstream_db.mkdir(parents=True, exist_ok=True)
    for name in ['README.md','LICENSE.md']:
        shutil.copyfile(database/name, upstream_db/name)
    shutil.copytree(database/'database', upstream_db/'database', dirs_exist_ok=True)
    profiles = {'originalChip8':0,'modernChip8':1,'chip48':1,'superchip1':2,'superchip':2,'superchipModern':2,'xochip':3}
    defaults = {'shift':False,'memoryLeaveIUnchanged':False,'logic':False,'jump':False,'wrap':True,'vblank':False}
    known = {}
    for program in db:
        for sha1, rom in program.get('roms',{}).items():
            ids = rom.get('platforms',[])
            selected = next((p for p in ids if p in profiles), ids[0] if ids else 'modernChip8')
            p = platforms.get(selected,{})
            q = {**defaults, **p.get('quirks',{}), **rom.get('quirks',{})}
            runnable = selected in profiles and not q.get('memoryIncrementByX',False)
            bits = int(q['shift']) | int(not q['memoryLeaveIUnchanged'])<<1 | int(q['logic'])<<2 | int(q['jump'])<<3 | int(not q['wrap'])<<4 | int(q['vblank'])<<5 | 64
            keys = rom.get('keys',{})
            controls = [int(keys.get(k,d)) if isinstance(keys.get(k,d),int) else d for k,d in [('left',7),('right',9),('up',5),('down',8),('a',6),('b',4)]]
            known[sha1] = {'sha1':sha1,'title':program['title'],'authors':', '.join(program.get('authors',[])), 'description':program.get('description',''), 'platform':selected,'profile':profiles.get(selected,1),'quirks':bits,'hz':int(rom.get('tickrate',p.get('defaultTickrate',12)))*60,'runnable':runnable,'controls':controls,'colors':[0x0b1420,0xd8f3ff,0x66b3ff,0xffbd86],'path':'','cover':'','license':'metadata: MIT','source':'chip-8/chip-8-database'}
    rows = []
    for key, program in upstream.items():
        data = (community/'roms'/(key+'.ch8')).read_bytes()
        sha1 = hashlib.sha1(data).hexdigest()
        o = program.get('options',{})
        platform = program.get('platform','chip8')
        profile = {'chip8':1,'schip':2,'xochip':3}.get(platform,1)
        bits = int(o.get('shiftQuirks',False)) | int(not o.get('loadStoreQuirks',False))<<1 | int(o.get('logicQuirks',False))<<2 | int(o.get('jumpQuirks',False))<<3 | int(o.get('clipQuirks',False))<<4 | int(o.get('vBlankQuirks',False))<<5 | 64
        def rgb(name, default):
            value = o.get(name,default)
            try:return int(value.lstrip('#'),16)&0xffffff
            except (ValueError,AttributeError):return int(default.lstrip('#'),16)
        cover = ''
        for image in program.get('images',[]):
            candidate = community/'src'/key/image
            if candidate.is_file() and candidate.suffix.lower() in ['.png','.gif','.jpg','.jpeg']:
                name = key+candidate.suffix.lower()
                shutil.copyfile(candidate,out/'covers'/name)
                cover = 'roms/catalog/covers/'+name
                break
        destination = out/'community'/(key+'.ch8')
        shutil.copyfile(community/'roms'/(key+'.ch8'),destination)
        row = {'sha1':sha1,'sha256':hashlib.sha256(data).hexdigest(),'title':program['title'],'authors':', '.join(program.get('authors',[])),'description':program.get('desc',''),'platform':platform,'profile':profile,'quirks':bits,'hz':max(60,min(100000,int(float(o.get('tickrate',20)))*60)),'runnable':platform in ['chip8','schip','xochip'],'controls':known.get(sha1,{}).get('controls',[7,9,5,8,6,4]),'colors':[rgb('backgroundColor','#0b1420'),rgb('fillColor','#d8f3ff'),rgb('fillColor2','#66b3ff'),rgb('blendColor','#ffbd86')],'path':destination.relative_to(ROOT).as_posix(),'cover':cover,'license':'CC0-1.0','source':'JohnEarnest/chip8Archive','category':'Community'}
        rows.append(row);known[sha1] = row
    for path in sorted((out/'classics').rglob('*.ch8')):
        data=path.read_bytes();sha1=hashlib.sha1(data).hexdigest()
        if any(r['sha1']==sha1 for r in rows):continue
        row = dict(known.get(sha1,{'sha1':sha1,'title':path.stem,'authors':'','description':'','platform':'chip8','profile':1,'quirks':65,'hz':700,'runnable':'hires' not in path.parts,'controls':[7,9,5,8,6,4],'colors':[0x0b1420,0xd8f3ff,0x66b3ff,0xffbd86]}))
        row.update({'sha256':hashlib.sha256(data).hexdigest(),'path':path.relative_to(ROOT).as_posix(),'cover':'','license':'Original collection redistribution notice; individual notices retained','source':'kripod/chip8-roms / Revival Studios','category':'Classic'})
        if 'hires' in path.parts:row['runnable']=False;row['platform']='CHIP-8 64x64 / hybrid'
        rows.append(row)
    # Existing built-in ROMs keep their tested defaults and controls.
    for name in ['Pong.ch8','Breakout.ch8']:
        sha1=hashlib.sha1((ROOT/'roms'/name).read_bytes()).hexdigest()
        if sha1 in known:known[sha1].update(profile=1,quirks=65,hz=700)
        for row in rows:
            if row['sha1']==sha1:row.update(profile=1,quirks=65,hz=700)
    (out/'catalog.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    (out/'community/programs.json').write_text((community/'programs.json').read_text(),encoding='utf-8')
    summary={'programs':len(rows),'community':sum(r['category']=='Community' for r in rows),'classic':sum(r['category']=='Classic' for r in rows),'launchable_profiles':sum(r['runnable'] for r in rows),'other_platforms':sum(not r['runnable'] for r in rows),'known_sha1':len(known),'complete_historical_archive':False}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    header = '#pragma once\n#include <array>\n#include <cstdint>\n#include <string>\n#include <vector>\nnamespace chip8_catalog {\nstruct Program { const char* sha1; const char* title; const char* authors; const char* description; const char* platform; const char* path; const char* cover; const char* category; int profile; int quirks; int hz; bool runnable; std::array<int,6> controls; std::array<std::uint32_t,4> colors; };\n'
    def emit(row):
        strings=[row.get(k,'') for k in ['sha1','title','authors','description','platform','path','cover','category']]
        strings[3]=strings[3][:1800]
        return '{'+','.join(text(s) for s in strings)+','+','.join(str(row[k]) for k in ['profile','quirks','hz'])+','+str(row['runnable']).lower()+',{'+','.join(map(str,row['controls']))+'},{'+','.join(str(c)+'u' for c in row['colors'])+'}},\n'
    header += 'inline const std::vector<Program> Games = {\n'+''.join(emit(row) for row in rows)+'};\n'
    header += 'inline const std::vector<Program> Known = {\n'+''.join(emit(row) for row in known.values())+'};\n}\n'
    (ROOT/'src/CatalogData.hpp').write_text(header,encoding='utf-8')
    # Verify the complete original collection was retained byte-for-byte.
    checksums = []
    for original in classics.rglob('*'):
        if original.is_file():
            assert original.read_bytes()==(out/'classics'/original.relative_to(classics)).read_bytes()
            checksums.append(f"{hashlib.sha256(original.read_bytes()).hexdigest()}  {original.relative_to(classics).as_posix()}\n")
    (out/'classics-files.sha256').write_text(''.join(sorted(checksums)),encoding='utf-8')
    print(json.dumps(summary))


if __name__=='__main__':
    main()
