"""Build one SD-card installation ZIP. Never include an account/session file."""
from pathlib import Path
import hashlib
import json
import zipfile
VERSION = '0.4.2'
ROOT = Path(__file__).resolve().parents[1]
def package(nro=None, output=None):
    nro = Path(nro or ROOT / 'SaveNX.nro')
    output = Path(output or ROOT / f'SaveNX-v{VERSION}-switch.zip')
    data = nro.read_bytes()
    if data[0x10:0x14] != b'NRO0':
        raise ValueError('Not a Switch NRO')
    checksum = hashlib.sha256(data).hexdigest()
    instructions = (ROOT / 'INSTALL.md').read_bytes()
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr('switch/SaveNX/SaveNX.nro', data)
        z.writestr('LEIA-ME.txt', instructions)
        z.writestr('LICENSE.txt', (ROOT / 'LICENSE').read_bytes())
        z.writestr('THIRD_PARTY_NOTICES.md', (ROOT / 'THIRD_PARTY_NOTICES.md').read_bytes())
        for notice in sorted((ROOT/'licenses').glob('*.txt')):
            z.writestr('licenses/'+notice.name, notice.read_bytes())
        z.writestr('SHA256SUMS.txt', f'{checksum}  switch/SaveNX/SaveNX.nro\n')
    with zipfile.ZipFile(output) as z:
        if z.testzip():
            raise ValueError('Installation archive failed verification')
    print(json.dumps({'package': str(output), 'nro_sha256': checksum, 'nro_bytes': len(data)}))
    return output
if __name__ == '__main__':
    package()
