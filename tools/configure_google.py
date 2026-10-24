"""Configure an application registration, never an account token. No values are logged."""
import json
import os
from pathlib import Path
SLOT = 4096
MARKER = 'SAVENX_GOOGLE_CLIENT_V1'
PREFIX = ('{"slot":"' + MARKER + '",').encode()
def slot(client='', secret=''):
    data = json.dumps({'slot': MARKER, 'client_id': client, 'client_secret': secret}, separators=(',', ':')).encode()
    if len(data) > SLOT:
        raise ValueError('Application configuration exceeds the reserved slot')
    return data + b' ' * (SLOT - len(data))
def configure_binary(binary, config):
    data = binary.read_bytes()
    # The source ROMFS file, not executable instructions, is changed.
    if data.count(PREFIX) != 1 or data[0x10:0x14] != b'NRO0':
        raise ValueError('Expected one SaveNX application configuration slot in a Switch NRO')
    offset = data.index(PREFIX)
    old = data[offset:offset + SLOT]
    if len(old) != SLOT or json.loads(old)['slot'] != MARKER:
        raise ValueError('Invalid ROMFS configuration slot')
    replacement = slot(config['client_id'], config.get('client_secret', ''))
    binary.write_bytes(data[:offset] + replacement + data[offset + SLOT:])
if __name__ == '__main__':
    path = Path('romfs/application/google-client.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(slot(os.environ.get('SAVENX_GOOGLE_CLIENT_ID', ''), os.environ.get('SAVENX_GOOGLE_CLIENT_SECRET', '')))
    print('Application configuration prepared; credential values omitted.')
