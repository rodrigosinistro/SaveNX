import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile
ROOT = Path(__file__).resolve().parents[1]
def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix='savenx-test-') as work:
    work = Path(work)
    run('g++', '-std=c++20', '-Wall', '-Wextra', '-Wno-misleading-indentation', '-Werror', '-Iinclude', 'tests/core_test.cpp', '-o', str(work / 'core'))
    run(str(work / 'core'))
    run('g++', '-std=c++20', '-Wall', '-Wextra', '-Wno-misleading-indentation', '-Werror', '-Iinclude', 'tests/navigation_inventory_test.cpp', '-o', str(work/'navigation-inventory'))
    run(str(work/'navigation-inventory'))
    spec = importlib.util.spec_from_file_location('configure', ROOT/'tools/configure_google.py')
    cfg = importlib.util.module_from_spec(spec);spec.loader.exec_module(cfg)
    original = bytearray(64);original[16:20] = b'NRO0'
    binary = work/'test.nro';binary.write_bytes(original+cfg.slot()+b'tail')
    cfg.configure_binary(binary, {'client_id': 'test.apps.googleusercontent.com', 'client_secret': 'test'})
    result = binary.read_bytes();assert len(result) == 64+4096+4 and result[-4:] == b'tail'
    assert json.loads(result[64:4160])['client_id'] == 'test.apps.googleusercontent.com'
    print('Application configuration: fixed ROMFS slot and unchanged binary size passed.')
    if not Path('/usr/include/minizip/unzip.h').exists():
        print('Archive integration tests require libminizip-dev and libjson-c-dev (installed in CI).')
    else:
        meta = {'format':'savenx.original.v1','title':'010012345678ABCD','user':'00000000000056780000000000001234','type':1,'rank':0,'index':0,'data_size':1048576,'journal_size':1048576}
        def fixture(name, entries, metadata=meta):
            with zipfile.ZipFile(work/name,'w',compression=zipfile.ZIP_STORED) as z:
                if metadata is not None:z.writestr('.savenx/manifest.json', json.dumps(metadata))
                for key,value in entries:z.writestr(key,value)
        fixture('valid.zip',[('save/data/value','PLAYER_DATA_123')])
        fixture('traversal.zip',[('save/../../escape','x')]);fixture('absolute.zip',[('/tmp/escape','x')])
        fixture('duplicate.zip',[('save/value','a'),('save/value','b')]);fixture('missing.zip',[('save/value','x')],None)
        fixture('space-mismatch.zip',[('save/value','x')],dict(meta,space=4));fixture('mismatch.zip',[('save/value','x')],dict(meta,index=1));fixture('invalid-hex.zip',[],dict(meta,title='000000000000000G'))
        fixture('conflict.zip',[('save/value','x'),('save/value/child','x')]);fixture('oversize-meta.zip',[],dict(meta,game='x'*65536))
        fixture('corrupt.zip',[('save/value','PLAYER_DATA_123')]);bad=work/'corrupt.zip';bad.write_bytes(bad.read_bytes().replace(b'PLAYER_DATA_123',b'PLAYER_DATA_ABC'))
        with zipfile.ZipFile(work/'symlink.zip','w') as z:
            z.writestr('.savenx/manifest.json',json.dumps(meta));link=zipfile.ZipInfo('save/link');link.create_system=3;link.external_attr=0o120777<<16;z.writestr(link,'/etc/passwd')
        run('g++','-std=c++20','-Wall','-Wextra','-Wno-misleading-indentation','-Iinclude','tests/archive_test.cpp','source/archive.cpp','-lminizip','-ljson-c','-lz','-o',str(work/'archive'))
        run(str(work/'archive'),str(work))
