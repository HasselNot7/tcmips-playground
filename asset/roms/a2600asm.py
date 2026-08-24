#!/usr/bin/env python3
"""Minimal two-pass 6502 assembler for the TCMIPS Atari 2600 demo ROM."""
import re, sys

OPCODES = {
    'LDA': [('IMM',0xA9),('ZPX',0xB5),('ZP',0xA5),('INDY',0xB1),('INDX',0xA1),('ABSY',0xB9),('ABSX',0xBD),('ABS',0xAD)],
    'LDX': [('IMM',0xA2),('ZPY',0xB6),('ZP',0xA6),('ABSY',0xBE),('ABS',0xAE)],
    'LDY': [('IMM',0xA0),('ZPX',0xB4),('ZP',0xA4),('ABSX',0xBC),('ABS',0xAC)],
    'STA': [('ZPX',0x95),('ZP',0x85),('INDY',0x91),('INDX',0x81),('ABSY',0x99),('ABSX',0x9D),('ABS',0x8D)],
    'STX': [('ZPY',0x96),('ZP',0x86),('ABS',0x8E)],
    'STY': [('ZPX',0x94),('ZP',0x84),('ABS',0x8C)],
    'ADC': [('IMM',0x69),('ZPX',0x75),('ZP',0x65),('INDY',0x71),('INDX',0x61),('ABSY',0x79),('ABSX',0x7D),('ABS',0x6D)],
    'SBC': [('IMM',0xE9),('ZPX',0xF5),('ZP',0xE5),('INDY',0xF1),('INDX',0xE1),('ABSY',0xF9),('ABSX',0xFD),('ABS',0xED)],
    'AND': [('IMM',0x29),('ZPX',0x35),('ZP',0x25),('INDY',0x31),('INDX',0x21),('ABSY',0x39),('ABSX',0x3D),('ABS',0x2D)],
    'ORA': [('IMM',0x09),('ZPX',0x15),('ZP',0x05),('INDY',0x11),('INDX',0x01),('ABSY',0x19),('ABSX',0x1D),('ABS',0x0D)],
    'EOR': [('IMM',0x49),('ZPX',0x55),('ZP',0x45),('INDY',0x51),('INDX',0x41),('ABSY',0x59),('ABSX',0x5D),('ABS',0x4D)],
    'CMP': [('IMM',0xC9),('ZPX',0xD5),('ZP',0xC5),('INDY',0xD1),('INDX',0xC1),('ABSY',0xD9),('ABSX',0xDD),('ABS',0xCD)],
    'CPX': [('IMM',0xE0),('ZP',0xE4),('ABS',0xEC)],
    'CPY': [('IMM',0xC0),('ZP',0xC4),('ABS',0xCC)],
    'INC': [('ZPX',0xF6),('ZP',0xE6),('ABSX',0xFE),('ABS',0xEE)],
    'DEC': [('ZPX',0xD6),('ZP',0xC6),('ABSX',0xDE),('ABS',0xCE)],
    'BIT': [('ZP',0x24),('ABS',0x2C)],
    'ASL': [('ACC',0x0A),('ZPX',0x16),('ZP',0x06),('ABSX',0x1E),('ABS',0x0E)],
    'LSR': [('ACC',0x4A),('ZPX',0x56),('ZP',0x46),('ABSX',0x5E),('ABS',0x4E)],
    'ROL': [('ACC',0x2A),('ZPX',0x36),('ZP',0x26),('ABSX',0x3E),('ABS',0x2E)],
    'ROR': [('ACC',0x6A),('ZPX',0x76),('ZP',0x66),('ABSX',0x7E),('ABS',0x6E)],
    'JMP': [('ABS',0x4C),('IND',0x6C)],
    'JSR': [('ABS',0x20)],
    'BRK': [('IMP',0x00)], 'RTI': [('IMP',0x40)], 'RTS': [('IMP',0x60)],
    'CLC': [('IMP',0x18)], 'SEC': [('IMP',0x38)],
    'CLI': [('IMP',0x58)], 'SEI': [('IMP',0x78)],
    'CLV': [('IMP',0xB8)], 'CLD': [('IMP',0xD8)], 'SED': [('IMP',0xF8)],
    'NOP': [('IMP',0xEA)],
    'TAX': [('IMP',0xAA)], 'TXA': [('IMP',0x8A)],
    'TAY': [('IMP',0xA8)], 'TYA': [('IMP',0x98)],
    'TSX': [('IMP',0xBA)], 'TXS': [('IMP',0x9A)],
    'INX': [('IMP',0xE8)], 'DEX': [('IMP',0xCA)],
    'INY': [('IMP',0xC8)], 'DEY': [('IMP',0x88)],
    'PHA': [('IMP',0x48)], 'PLA': [('IMP',0x68)],
    'PHP': [('IMP',0x08)], 'PLP': [('IMP',0x28)],
    'BPL': [('REL',0x10)], 'BMI': [('REL',0x30)],
    'BVC': [('REL',0x50)], 'BVS': [('REL',0x70)],
    'BCC': [('REL',0x90)], 'BCS': [('REL',0xB0)],
    'BNE': [('REL',0xD0)], 'BEQ': [('REL',0xF0)],
}
MODE_LEN = {'IMP':1,'ACC':1,'IMM':2,'ZP':2,'ZPX':2,'ZPY':2,'ABS':3,'ABSX':3,'ABSY':3,'INDX':2,'INDY':2,'IND':3,'REL':2}

TOKEN_RE = re.compile(r'''
    (?P<hex>\$[0-9A-Fa-f]+)
  | (?P<num>\d+)
  | (?P<id>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<op><<|>>|\||&|\+|-|\*|/|\(|\)|,)
  | (?P<ws>\s+)
''', re.X)

class AsmError(Exception):
    pass

def parse_expr(s, labels, pc):
    s = s.strip()
    toks = []
    pos = 0
    while pos < len(s):
        m = TOKEN_RE.match(s, pos)
        if not m:
            raise AsmError('bad expr: %r' % s)
        pos = m.end()
        if m.group('ws'):
            continue
        if m.group('hex'):
            toks.append(str(int(m.group('hex')[1:], 16)))
        elif m.group('num'):
            toks.append(m.group('num'))
        elif m.group('id'):
            name = m.group('id')
            toks.append(str(labels[name]) if name in labels else '0')
        else:
            toks.append(m.group('op'))
    if not toks:
        raise AsmError('empty expr')
    expr = ' '.join(toks).replace('( ', '(').replace(' )', ')').replace(' ,', ',').replace(', ', ',')
    try:
        return int(eval(expr, {'__builtins__': {}}))
    except Exception as e:
        raise AsmError('expr eval failed: %r (%s)' % (expr, e))

def guess_mode(mn, operand):
    if operand == '':
        return 'IMP' if mn not in ('ASL','LSR','ROL','ROR') else 'ACC'
    if operand.startswith('#'):
        return 'IMM'
    if operand.startswith('('):
        return 'INDY' if operand.endswith(',Y)') else 'INDX'
    if ',' in operand:
        a, b = operand.split(',')
        if a.startswith('$') and len(a) <= 3:
            return 'ZPX' if b.upper() == 'X' else 'ZPY'
        return 'ABSX' if b.upper() == 'X' else 'ABSY'
    if operand.startswith('$') and len(operand) <= 3:
        return 'ZP'
    return 'ABS'

def emit_line(mn, operand, mode, labels, pc):
    modes = dict(OPCODES[mn])
    if mode not in modes:
        raise AsmError('%s has no %s mode' % (mn, mode))
    out = [modes[mode]]
    ln = MODE_LEN[mode]
    if mode in ('IMP','ACC'):
        return out, ln
    if mode in ('IMM','ZP','ZPX','ZPY'):
        v = parse_expr(operand[1:].strip() if mode == 'IMM' else operand, labels, pc)
        out.append(v & 0xFF)
        return out, ln
    if mode in ('INDX','INDY'):
        inner = operand.strip('()')
        if mode == 'INDY':
            inner = inner[:-2]
        v = parse_expr(inner, labels, pc)
        out.append(v & 0xFF)
        return out, ln
    if mode == 'IND':
        inner = operand.strip('()')
        v = parse_expr(inner, labels, pc)
        out.append(v & 0xFF)
        out.append((v >> 8) & 0xFF)
        return out, ln
    if mode == 'REL':
        tgt = parse_expr(operand, labels, pc)
        off = tgt - (pc + 2)
        if off < -128 or off > 127:
            raise AsmError('branch out of range @ %04X' % pc)
        out.append(off & 0xFF)
        return out, ln
    v = parse_expr(operand, labels, pc)
    out.append(v & 0xFF)
    out.append((v >> 8) & 0xFF)
    return out, ln

def assemble(lines):
    labels = {}
    entries = []  # (start_pc, mnemonic_or_directive, operand, mode, size)
    pc = 0
    for raw in lines:
        ln = raw.split(';')[0].strip()
        if not ln:
            continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', ln)
        if m:
            labels[m.group(1)] = pc
            ln = ln[m.end():].strip()
            if not ln:
                continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$', ln)
        if m:
            labels[m.group(1)] = parse_expr(m.group(2), labels, pc)
            continue
        parts = ln.replace(',', ' , ').split()
        mn = parts[0].upper()
        rest = ln[len(parts[0]):].strip()
        if mn == 'ORG':
            entries.append((pc, 'ORG', rest, None, 0))
            pc = parse_expr(rest, labels, pc)
            continue
        if mn == '.BYTE':
            toks = [t.strip() for t in rest.split(',') if t.strip()]
            entries.append((pc, '.BYTE', toks, None, len(toks)))
            pc += len(toks)
            continue
        if mn == '.WORD':
            entries.append((pc, '.WORD', rest, None, 2))
            pc += 2
            continue
        if mn not in OPCODES:
            raise AsmError('unknown mnemonic %s' % mn)
        if mn == 'JMP' and rest.startswith('('):
            mode = 'IND'
        elif mn in ('JMP','JSR'):
            mode = 'ABS'
        elif mn in ('BPL','BMI','BVC','BVS','BCC','BCS','BNE','BEQ'):
            mode = 'REL'
        else:
            mode = guess_mode(mn, rest)
            # Zero-page optimization: if the operand is already resolvable
            # (constant defined before use) and fits in one byte, prefer the
            # ZP form - it is one byte shorter and one cycle faster.
            if mode == 'ABS' and any(m == 'ZP' for m, _ in OPCODES[mn]):
                try:
                    v = parse_expr(rest, labels, pc)
                    if 0 <= v < 0x100:
                        mode = 'ZP'
                except AsmError:
                    pass
        entries.append((pc, mn, rest, mode, MODE_LEN[mode]))
        pc += MODE_LEN[mode]

    out = []
    cur = 0
    for start, mn, operand, mode, size in entries:
        while cur < start:
            out.append(0)
            cur += 1
        if mn == 'ORG':
            continue
        if mn == '.BYTE':
            for tok in operand:
                out.append(parse_expr(tok, labels, start) & 0xFF)
            cur += len(operand)
        elif mn == '.WORD':
            v = parse_expr(operand, labels, start)
            out.append(v & 0xFF)
            out.append((v >> 8) & 0xFF)
            cur += 2
        else:
            bytes_, ln = emit_line(mn, operand, mode, labels, start)
            out.extend(bytes_)
            cur += ln
    return bytes(out)

if __name__ == '__main__':
    data = assemble(open(sys.argv[1]).read().splitlines())
    sys.stdout.buffer.write(data)