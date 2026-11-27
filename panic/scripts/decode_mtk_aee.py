"""Decode the Oplus MediaTek AEE layout verified on the PGZ110 / MT6895 dump.

Preparation and decryption depend on this AEE container variant. Report export
is restricted to the verified 2026-11-26 16:58:35 sample and its printk layout.
See ../docs/mediatek_aee_guide.md for prerequisites, commands and limitations.
"""

import argparse
import hashlib
import io
import json
import re
import struct
import subprocess
import tarfile
import zlib
from pathlib import Path


SAMPLE_SHA256 = '4387889dab626570b718ac17c83a13c88ec0ebd97cf375f4c28d69e9d35ac253'


def read_members(archive):
    inflater = zlib.decompressobj(31)
    outer = inflater.decompress(archive.read_bytes())
    print('GZIP_COMPLETE', inflater.eof)
    with tarfile.open(fileobj=io.BytesIO(outer), mode='r:') as package:
        for member in package:
            if member.name.endswith('.dbg'):
                raw = outer[member.offset_data:member.offset_data + member.size]
                print('DBG_SIZE', member.size, 'AVAILABLE', len(raw))
                break
        else:
            raise ValueError('AEE .dbg member missing')
    if len(raw) < 256:
        raise ValueError('AEE header is incomplete')
    key = raw[64:80]
    restored = bytes(value ^ key[index % 16] for index, value in enumerate(raw[:256])) + raw[256:]
    if not restored.startswith(b'PK\x03\x04'):
        raise ValueError('Unsupported AEE header layout')
    offset = 0
    members = []
    names = set()
    while restored[offset:offset + 4] == b'PK\x03\x04':
        if offset + 30 > len(restored):
            print('TRUNCATED_LOCAL_HEADER', offset)
            break
        fields = struct.unpack_from('<4s5H3I2H', restored, offset)
        name_length, extra_length = fields[-2:]
        start = offset + 30 + name_length + extra_length
        end = start + fields[7]
        if end > len(restored):
            print('TRUNCATED_MEMBER_AT', offset)
            break
        if fields[2] != 0x7 or fields[3] != 8:
            raise ValueError('Unsupported ZIP flags or compression method')
        filename = restored[offset + 30:offset + 30 + name_length].decode('ascii')
        chunks = re.split('lneO|taZP|dxSA', filename)
        if len(chunks) < 3:
            raise ValueError('Unsupported AEE member name: ' + filename)
        decoded_name = ''.join(chunks[column][row] for row in range(max(map(len, chunks[:3])))
                               for column in range(3) if row < len(chunks[column]))[::-1]
        if not re.fullmatch(r'[A-Za-z0-9_.-]+', decoded_name) or decoded_name in ('.', '..'):
            raise ValueError('Unsafe member name')
        if decoded_name in names:
            raise ValueError('Duplicate member name: ' + decoded_name)
        names.add(decoded_name)
        members.append((decoded_name, fields, restored[start:end]))
        print('MEMBER', decoded_name, 'FLAGS', hex(fields[2]), 'METHOD', fields[3],
              'CRC', hex(fields[6]), 'RAW_SIZE', fields[8], 'STORED_SIZE', fields[7])
        offset = end
    if not members:
        raise ValueError('No complete encrypted members found')
    return members


def prepare_members(archive, directory):
    members = read_members(archive)
    directory.mkdir(parents=True, exist_ok=False)
    manifest = []
    known_plaintext = False
    for name, fields, payload in members:
        (directory / (name + '.cipher')).write_bytes(payload)
        manifest.append(dict(name=name, crc=fields[6], size=fields[8]))
        if name == 'SYS_EXTRA_CCCI_RAW' and fields[8] == 32768:
            plain = bytes(fields[8])
            compressor = zlib.compressobj(1, zlib.DEFLATED, -15)
            compressed = compressor.compress(plain) + compressor.flush()
            if zlib.crc32(plain) == fields[6] and len(compressed) == len(payload) - 12:
                (directory / 'known.deflate').write_bytes(compressed)
                known_plaintext = True
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('PREPARED_COMPLETE_MEMBERS', len(manifest))
    print('KNOWN_PLAINTEXT_CANDIDATE_READY', known_plaintext)
    if not known_plaintext:
        print('No matching all-zero CCCI candidate; do not reuse another archive\'s keys.')


def read_manifest(directory):
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    names = set()
    if not manifest:
        raise ValueError('Empty member manifest')
    for member in manifest:
        name = member['name']
        if not re.fullmatch(r'[A-Za-z0-9_.-]+', name) or name in ('.', '..') or name in names:
            raise ValueError('Unsafe or duplicate member name')
        names.add(name)
    return manifest


def decrypt_members(directory, tool, recovery):
    key_matches = re.findall(r'^([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8})\s*$',
                             recovery.read_text(encoding='utf-8-sig'), re.MULTILINE)
    if not key_matches:
        raise ValueError('Recovered keys missing from UTF-8 bkcrack log')
    manifest = read_manifest(directory)
    output = directory.parent / 'decoded'
    output.mkdir(exist_ok=False)
    total_bytes = 0
    for member in manifest:
        name = member['name']
        intermediate = output / (name + '.deflate')
        subprocess.run([str(tool), '-c', str(directory / (name + '.cipher')),
                        '-k', *key_matches[-1], '-d', str(intermediate)],
                       check=True, capture_output=True)
        inflater = zlib.decompressobj(-15)
        plain = inflater.decompress(intermediate.read_bytes(), member['size'] + 1)
        if not inflater.eof or inflater.unused_data or len(plain) != member['size'] or zlib.crc32(plain) != member['crc']:
            raise ValueError('Decryption verification failed: ' + name)
        (output / name).write_bytes(plain)
        intermediate.unlink()
        total_bytes += len(plain)
        print('VERIFIED', name, 'SIZE', len(plain), 'CRC', format(member['crc'], '08x'))
    print('ALL_COMPLETE_MEMBERS_VERIFIED', len(manifest), 'BYTES', total_bytes)


def export_report(archive, directory, output):
    archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
    if archive_hash != SAMPLE_SHA256:
        raise ValueError('Report layout is sample-specific; this archive has not been validated')
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN

    decoded = directory.parent / 'decoded'
    manifest = read_manifest(directory)
    for member in manifest:
        plain = (decoded / member['name']).read_bytes()
        if len(plain) != member['size'] or zlib.crc32(plain) != member['crc']:
            raise ValueError('Decoded member verification failed: ' + member['name'])
        member['sha256'] = hashlib.sha256(plain).hexdigest()
    metadata = (decoded / 'SYS_KERNEL_LOG_TP_RAW').read_bytes()
    count_bits = struct.unpack_from('<I', metadata, 0)[0]
    data_bits = struct.unpack_from('<I', metadata, 48)[0]
    head_id, tail_id = struct.unpack_from('<QQ', metadata, 24)
    head_position, tail_position = struct.unpack_from('<QQ', metadata, 64)
    infos = (decoded / 'SYS_KERNEL_LOG_INFOS_RAW').read_bytes()
    descs = (decoded / 'SYS_KERNEL_LOG_DESCS_RAW').read_bytes()
    data = (decoded / 'SYS_KERNEL_LOG_DATA_RAW').read_bytes()
    if len(infos) != (1 << count_bits) * 88 or len(descs) != (1 << count_bits) * 24 or len(data) != 1 << data_bits:
        raise ValueError('Unexpected printk ring layout')
    records = []
    for index in range(1 << count_bits):
        state, begin, end = struct.unpack_from('<QQQ', descs, index * 24)
        descriptor_id = state & ((1 << 62) - 1)
        if state >> 62 not in (1, 2) or not tail_id <= descriptor_id <= head_id:
            continue
        if not tail_position <= begin < end <= head_position or end - begin > len(data):
            continue
        seq, timestamp, text_length, facility, flags, caller = struct.unpack_from('<QQHBBI', infos, index * 88)
        position = begin % len(data)
        if position + end - begin > len(data):
            position = 0
        block_id = struct.unpack_from('<Q', data, position)[0]
        if block_id != descriptor_id or text_length > 4096 or position + 8 + text_length > len(data):
            continue
        text = data[position + 8:position + 8 + text_length].decode('utf-8', errors='replace')
        records.append((seq, timestamp, caller, text))
    records.sort()
    if len({record[0] for record in records}) != len(records):
        raise ValueError('Duplicate printk sequence numbers')
    panic_records = [record for record in records if 'Unable to handle kernel NULL' in record[3]]
    if len(panic_records) != 1:
        raise ValueError('Expected exactly one NULL panic in the sample')
    panic_time = panic_records[0][1]
    selected = [record for record in records if panic_time - 100_000_000 <= record[1] <= panic_time + 50_000_000]
    lines = ['[%12.6f] [seq=%d caller=%d] %s' % (timestamp / 1e9, seq, caller, text)
             for seq, timestamp, caller, text in records]
    panic_lines = ['[%12.6f] [seq=%d caller=%d] %s' % (timestamp / 1e9, seq, caller, text)
                   for seq, timestamp, caller, text in selected]
    panic_text = '\n'.join(panic_lines) + '\n'
    pstore = (decoded / 'SYS_PSTORE_RAW').read_bytes()
    if b'lr : [0xffffffde41b4a63c]' not in pstore or 'lr : [0xffffffde41b4a63c]' not in panic_text:
        raise ValueError('Pstore and printk fault locations do not match the sample')
    matches = re.findall(r'^.*? a630: ((?:[0-9a-f]{8} ){7}[0-9a-f]{8})', panic_text, re.MULTILINE)
    if len(matches) != 1:
        raise ValueError('Expected one fault instruction block')
    words = [int(word, 16) for word in matches[0].split()]
    instructions = list(Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN).disasm(struct.pack('<8I', *words), 0xffffffde41b4a630))
    if len(instructions) != 8 or instructions[2].address != 0xffffffde41b4a638 or instructions[2].mnemonic != 'blr' or instructions[2].op_str != 'x8':
        raise ValueError('Fault instruction does not match BLR X8')
    disassembly = '\n'.join('0x%x: %s %s' % (instruction.address, instruction.mnemonic, instruction.op_str)
                            for instruction in instructions) + '\n'
    task_data = (decoded / 'PROC_CUR_TSK').read_bytes()
    task_text = b'\n'.join(match.group() for match in re.finditer(rb'[\x20-\x7e\n\t]{8,}', task_data)).decode('ascii')
    output.mkdir(parents=True, exist_ok=False)
    (output / 'kernel.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (output / 'panic.log').write_text(panic_text, encoding='utf-8')
    (output / 'current-task.txt').write_text(task_text + '\n', encoding='utf-8')
    (output / 'fault-call.txt').write_text(disassembly, encoding='utf-8')
    verification = dict(archive=str(archive), archive_sha256=archive_hash,
                        archive_complete=False, declared_dbg_bytes=12216917,
                        available_dbg_bytes=10372073, verified_members=manifest,
                        recovered_record_count=len(records),
                        decoded_members_directory=str(decoded),
                        recovery_tool='bkcrack (validated with 1.8.1)',
                        recovery_method='Known compressed plaintext; full member length and CRC32 verified')
    (output / 'verification.json').write_text(json.dumps(verification, indent=2), encoding='utf-8')
    print('REPORT_EXPORTED', output)
    print('VERIFIED_MEMBERS', len(manifest), 'ORDERED_RECORDS', len(records), 'PANIC_RECORDS', len(selected))
    print(disassembly)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inspect = commands.add_parser('inspect', help='List complete members without writing files')
    inspect.add_argument('archive', type=Path)
    prepare = commands.add_parser('prepare', help='Export ciphertext, manifest and matching known-plaintext candidate')
    prepare.add_argument('archive', type=Path)
    prepare.add_argument('directory', type=Path, help='New member directory; existing directories are not overwritten')
    decrypt = commands.add_parser('decrypt', help='Decrypt with recovered keys and verify every member')
    decrypt.add_argument('directory', type=Path, help='Member directory created by prepare')
    decrypt.add_argument('tool', type=Path, help='Path to an independently installed bkcrack executable')
    decrypt.add_argument('recovery', type=Path, help='UTF-8 bkcrack recovery log containing the three internal keys')
    report = commands.add_parser('report', help='Export the verified PGZ110 sample only; requires Capstone')
    report.add_argument('archive', type=Path)
    report.add_argument('directory', type=Path, help='Member directory with a sibling decoded directory')
    report.add_argument('output', type=Path, help='New report directory')
    args = parser.parse_args()
    try:
        if args.command == 'inspect':
            read_members(args.archive)
        elif args.command == 'prepare':
            prepare_members(args.archive, args.directory)
        elif args.command == 'decrypt':
            decrypt_members(args.directory, args.tool, args.recovery)
        else:
            export_report(args.archive, args.directory, args.output)
    except (OSError, ValueError, KeyError, struct.error, zlib.error, tarfile.TarError,
            subprocess.CalledProcessError, ImportError) as error:
        parser.exit(1, 'ERROR: ' + str(error) + '\n')


if __name__ == '__main__':
    main()