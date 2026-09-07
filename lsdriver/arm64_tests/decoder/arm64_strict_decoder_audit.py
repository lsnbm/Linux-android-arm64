import argparse
import csv
import re
from collections import Counter

FIELDS = (
    "offset", "immediate", "bitfield_wmask", "bitfield_tmask",
    "extend_type", "shift_type", "shift_amount", "immr", "imms",
    "simd_cmode",
    "condition", "nzcv", "sysreg", "element_width",
    "lane_index", "rd", "rn", "rm", "ra", "rt",
    "rt2", "rs", "operand_width",
)

LLVM_DIRECT_FIELDS = {
    "offset", "immediate", "condition", "lane_index",
    "rd", "rn", "rm", "ra", "rt", "rt2", "rs",
}


def bits(raw, high, low=None):
    if low is None:
        low = high
    return (raw >> low) & ((1 << (high - low + 1)) - 1)


def signed(value, width):
    sign = 1 << (width - 1)
    return value - (1 << width) if value & sign else value


def low_mask(width):
    return (1 << width) - 1 if width < 64 else (1 << 64) - 1


def replicate(value, element_width, width):
    value &= low_mask(element_width)
    result = 0
    for offset in range(0, width, element_width):
        result |= value << offset
    return result & low_mask(width)


def ror(value, rotation, width):
    rotation %= width
    value &= low_mask(width)
    return value if not rotation else ((value >> rotation) | (value << (width - rotation))) & low_mask(width)


def decode_bit_masks(n, immr, imms, width, immediate):
    value = (n << 6) | (~imms & 0x3F)
    if not value:
        return None
    length = value.bit_length() - 1
    if length < 1 or (width == 32 and length == 6):
        return None
    levels = (1 << length) - 1
    s = imms & levels
    r = immr & levels
    if immediate and s == levels:
        return None
    element_width = 1 << length
    wmask = replicate(ror(low_mask(s + 1), r, element_width), element_width, width)
    if immediate:
        return wmask, 0
    diff = (s - r) & levels
    return wmask, replicate(low_mask(diff + 1), element_width, width)


def instruction_names(header_path):
    names = []
    inside = False
    with open(header_path, encoding="utf-8") as header:
        for line in header:
            if line.startswith("enum arm64_instruction"):
                inside = True
                continue
            if inside and line.startswith("};"):
                break
            if inside:
                match = re.search(r"\b(ARM64_INST_[A-Z0-9_]+)\s*,", line)
                if match:
                    names.append(match.group(1))
    return names


def non_negative_int(value):
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return parsed


def load_identity_pairs(path):
    with open(path, newline="", encoding="utf-8") as mapping_file:
        rows = list(csv.DictReader(mapping_file, delimiter="\t"))
    return {(normalize_instruction_name(row["instruction"].strip()), row["llvm_opcode"].strip()) for row in rows}


def llvm_operands(row):
    result = []
    for token in row.get("operands", "").split(";"):
        if token.startswith("r:"):
            register = token[2:]
            match = re.search(r"(?:[WXSQDHBVZP])([0-9]+)$", register)
            if match:
                result.append(("reg", int(match.group(1))))
            elif register in {"FP", "WFP"}:
                result.append(("reg", 29))
            elif register in {"LR", "WLR"}:
                result.append(("reg", 30))
            elif register in {"SP", "WSP", "XSP", "WZR", "XZR"}:
                result.append(("reg", 31))
        elif token.startswith("i:"):
            result.append(("imm", int(token[2:], 0)))
    return result


def llvm_assembly_immediates(row):
    values = []
    for text in re.findall(r"#(-?(?:0x[0-9a-fA-F]+|[0-9]+))", row.get("assembly", "")):
        values.append(int(text, 0))
    return values


def llvm_project_fields(decoder_row, llvm_row, name):
    operands = llvm_operands(llvm_row)
    registers = [value for kind, value in operands if kind == "reg"]
    immediates = [value for kind, value in operands if kind == "imm"]
    assembly_immediates = llvm_assembly_immediates(llvm_row)
    opcode = llvm_row["opcode"]
    projected = {}

    def regs(*fields):
        for field, value in zip(fields, registers):
            projected[field] = value

    def first_immediate(field):
        if immediates:
            projected[field] = immediates[0]

    def scaled_immediate(field, scale, index=0):
        if len(immediates) > index:
            projected[field] = immediates[index] * scale

    def pair_scale():
        if opcode.startswith(("LDPQ", "STPQ")):
            return 16
        if opcode.startswith(("LDPD", "STPD")):
            return 8
        if opcode.startswith(("LDPS", "STPS", "LDPW", "STPW")):
            return 4
        return 8 if opcode.startswith(("LDPX", "STPX", "LDNPX", "STNPX")) else 4

    if name in {"ARM64_INST_B", "ARM64_INST_BL"}:
        scaled_immediate("offset", 4)
    elif name == "ARM64_INST_B_COND":
        if immediates:
            projected["condition"] = immediates[0]
        scaled_immediate("offset", 4, 1)
    elif name in {"ARM64_INST_CBZ", "ARM64_INST_CBNZ"}:
        regs("rt")
        scaled_immediate("offset", 4)
    elif name in {"ARM64_INST_TBZ", "ARM64_INST_TBNZ"}:
        regs("rt")
        if immediates:
            projected["immediate"] = immediates[0]
        scaled_immediate("offset", 4, 1)
    elif name in {"ARM64_INST_ADR", "ARM64_INST_ADRP"}:
        regs("rd")
        scaled_immediate("offset", 4096 if name == "ARM64_INST_ADRP" else 1)
    elif name in {"ARM64_INST_BR", "ARM64_INST_BLR", "ARM64_INST_RET"}:
        regs("rn")
    elif name in {"ARM64_INST_ADD_IMMEDIATE", "ARM64_INST_ADDS_IMMEDIATE",
                  "ARM64_INST_SUB_IMMEDIATE", "ARM64_INST_SUBS_IMMEDIATE",
                  "ARM64_INST_AND_IMMEDIATE", "ARM64_INST_ORR_IMMEDIATE",
                  "ARM64_INST_EOR_IMMEDIATE"}:
        regs("rd", "rn")
        if name in {"ARM64_INST_AND_IMMEDIATE", "ARM64_INST_ORR_IMMEDIATE", "ARM64_INST_EOR_IMMEDIATE"}:
            if assembly_immediates:
                projected["immediate"] = assembly_immediates[0]
        else:
            first_immediate("immediate")
    elif name in {"ARM64_INST_ADD_SHIFTED_REGISTER", "ARM64_INST_ADDS_SHIFTED_REGISTER",
                  "ARM64_INST_SUB_SHIFTED_REGISTER", "ARM64_INST_SUBS_SHIFTED_REGISTER",
                  "ARM64_INST_AND_SHIFTED_REGISTER", "ARM64_INST_BIC_SHIFTED_REGISTER",
                  "ARM64_INST_ORR_SHIFTED_REGISTER", "ARM64_INST_ORN_SHIFTED_REGISTER",
                  "ARM64_INST_EOR_SHIFTED_REGISTER", "ARM64_INST_EON_SHIFTED_REGISTER",
                  "ARM64_INST_ANDS_SHIFTED_REGISTER", "ARM64_INST_BICS_SHIFTED_REGISTER"}:
        regs("rd", "rn", "rm")
    elif name in {"ARM64_INST_MADD", "ARM64_INST_MSUB", "ARM64_INST_SMADDL",
                  "ARM64_INST_UMADDL", "ARM64_INST_SMSUBL", "ARM64_INST_UMSUBL"}:
        regs("rd", "rn", "rm", "ra")
    elif name in {"ARM64_INST_LDP_GPR_OFFSET", "ARM64_INST_LDP_GPR_POST_INDEX",
                  "ARM64_INST_LDP_GPR_PRE_INDEX", "ARM64_INST_STP_GPR_OFFSET",
                  "ARM64_INST_STP_GPR_POST_INDEX", "ARM64_INST_STP_GPR_PRE_INDEX",
                  "ARM64_INST_LDNP_GPR", "ARM64_INST_STNP_GPR"}:
        if name.endswith(("POST_INDEX", "PRE_INDEX")) and len(registers) >= 4:
            projected["rt"] = registers[1]
            projected["rt2"] = registers[2]
            projected["rn"] = registers[0]
        else:
            regs("rt", "rt2", "rn")
        scaled_immediate("offset", pair_scale())
    elif name in {"ARM64_INST_LDP_FP_SIMD_OFFSET", "ARM64_INST_LDP_FP_SIMD_POST_INDEX",
                  "ARM64_INST_LDP_FP_SIMD_PRE_INDEX", "ARM64_INST_STP_FP_SIMD_OFFSET",
                  "ARM64_INST_STP_FP_SIMD_PRE_INDEX"}:
        if name.endswith(("POST_INDEX", "PRE_INDEX")) and len(registers) >= 4:
            projected["rt"] = registers[1]
            projected["rt2"] = registers[2]
            projected["rn"] = registers[0]
        else:
            regs("rt", "rt2", "rn")
        scaled_immediate("offset", pair_scale())
    elif ("_GPR_" in name or "_FP_SIMD_" in name) and any(
            suffix in name for suffix in ("OFFSET", "INDEX", "REGISTER_OFFSET")):
        if name.endswith(("POST_INDEX", "PRE_INDEX")) and len(registers) >= 3:
            projected["rt"] = registers[1]
            projected["rn"] = registers[0]
        else:
            regs("rt", "rn")
        if "REGISTER_OFFSET" in name:
            if len(registers) > 2:
                projected["rm"] = registers[2]
        else:
            scale = 1
            if opcode.endswith(("pre", "post")):
                scale = 1
            elif opcode.startswith(("LDRX", "STRX")) and opcode.endswith("ui"):
                scale = 8
            elif opcode.startswith(("LDRW", "STRW", "LDRSW")) and opcode.endswith("ui"):
                scale = 4
            elif opcode.startswith(("LDRH", "STRH")) and opcode.endswith("ui"):
                scale = 2
            elif opcode.startswith(("LDRB", "STRB")) and opcode.endswith("ui"):
                scale = 1
            elif opcode.endswith(("Sui", "Spre", "Spost")):
                scale = 4
            elif opcode.endswith(("Dui", "Dpre", "Dpost")):
                scale = 8
            elif opcode.endswith(("Qui", "Qpre", "Qpost")):
                scale = 16
            scaled_immediate("offset", scale)
    elif name in {"ARM64_INST_FADD_SCALAR", "ARM64_INST_FSUB_SCALAR",
                  "ARM64_INST_FMUL_SCALAR", "ARM64_INST_FDIV_SCALAR",
                  "ARM64_INST_FMAX_SCALAR", "ARM64_INST_FMIN_SCALAR",
                  "ARM64_INST_FMAXNM_SCALAR", "ARM64_INST_FMINNM_SCALAR",
                  "ARM64_INST_FNMUL_SCALAR", "ARM64_INST_FADD_VECTOR",
                  "ARM64_INST_FSUB_VECTOR", "ARM64_INST_FMUL_VECTOR",
                  "ARM64_INST_FDIV_VECTOR", "ARM64_INST_FMLA_VECTOR",
                  "ARM64_INST_FCMGT_VECTOR", "ARM64_INST_ORR_VECTOR",
                  "ARM64_INST_BIT_VECTOR", "ARM64_INST_BSL_VECTOR"}:
        if len(registers) >= 4 and name.endswith("_VECTOR"):
            projected["rd"] = registers[0]
            projected["rn"] = registers[2]
            projected["rm"] = registers[3]
        else:
            regs("rd", "rn", "rm")
    elif name in {"ARM64_INST_FMOV_SCALAR", "ARM64_INST_FABS_SCALAR",
                  "ARM64_INST_FNEG_SCALAR", "ARM64_INST_FSQRT_SCALAR",
                  "ARM64_INST_FRINTM_VECTOR"}:
        regs("rd", "rn")
    elif name in {"ARM64_INST_DUP_ELEMENT_VECTOR", "ARM64_INST_DUP_GENERAL_VECTOR",
                  "ARM64_INST_UMOV_VECTOR_TO_GPR", "ARM64_INST_SMOV_VECTOR_TO_GPR",
                  "ARM64_INST_INS_GPR_VECTOR"}:
        if name == "ARM64_INST_INS_GPR_VECTOR" and len(registers) >= 3:
            projected["rd"] = registers[0]
            projected["rn"] = registers[2]
        else:
            regs("rd", "rn")
        if name == "ARM64_INST_INS_GPR_VECTOR":
            first_immediate("lane_index")
    elif name in {"ARM64_INST_FCCMP_SCALAR", "ARM64_INST_FCCMPE_SCALAR"}:
        regs("rn", "rm")
        if len(immediates) >= 2:
            projected["immediate"] = immediates[0]
            projected["condition"] = immediates[1]
    return projected


def audit_llvm_projected_fields(rows, llvm_rows, names):
    failures = []
    covered = Counter()
    for decoder_row, llvm_row in zip(rows, llvm_rows):
        name = names[int(decoder_row["instruction"], 0)]
        projected = llvm_project_fields(decoder_row, llvm_row, name)
        for field, expected in projected.items():
            covered[field] += 1
            actual = int(decoder_row[field], 0)
            if actual != expected:
                failures.append((decoder_row["index"], name, field, actual, expected, llvm_row["opcode"]))
    return failures, covered


def normalize_instruction_name(name):
    replacements = {
        "ARM64_INST_LD1_SINGLE_STRUCTURE_S": "ARM64_INST_LD1",
        "ARM64_INST_LDADDAL_X": "ARM64_INST_LDADDAL",
        "ARM64_INST_LDAPR_X": "ARM64_INST_LDAPR",
        "ARM64_INST_LDAR_X": "ARM64_INST_LDAR",
        "ARM64_INST_LDNPX_GPR": "ARM64_INST_LDNP_GPR",
        "ARM64_INST_LDPX_GPR_OFFSET": "ARM64_INST_LDP_GPR_OFFSET",
        "ARM64_INST_LDPX_GPR_POST_INDEX": "ARM64_INST_LDP_GPR_POST_INDEX",
        "ARM64_INST_LDPD_FP_SIMD_OFFSET": "ARM64_INST_LDP_FP_SIMD_OFFSET",
        "ARM64_INST_LDPQ_FP_SIMD_OFFSET": "ARM64_INST_LDP_FP_SIMD_OFFSET",
        "ARM64_INST_LDPS_FP_SIMD_OFFSET": "ARM64_INST_LDP_FP_SIMD_OFFSET",
        "ARM64_INST_LDPD_FP_SIMD_POST_INDEX": "ARM64_INST_LDP_FP_SIMD_POST_INDEX",
        "ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET": "ARM64_INST_LDR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET": "ARM64_INST_LDR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_LDRW_GPR_UNSIGNED_OFFSET": "ARM64_INST_LDR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_LDRX_GPR_UNSIGNED_OFFSET": "ARM64_INST_LDR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_LDRX_GPR_REGISTER_OFFSET": "ARM64_INST_LDR_GPR_REGISTER_OFFSET",
        "ARM64_INST_LDRX_LITERAL_GPR": "ARM64_INST_LDR_GPR_LITERAL",
        "ARM64_INST_LDRSW_X_GPR_UNSIGNED_OFFSET": "ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_LDTRX_GPR": "ARM64_INST_LDTR_GPR",
        "ARM64_INST_LDURD_FP_SIMD": "ARM64_INST_LDUR_FP_SIMD",
        "ARM64_INST_LDURS_FP_SIMD": "ARM64_INST_LDUR_FP_SIMD",
        "ARM64_INST_LDURX_GPR": "ARM64_INST_LDUR_GPR",
        "ARM64_INST_LDXRX": "ARM64_INST_LDXR",
        "ARM64_INST_STLR_X": "ARM64_INST_STLR",
        "ARM64_INST_STNPX_GPR": "ARM64_INST_STNP_GPR",
        "ARM64_INST_STPX_GPR_OFFSET": "ARM64_INST_STP_GPR_OFFSET",
        "ARM64_INST_STPX_GPR_PRE_INDEX": "ARM64_INST_STP_GPR_PRE_INDEX",
        "ARM64_INST_STPD_FP_SIMD_OFFSET": "ARM64_INST_STP_FP_SIMD_OFFSET",
        "ARM64_INST_STPQ_FP_SIMD_OFFSET": "ARM64_INST_STP_FP_SIMD_OFFSET",
        "ARM64_INST_STPS_FP_SIMD_OFFSET": "ARM64_INST_STP_FP_SIMD_OFFSET",
        "ARM64_INST_STPD_FP_SIMD_PRE_INDEX": "ARM64_INST_STP_FP_SIMD_PRE_INDEX",
        "ARM64_INST_STRD_FP_SIMD_PRE_INDEX": "ARM64_INST_STR_FP_SIMD_PRE_INDEX",
        "ARM64_INST_STRQ_FP_SIMD_PRE_INDEX": "ARM64_INST_STR_FP_SIMD_PRE_INDEX",
        "ARM64_INST_STRD_FP_SIMD_UNSIGNED_OFFSET": "ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET",
        "ARM64_INST_STRQ_FP_SIMD_UNSIGNED_OFFSET": "ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET",
        "ARM64_INST_STRS_FP_SIMD_UNSIGNED_OFFSET": "ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET",
        "ARM64_INST_STRX_GPR_PRE_INDEX": "ARM64_INST_STR_GPR_PRE_INDEX",
        "ARM64_INST_STRX_GPR_REGISTER_OFFSET": "ARM64_INST_STR_GPR_REGISTER_OFFSET",
        "ARM64_INST_STRB_GPR_UNSIGNED_OFFSET": "ARM64_INST_STR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_STRH_GPR_UNSIGNED_OFFSET": "ARM64_INST_STR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_STRW_GPR_UNSIGNED_OFFSET": "ARM64_INST_STR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_STRX_GPR_UNSIGNED_OFFSET": "ARM64_INST_STR_GPR_UNSIGNED_OFFSET",
        "ARM64_INST_STTRX_GPR": "ARM64_INST_STTR_GPR",
        "ARM64_INST_STURD_FP_SIMD": "ARM64_INST_STUR_FP_SIMD",
        "ARM64_INST_STURS_FP_SIMD": "ARM64_INST_STUR_FP_SIMD",
        "ARM64_INST_STURX_GPR": "ARM64_INST_STUR_GPR",
        "ARM64_INST_STXRX": "ARM64_INST_STXR",
        "ARM64_INST_CASALX": "ARM64_INST_CASAL",
        "ARM64_INST_CASPALX": "ARM64_INST_CASPAL",
    }
    return replacements.get(name, name)


def expected_instruction_class(raw):
    owner = bits(raw, 28, 25)
    classes = {
        4: 3,
        5: 4,
        6: 3,
        7: 5,
        8: 6,
        9: 6,
        10: 7,
        11: 7,
        12: 3,
        13: 4,
        14: 3,
        15: 5,
    }
    return classes.get(owner)


def audit_llvm_rows(rows, llvm_rows, names, identity_path):
    expected_pairs = load_identity_pairs(identity_path)
    failures = []
    observed_pairs = set()
    if len(rows) != len(llvm_rows):
        failures.append(("rows", len(llvm_rows), len(rows)))
    for index, (decoder_row, llvm_row) in enumerate(zip(rows, llvm_rows)):
        name = names[int(decoder_row["instruction"], 0)]
        raw = int(decoder_row["raw"], 0)
        pair = (normalize_instruction_name(name.strip()), llvm_row["opcode"].strip())
        observed_pairs.add(pair)
        if int(llvm_row["index"]) != index or int(decoder_row["index"]) != index:
            failures.append((index, "index", llvm_row["index"], index))
        if int(llvm_row["input_raw"], 16) != raw:
            failures.append((index, "input_raw", llvm_row["input_raw"], decoder_row["raw"]))
        if llvm_row["decode_status"] != "success":
            failures.append((index, "decode_status", llvm_row["decode_status"], "success"))
        if int(llvm_row["decode_size"]) != 4:
            failures.append((index, "decode_size", llvm_row["decode_size"], 4))
        if llvm_row["encoded_raw"] != llvm_row["input_raw"]:
            failures.append((index, "encoded_raw", llvm_row["encoded_raw"], llvm_row["input_raw"]))
        if llvm_row["fixups"] != "0":
            failures.append((index, "fixups", llvm_row["fixups"], 0))
        if llvm_row["identity"] != "1":
            failures.append((index, "identity", llvm_row["identity"], 1))
        if pair not in expected_pairs:
            failures.append((index, "identity_pair", pair, "allowlist"))
    for pair in sorted(expected_pairs - observed_pairs):
        failures.append(("missing_pair", pair, "observed"))
    for pair in sorted(observed_pairs - expected_pairs):
        failures.append(("unexpected_pair", pair, "allowlist"))
    return failures, expected_pairs, observed_pairs


def audit_row(row, name):
    raw = int(row["raw"], 0)
    failures = []
    covered = set()

    def expect(field, value):
        covered.add(field)
        actual = int(row[field], 0)
        if actual != value:
            failures.append((field, actual, value))

    def gpr3():
        expect("rd", bits(raw, 4, 0))
        expect("rn", bits(raw, 9, 5))
        expect("rm", bits(raw, 20, 16))
        expect("operand_width", 64 if bits(raw, 31) else 32)

    shifted = {
        "ARM64_INST_AND_SHIFTED_REGISTER", "ARM64_INST_BIC_SHIFTED_REGISTER",
        "ARM64_INST_ORR_SHIFTED_REGISTER", "ARM64_INST_ORN_SHIFTED_REGISTER",
        "ARM64_INST_EOR_SHIFTED_REGISTER", "ARM64_INST_EON_SHIFTED_REGISTER",
        "ARM64_INST_ANDS_SHIFTED_REGISTER", "ARM64_INST_BICS_SHIFTED_REGISTER",
        "ARM64_INST_ADD_SHIFTED_REGISTER", "ARM64_INST_ADDS_SHIFTED_REGISTER",
        "ARM64_INST_SUB_SHIFTED_REGISTER", "ARM64_INST_SUBS_SHIFTED_REGISTER",
    }

    if name in {"ARM64_INST_ADR", "ARM64_INST_ADRP"}:
        imm21 = (bits(raw, 23, 5) << 2) | bits(raw, 30, 29)
        expect("rd", bits(raw, 4, 0))
        expect("offset", signed(imm21 << (12 if name.endswith("ADRP") else 0), 33 if name.endswith("ADRP") else 21))
        expect("operand_width", 64)
    elif name in {"ARM64_INST_B", "ARM64_INST_BL"}:
        expect("offset", signed(bits(raw, 25, 0) << 2, 28))
    elif name in {"ARM64_INST_CBZ", "ARM64_INST_CBNZ"}:
        expect("rt", bits(raw, 4, 0)); expect("offset", signed(bits(raw, 23, 5) << 2, 21))
        expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name in {"ARM64_INST_TBZ", "ARM64_INST_TBNZ"}:
        expect("rt", bits(raw, 4, 0)); expect("immediate", (bits(raw, 31) << 5) | bits(raw, 23, 19))
        expect("offset", signed(bits(raw, 18, 5) << 2, 16)); expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name == "ARM64_INST_B_COND":
        expect("condition", bits(raw, 3, 0)); expect("offset", signed(bits(raw, 23, 5) << 2, 21))
    elif name in {"ARM64_INST_BR", "ARM64_INST_BLR", "ARM64_INST_RET"}:
        expect("rn", bits(raw, 9, 5)); expect("operand_width", 64)
    elif name in {"ARM64_INST_SVC", "ARM64_INST_HVC", "ARM64_INST_SMC", "ARM64_INST_BRK", "ARM64_INST_HLT"}:
        expect("immediate", bits(raw, 20, 5))
    elif name == "ARM64_INST_BTI":
        expect("immediate", bits(raw, 11, 5) - 0x20)
    elif name in {"ARM64_INST_CLREX", "ARM64_INST_DSB", "ARM64_INST_DMB", "ARM64_INST_ISB"}:
        expect("immediate", bits(raw, 11, 8))
    elif name in {"ARM64_INST_NOP", "ARM64_INST_YIELD", "ARM64_INST_WFE", "ARM64_INST_WFI", "ARM64_INST_SEV", "ARM64_INST_SEVL"}:
        covered.add("encoding_identity_only")
    elif name in {"ARM64_INST_MRS", "ARM64_INST_MSR_REGISTER"}:
        expect("sysreg", bits(raw, 20, 5)); expect("rt", bits(raw, 4, 0)); expect("operand_width", 64)
    elif name in {"ARM64_INST_ADD_IMMEDIATE", "ARM64_INST_ADDS_IMMEDIATE", "ARM64_INST_SUB_IMMEDIATE", "ARM64_INST_SUBS_IMMEDIATE"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        shift = 12 if bits(raw, 22) else 0
        expect("immediate", bits(raw, 21, 10) << shift); expect("shift_amount", shift)
        expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name in {"ARM64_INST_MOVN", "ARM64_INST_MOVZ", "ARM64_INST_MOVK"}:
        expect("rd", bits(raw, 4, 0)); expect("immediate", bits(raw, 20, 5))
        expect("shift_amount", bits(raw, 22, 21) * 16); expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name in {"ARM64_INST_AND_IMMEDIATE", "ARM64_INST_ORR_IMMEDIATE", "ARM64_INST_EOR_IMMEDIATE", "ARM64_INST_ANDS_IMMEDIATE"}:
        width = 64 if bits(raw, 31) else 32
        decoded = decode_bit_masks(bits(raw, 22), bits(raw, 21, 16), bits(raw, 15, 10), width, True)
        if decoded is None: failures.append(("bitmask", None, "valid"))
        else: expect("immediate", decoded[0])
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("immr", bits(raw, 21, 16)); expect("operand_width", width)
    elif name in {"ARM64_INST_BFM", "ARM64_INST_SBFM", "ARM64_INST_UBFM"}:
        width = 64 if bits(raw, 31) else 32
        decoded = decode_bit_masks(bits(raw, 22), bits(raw, 21, 16), bits(raw, 15, 10), width, False)
        if decoded is None: failures.append(("bitmask", None, "valid"))
        else: expect("bitfield_wmask", decoded[0]); expect("bitfield_tmask", decoded[1])
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("immr", bits(raw, 21, 16)); expect("imms", bits(raw, 15, 10)); expect("operand_width", width)
    elif name == "ARM64_INST_EXTR":
        gpr3(); expect("shift_amount", bits(raw, 15, 10))
    elif name in {"ARM64_INST_ADD_EXTENDED_REGISTER", "ARM64_INST_ADDS_EXTENDED_REGISTER", "ARM64_INST_SUB_EXTENDED_REGISTER", "ARM64_INST_SUBS_EXTENDED_REGISTER"}:
        gpr3(); expect("extend_type", bits(raw, 15, 13)); expect("shift_amount", bits(raw, 12, 10))
    elif name in shifted:
        gpr3(); expect("shift_type", bits(raw, 23, 22)); expect("shift_amount", bits(raw, 15, 10))
    elif name in {"ARM64_INST_ADC", "ARM64_INST_ADCS", "ARM64_INST_SBC", "ARM64_INST_SBCS", "ARM64_INST_CSEL", "ARM64_INST_CSINC", "ARM64_INST_CSINV", "ARM64_INST_CSNEG", "ARM64_INST_UDIV", "ARM64_INST_SDIV", "ARM64_INST_LSLV", "ARM64_INST_LSRV", "ARM64_INST_ASRV", "ARM64_INST_RORV"}:
        gpr3()
        if name in {"ARM64_INST_CSEL", "ARM64_INST_CSINC", "ARM64_INST_CSINV", "ARM64_INST_CSNEG"}: expect("condition", bits(raw, 15, 12))
    elif name in {"ARM64_INST_RBIT", "ARM64_INST_CLZ", "ARM64_INST_REV64"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name == "ARM64_INST_CCMP_IMMEDIATE":
        expect("rn", bits(raw, 9, 5)); expect("immediate", bits(raw, 20, 16))
        expect("condition", bits(raw, 15, 12)); expect("nzcv", bits(raw, 3, 0)); expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name in {"ARM64_INST_MADD", "ARM64_INST_MSUB", "ARM64_INST_SMADDL", "ARM64_INST_UMADDL", "ARM64_INST_SMSUBL", "ARM64_INST_UMSUBL", "ARM64_INST_SMULH", "ARM64_INST_UMULH"}:
        gpr3()
        if name in {"ARM64_INST_MADD", "ARM64_INST_MSUB", "ARM64_INST_SMADDL", "ARM64_INST_UMADDL", "ARM64_INST_SMSUBL", "ARM64_INST_UMSUBL"}: expect("ra", bits(raw, 14, 10))
    elif name == "ARM64_INST_FCMP_REGISTER_SCALAR":
        expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        fp_width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("element_width", fp_width); expect("operand_width", fp_width)
    elif name in {"ARM64_INST_FCMP_ZERO_SCALAR", "ARM64_INST_FCMPE_ZERO_SCALAR"}:
        expect("rn", bits(raw, 9, 5))
        expect("element_width", {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)])
        expect("operand_width", {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)])
    elif name in {"ARM64_INST_FCCMP_SCALAR", "ARM64_INST_FCCMPE_SCALAR"}:
        width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        expect("immediate", bits(raw, 3, 0)); expect("condition", bits(raw, 15, 12))
        expect("element_width", width); expect("operand_width", width)
    elif name in {"ARM64_INST_FADD_SCALAR", "ARM64_INST_FMUL_SCALAR", "ARM64_INST_FDIV_SCALAR", "ARM64_INST_FSUB_SCALAR", "ARM64_INST_FMAX_SCALAR", "ARM64_INST_FMIN_SCALAR", "ARM64_INST_FMAXNM_SCALAR", "ARM64_INST_FMINNM_SCALAR", "ARM64_INST_FNMUL_SCALAR", "ARM64_INST_FCSEL_SCALAR"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        fp_width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("element_width", fp_width); expect("operand_width", fp_width)
        if name == "ARM64_INST_FCSEL_SCALAR": expect("condition", bits(raw, 15, 12))
    elif name in {"ARM64_INST_FMOV_SCALAR", "ARM64_INST_FABS_SCALAR", "ARM64_INST_FNEG_SCALAR", "ARM64_INST_FSQRT_SCALAR", "ARM64_INST_FRINTN_SCALAR", "ARM64_INST_FRINTP_SCALAR", "ARM64_INST_FRINTM_SCALAR", "ARM64_INST_FRINTZ_SCALAR", "ARM64_INST_FRINTA_SCALAR", "ARM64_INST_FRINTX_SCALAR", "ARM64_INST_FRINTI_SCALAR"}:
        width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", width); expect("operand_width", width)
    elif name in {"ARM64_INST_FMADD_SCALAR", "ARM64_INST_FMSUB_SCALAR", "ARM64_INST_FNMADD_SCALAR", "ARM64_INST_FNMSUB_SCALAR"}:
        width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16)); expect("ra", bits(raw, 14, 10))
        expect("element_width", width); expect("operand_width", width)
    elif name in {"ARM64_INST_FMOV_GPR_TO_FP", "ARM64_INST_FMOV_FP_TO_GPR"}:
        width = 64 if bits(raw, 31) else 32
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", width); expect("operand_width", width)
    elif name in {"ARM64_INST_SCVTF_SIMD_SCALAR", "ARM64_INST_UCVTF_SIMD_SCALAR", "ARM64_INST_FCVTZS_SIMD_SCALAR", "ARM64_INST_FCVTZU_SIMD_SCALAR", "ARM64_INST_FCVTNS_SIMD_SCALAR", "ARM64_INST_FCVTNU_SIMD_SCALAR", "ARM64_INST_FCVTAS_SIMD_SCALAR", "ARM64_INST_FCVTAU_SIMD_SCALAR", "ARM64_INST_FCVTPS_SIMD_SCALAR", "ARM64_INST_FCVTPU_SIMD_SCALAR", "ARM64_INST_FCVTMS_SIMD_SCALAR", "ARM64_INST_FCVTMU_SIMD_SCALAR"}:
        width = 64 if bits(raw, 22) else 32
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", width); expect("operand_width", width if bits(raw, 28) else width)
    elif name in {"ARM64_INST_FADD_VECTOR", "ARM64_INST_FMUL_VECTOR", "ARM64_INST_FSUB_VECTOR", "ARM64_INST_FMLA_VECTOR", "ARM64_INST_FCMGT_VECTOR", "ARM64_INST_ORR_VECTOR", "ARM64_INST_BIT_VECTOR", "ARM64_INST_BSL_VECTOR", "ARM64_INST_ZIP1_VECTOR", "ARM64_INST_ZIP2_VECTOR", "ARM64_INST_TRN1_VECTOR", "ARM64_INST_TRN2_VECTOR", "ARM64_INST_UZP1_VECTOR", "ARM64_INST_UZP2_VECTOR"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16)); expect("operand_width", 128 if bits(raw, 30) else 64)
        expect("element_width", 8 if name in {"ARM64_INST_ORR_VECTOR", "ARM64_INST_BIT_VECTOR", "ARM64_INST_BSL_VECTOR"} else 32 << (bits(raw, 22) & 1))
    elif name in {"ARM64_INST_BIF_VECTOR", "ARM64_INST_BIC_VECTOR"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        expect("element_width", 8); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_FDIV_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        expect("element_width", 32 << (bits(raw, 22) & 1)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_FRINTM_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", 32); expect("operand_width", 64)
    elif name in {"ARM64_INST_FCMGT_ZERO_VECTOR", "ARM64_INST_FCMEQ_ZERO_VECTOR", "ARM64_INST_FCMLT_ZERO_VECTOR", "ARM64_INST_FCMGE_ZERO_VECTOR", "ARM64_INST_FCMLE_ZERO_VECTOR"}:
        shape = bits(raw, 23, 16)
        width = 16 if shape == 0xF8 else 32 if shape == 0xA0 else 64
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", width); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name in {"ARM64_INST_FMLA_VECTOR_BY_ELEMENT", "ARM64_INST_FMUL_VECTOR_BY_ELEMENT", "ARM64_INST_FMLS_VECTOR_BY_ELEMENT", "ARM64_INST_FMUL_SCALAR_BY_ELEMENT", "ARM64_INST_FMLA_SCALAR_BY_ELEMENT"}:
        size = bits(raw, 23, 22); width = 8 << size if size != 0 else 16
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 19, 16) if size == 0 else bits(raw, 20, 16))
        expect("element_width", width); expect("lane_index", (bits(raw, 11) << 2 | bits(raw, 21) << 1 | bits(raw, 20)) if size == 0 else ((bits(raw, 11) << 1 | bits(raw, 21)) if size == 2 else bits(raw, 11)))
        expect("operand_width", width if "SCALAR" in name else (128 if bits(raw, 30) else 64))
    elif name == "ARM64_INST_DUP_ELEMENT_SCALAR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << size); expect("lane_index", imm5 >> (size + 1)); expect("operand_width", 8 << size)
    elif name == "ARM64_INST_DUP_ELEMENT_VECTOR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << size)
        expect("lane_index", imm5 >> (size + 1)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_INS_GPR_VECTOR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << size)
        expect("lane_index", imm5 >> (size + 1)); expect("immediate", bits(raw, 14, 11) >> size); expect("operand_width", 128)
    elif name == "ARM64_INST_INS_ELEMENT_VECTOR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("immediate", bits(raw, 14, 11) >> size); expect("element_width", 8 << size); expect("lane_index", imm5 >> (size + 1)); expect("operand_width", 128)
    elif name == "ARM64_INST_FMOV_FP_TO_GPR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", 64 if bits(raw, 31) else 32); expect("operand_width", 64 if bits(raw, 31) else 32)
    elif name == "ARM64_INST_FCVTZS_GPR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("operand_width", 64 if bits(raw, 31) else 32); expect("element_width", 64 if bits(raw, 22) else 32)
    elif name == "ARM64_INST_FMOV_SCALAR_IMMEDIATE":
        immediate = bits(raw, 20, 13)
        width = {0: 32, 1: 64, 3: 16}[bits(raw, 23, 22)]
        expect("rd", bits(raw, 4, 0)); expect("immediate", immediate)
        expect("element_width", width); expect("operand_width", width)
    elif name == "ARM64_INST_FMOV_VECTOR_IMMEDIATE":
        immediate = (bits(raw, 18, 16) << 5) | bits(raw, 9, 5)
        width = 64 if bits(raw, 29) else 32
        expect("rd", bits(raw, 4, 0)); expect("immediate", immediate); expect("element_width", width)
        expect("simd_cmode", bits(raw, 15, 12))
        expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_EXT_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16))
        expect("immediate", bits(raw, 14, 11)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name in {"ARM64_INST_ADD_VECTOR", "ARM64_INST_EOR_VECTOR", "ARM64_INST_MUL_VECTOR"}:
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rm", bits(raw, 20, 16)); expect("operand_width", 128 if bits(raw, 30) else 64)
        expect("element_width", 8 if name == "ARM64_INST_EOR_VECTOR" else 8 << bits(raw, 23, 22))
    elif name == "ARM64_INST_ADDV_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", 8 << bits(raw, 23, 22)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_SHL_VECTOR_IMMEDIATE":
        immh = bits(raw, 22, 19); immb = bits(raw, 18, 16); element_width = 8 << (immh.bit_length() - 1)
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", element_width); expect("operand_width", 128 if bits(raw, 30) else 64)
        expect("immediate", ((immh << 3) | immb) - element_width)
    elif name == "ARM64_INST_MOVI_VECTOR_IMMEDIATE":
        expect("rd", bits(raw, 4, 0)); expect("operand_width", 128 if bits(raw, 30) else 64)
        immediate = (bits(raw, 18, 16) << 5) | bits(raw, 9, 5)
        expect("immediate", immediate)
        cmode = bits(raw, 15, 12)
        expect("simd_cmode", cmode)
        if cmode <= 7:
            expect("element_width", 32)
        elif cmode in {8, 10}:
            expect("element_width", 16)
        elif cmode == 14:
            if bits(raw, 29):
                expect("element_width", 64)
            else:
                expect("element_width", 8)
    elif name in {"ARM64_INST_ORR_VECTOR_IMMEDIATE", "ARM64_INST_MVNI_VECTOR_IMMEDIATE", "ARM64_INST_BIC_VECTOR_IMMEDIATE"}:
        expect("rd", bits(raw, 4, 0)); expect("operand_width", 128 if bits(raw, 30) else 64)
        immediate = (bits(raw, 18, 16) << 5) | bits(raw, 9, 5)
        cmode = bits(raw, 15, 12)
        expect("immediate", immediate); expect("simd_cmode", cmode)
        expect("element_width", 32 if cmode <= 7 or cmode in {12, 13} else 16)
    elif name == "ARM64_INST_DUP_GENERAL_VECTOR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << size); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_UMOV_VECTOR_TO_GPR":
        imm5 = bits(raw, 20, 16); size = (imm5 & -imm5).bit_length() - 1
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << size)
        expect("lane_index", imm5 >> (size + 1)); expect("operand_width", 64 if bits(raw, 30) else 32)
    elif name == "ARM64_INST_XTN_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 16 << bits(raw, 23, 22)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_REV64_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", 8 << bits(raw, 23, 22)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_UMAXV_VECTOR":
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", 8 << bits(raw, 23, 22)); expect("operand_width", 128 if bits(raw, 30) else 64)
    elif name == "ARM64_INST_FADDP_SCALAR_REDUCE":
        width = (64 if bits(raw, 22) else 32) if bits(raw, 29) else 16
        expect("rd", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("element_width", width); expect("operand_width", width * 2)
    elif name.startswith("ARM64_INST_LD1_SINGLE_STRUCTURE_") or name in {"ARM64_INST_LD1", "ARM64_INST_ST1"}:
        opcode = bits(raw, 15, 13); size = bits(raw, 11, 10); q = bits(raw, 30); s = bits(raw, 12)
        element_width = 8 if opcode == 0 else 16 if opcode == 2 else 32 << (size & 1)
        lane_index = (q << 3) | (s << 2) | size if opcode == 0 else (q << 2) | (s << 1) | (size >> 1) if opcode == 2 else q if element_width == 64 else (q << 1) | s
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("element_width", element_width); expect("lane_index", lane_index); expect("operand_width", 128 if q else 64)
    elif name == "ARM64_INST_LDR_GPR_LITERAL":
        expect("rt", bits(raw, 4, 0)); expect("offset", signed(bits(raw, 23, 5) << 2, 21))
        expect("operand_width", 64 if bits(raw, 30) else 32)
    elif name == "ARM64_INST_LDR_FP_SIMD_LITERAL":
        literal_bytes = 4 << bits(raw, 31, 30)
        expect("rt", bits(raw, 4, 0)); expect("offset", signed(bits(raw, 23, 5) << 2, 21))
        expect("operand_width", literal_bytes * 8)
    elif re.fullmatch(r"ARM64_INST_(?:LDUR|LDTR)_(?:GPR|FP_SIMD)|ARM64_INST_(?:LDR|STR)_(?:GPR|FP_SIMD)_(?:POST_INDEX|PRE_INDEX|REGISTER_OFFSET|UNSIGNED_OFFSET)|ARM64_INST_(?:LDRSW)_(?:GPR)_(?:POST_INDEX|PRE_INDEX|REGISTER_OFFSET|UNSIGNED_OFFSET)|ARM64_INST_(?:STUR|STTR)_(?:GPR|FP_SIMD)", name):
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        fp_simd = "FP_SIMD" in name
        size = bits(raw, 31, 30)
        signed_word = name.startswith("ARM64_INST_LDRSW_")
        access_bytes = 4 if signed_word else (16 if fp_simd and size == 0 and (bits(raw, 23, 22) & 2) else 1 << size)
        expect("operand_width", access_bytes * 8 if fp_simd else (64 if signed_word or size == 3 else 32))
        if "REGISTER_OFFSET" in name:
            expect("rm", bits(raw, 20, 16)); expect("extend_type", bits(raw, 15, 13))
            expect("shift_amount", (access_bytes.bit_length() - 1) if bits(raw, 12) else 0)
        elif "UNSIGNED_OFFSET" in name: expect("offset", bits(raw, 21, 10) * access_bytes)
        else: expect("offset", signed(bits(raw, 20, 12), 9))
    elif re.fullmatch(
        r"ARM64_INST_(?:(?:LDUR|LDTR)(?:[BHWX]|SB_[WX]|SH_[WX]|SW_X)_GPR|"
        r"(?:STUR|STTR)[BHWX]_GPR|"
        r"LDR(?:[BHWX]|SB_[WX]|SH_[WX]|SW_X)_GPR_(?:POST_INDEX|PRE_INDEX|REGISTER_OFFSET|UNSIGNED_OFFSET)|"
        r"STR[BHWX]_GPR_(?:POST_INDEX|PRE_INDEX|REGISTER_OFFSET|UNSIGNED_OFFSET)|"
        r"(?:LDUR|STUR)[BHSDQ]_FP_SIMD|"
        r"(?:LDR|STR)[BHSDQ]_FP_SIMD_(?:POST_INDEX|PRE_INDEX|REGISTER_OFFSET|UNSIGNED_OFFSET))",
        name,
    ):
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        size = bits(raw, 31, 30)
        opc = bits(raw, 23, 22)
        fp_simd = "_FP_SIMD" in name
        access_bytes = 16 if fp_simd and size == 0 and (opc & 2) else 1 << size
        expect("operand_width", access_bytes * 8 if fp_simd else (64 if size == 3 or opc == 2 else 32))
        if "REGISTER_OFFSET" in name:
            expect("rm", bits(raw, 20, 16)); expect("extend_type", bits(raw, 15, 13))
            expect("shift_amount", (access_bytes.bit_length() - 1) if bits(raw, 12) else 0)
        elif "UNSIGNED_OFFSET" in name: expect("offset", bits(raw, 21, 10) * access_bytes)
        else: expect("offset", signed(bits(raw, 20, 12), 9))
    elif name in {"ARM64_INST_LDP_GPR_OFFSET", "ARM64_INST_LDP_GPR_POST_INDEX", "ARM64_INST_LDP_GPR_PRE_INDEX", "ARM64_INST_STP_GPR_OFFSET", "ARM64_INST_STP_GPR_POST_INDEX", "ARM64_INST_STP_GPR_PRE_INDEX", "ARM64_INST_STNP_GPR", "ARM64_INST_LDNP_GPR"} or name in {"ARM64_INST_LDPW_GPR_OFFSET", "ARM64_INST_LDPX_GPR_OFFSET", "ARM64_INST_LDPW_GPR_POST_INDEX", "ARM64_INST_LDPX_GPR_POST_INDEX", "ARM64_INST_LDPW_GPR_PRE_INDEX", "ARM64_INST_LDPX_GPR_PRE_INDEX", "ARM64_INST_STPW_GPR_OFFSET", "ARM64_INST_STPX_GPR_OFFSET", "ARM64_INST_STPW_GPR_POST_INDEX", "ARM64_INST_STPX_GPR_POST_INDEX", "ARM64_INST_STPW_GPR_PRE_INDEX", "ARM64_INST_STPX_GPR_PRE_INDEX", "ARM64_INST_STNPW_GPR", "ARM64_INST_STNPX_GPR", "ARM64_INST_LDNPW_GPR", "ARM64_INST_LDNPX_GPR"} or re.fullmatch(
        r"ARM64_INST_(?:LDNP[SDQ]|STNP[SDQ]|LDP[SDQ]|STP[SDQ])_FP_SIMD(?:_OFFSET|_POST_INDEX|_PRE_INDEX)?|"
        r"ARM64_INST_(?:LDNP|STNP)_FP_SIMD|ARM64_INST_(?:LDP|STP)_FP_SIMD_(?:OFFSET|POST_INDEX|PRE_INDEX)", name
    ):
        expect("rt", bits(raw, 4, 0)); expect("rt2", bits(raw, 14, 10)); expect("rn", bits(raw, 9, 5))
        fp_simd = "FP_SIMD" in name
        access_bytes = (4 << bits(raw, 31, 30)) if fp_simd else (8 if bits(raw, 31, 30) == 2 else 4)
        expect("offset", signed(bits(raw, 21, 15), 7) * access_bytes)
        expect("operand_width", access_bytes * 8 if fp_simd else (64 if bits(raw, 31, 30) else 32))
    elif name in {"ARM64_INST_LDXR", "ARM64_INST_LDAXR", "ARM64_INST_STXR", "ARM64_INST_STLXR", "ARM64_INST_LDXP", "ARM64_INST_LDAXP", "ARM64_INST_STXP", "ARM64_INST_STLXP", "ARM64_INST_LDAR", "ARM64_INST_STLR", "ARM64_INST_CASAL", "ARM64_INST_LDXRB", "ARM64_INST_LDXRH", "ARM64_INST_LDXRW", "ARM64_INST_LDXRX", "ARM64_INST_LDAXRB", "ARM64_INST_LDAXRH", "ARM64_INST_LDAXRW", "ARM64_INST_LDAXRX", "ARM64_INST_STXRB", "ARM64_INST_STXRH", "ARM64_INST_STXRW", "ARM64_INST_STXRX", "ARM64_INST_STLXRB", "ARM64_INST_STLXRH", "ARM64_INST_STLXRW", "ARM64_INST_STLXRX", "ARM64_INST_LDXPW", "ARM64_INST_LDXPX", "ARM64_INST_LDAXPW", "ARM64_INST_LDAXPX", "ARM64_INST_STXPW", "ARM64_INST_STXPX", "ARM64_INST_STLXPW", "ARM64_INST_STLXPX", "ARM64_INST_LDAR_X", "ARM64_INST_STLR_X"} or re.fullmatch(r"ARM64_INST_(?:STLLR|STLR|LDLAR|LDAR)_[BHWX]", name) or re.fullmatch(r"ARM64_INST_(?:LDADD|LDADDL|LDADDA|LDADDAL|LDCLR|LDCLRL|LDCLRA|LDCLRAL|LDEOR|LDEORL|LDEORA|LDEORAL|LDSET|LDSETL|LDSETA|LDSETAL|LDSMAX|LDSMAXL|LDSMAXA|LDSMAXAL|LDSMIN|LDSMINL|LDSMINA|LDSMINAL|LDUMAX|LDUMAXL|LDUMAXA|LDUMAXAL|LDUMIN|LDUMINL|LDUMINA|LDUMINAL|SWP|SWPL|SWPA|SWPAL)(?:_[BHWX])?", name):
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        if name.startswith(("ARM64_INST_STXR", "ARM64_INST_STLXR", "ARM64_INST_STXP", "ARM64_INST_STLXP")) or name in {"ARM64_INST_CASAL"} or re.fullmatch(r"ARM64_INST_(?:LDADD|LDADDL|LDADDA|LDADDAL|LDCLR|LDCLRL|LDCLRA|LDCLRAL|LDEOR|LDEORL|LDEORA|LDEORAL|LDSET|LDSETL|LDSETA|LDSETAL|LDSMAX|LDSMAXL|LDSMAXA|LDSMAXAL|LDSMIN|LDSMINL|LDSMINA|LDSMINAL|LDUMAX|LDUMAXL|LDUMAXA|LDUMIN|LDUMINL|LDUMINA|LDUMINAL|SWP|SWPL|SWPA|SWPAL)(?:_[BHWX])?", name): expect("rs", bits(raw, 20, 16))
        access_bytes = 1 << bits(raw, 31, 30)
        if name in {"ARM64_INST_LDXP", "ARM64_INST_LDAXP", "ARM64_INST_STXP", "ARM64_INST_STLXP", "ARM64_INST_LDXPW", "ARM64_INST_LDXPX", "ARM64_INST_LDAXPW", "ARM64_INST_LDAXPX", "ARM64_INST_STXPW", "ARM64_INST_STXPX", "ARM64_INST_STLXPW", "ARM64_INST_STLXPX"}:
            expect("rt2", bits(raw, 14, 10)); expect("operand_width", 64 if bits(raw, 30) else 32)
        elif name in {"ARM64_INST_LDXRB", "ARM64_INST_LDXRH", "ARM64_INST_LDXRW", "ARM64_INST_LDXRX", "ARM64_INST_LDAXRB", "ARM64_INST_LDAXRH", "ARM64_INST_LDAXRW", "ARM64_INST_LDAXRX", "ARM64_INST_STXRB", "ARM64_INST_STXRH", "ARM64_INST_STXRW", "ARM64_INST_STXRX", "ARM64_INST_STLXRB", "ARM64_INST_STLXRH", "ARM64_INST_STLXRW", "ARM64_INST_STLXRX"}:
            expect("operand_width", 64 if name.endswith("X") else 32)
        else:
            expect("operand_width", 64 if access_bytes == 8 else 32)
    elif name == "ARM64_INST_LDAPR" or name.startswith("ARM64_INST_LDAPR_"):
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5))
        expect("operand_width", 64 if bits(raw, 30) else 32)
    elif name.startswith("ARM64_INST_CASP") and name[-1] in {"W", "X"}:
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rs", bits(raw, 20, 16))
        expect("operand_width", 64 if name.endswith("X") else 32)
    elif name.startswith("ARM64_INST_CAS") and name[-1] in {"B", "H", "W", "X"}:
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rs", bits(raw, 20, 16))
        expect("operand_width", 64 if name.endswith("X") else 32)
    elif name == "ARM64_INST_CASPAL":
        expect("rt", bits(raw, 4, 0)); expect("rn", bits(raw, 9, 5)); expect("rs", bits(raw, 20, 16))
        access_bytes = 4 if bits(raw, 31, 30) == 0 else 8
        expect("operand_width", access_bytes * 8)
    elif name == "ARM64_INST_PRFM_UNSIGNED_OFFSET":
        expect("rn", bits(raw, 9, 5))
        expect("immediate", bits(raw, 4, 0)); expect("offset", bits(raw, 21, 10) * 8)
    else:
        failures.append(("instruction", name, "implemented audit rule"))
    for field in FIELDS:
        if field not in covered:
            expect(field, 0)
    return covered, failures


def main():
    parser = argparse.ArgumentParser(description="Independently recompute ARM64 decoder fields from raw encodings")
    parser.add_argument("header", help="path to arm64_decode.h")
    parser.add_argument("decoder_tsv", help="decoder output TSV with a header row")
    parser.add_argument("--llvm-tsv", help="LLVM strict audit TSV")
    parser.add_argument("--identity-map", help="instruction/opcode identity allowlist TSV")
    parser.add_argument("--skip-rows", type=non_negative_int, default=0, help="number of leading decoder rows to skip")
    parser.add_argument("--expected-rows", type=non_negative_int, help="require exactly this many audited rows")
    args = parser.parse_args()

    names = instruction_names(args.header)
    with open(args.decoder_tsv, newline="", encoding="utf-8") as decoder_file:
        all_rows = list(csv.DictReader(decoder_file, delimiter="\t"))
    rows = all_rows[args.skip_rows:]
    if not rows:
        parser.error("no decoder rows remain after --skip-rows")
    if args.expected_rows is not None and len(rows) != args.expected_rows:
        parser.error(f"audited row count is {len(rows)}, expected {args.expected_rows}")
    covered = Counter(); failures = []; identity_only = Counter()
    contract_checks = 0
    for row in rows:
        raw = int(row["raw"], 0)
        expected_class = expected_instruction_class(raw)
        contract_checks += 2
        if row["status"] != "0":
            failures.append(("decoder", row["raw"], "status", row["status"], 0))
        if expected_class is None:
            failures.append(("decoder", row["raw"], "class", row["class"], "known raw owner"))
        elif int(row["class"], 0) != expected_class:
            failures.append(("decoder", row["raw"], "class", row["class"], expected_class))
    decoder_contract_failures = len(failures)
    print(f"decoder_contract_checks={contract_checks}")
    print(f"decoder_contract_failures={decoder_contract_failures}")
    if args.llvm_tsv is not None:
        if args.identity_map is None:
            parser.error("--identity-map is required with --llvm-tsv")
        with open(args.llvm_tsv, newline="", encoding="utf-8") as llvm_file:
            llvm_rows = list(csv.DictReader(llvm_file, delimiter="\t"))
        llvm_failures, expected_pairs, observed_pairs = audit_llvm_rows(rows, llvm_rows, names, args.identity_map)
        failures.extend(("LLVM",) + failure for failure in llvm_failures)
        llvm_field_failures, llvm_field_coverage = audit_llvm_projected_fields(rows, llvm_rows, names)
        failures.extend(("LLVM_FIELD",) + failure for failure in llvm_field_failures)
        print(f"llvm_rows={len(llvm_rows)}")
        print(f"llvm_identity_pairs={len(observed_pairs)} expected={len(expected_pairs)}")
        print("llvm_missing_pairs=" + ",".join(f"{pair[0]}:{pair[1]}" for pair in sorted(expected_pairs - observed_pairs)))
        print("llvm_unexpected_pairs=" + ",".join(f"{pair[0]}:{pair[1]}" for pair in sorted(observed_pairs - expected_pairs)))
        print(f"llvm_field_checks={sum(llvm_field_coverage.values())}")
        print("llvm_field_coverage=" + ",".join(f"{field}:{llvm_field_coverage[field]}" for field in FIELDS if llvm_field_coverage[field]))
        print(f"llvm_field_failures={len(llvm_field_failures)}")
        print("llvm_nonprojectable_fields=" + ",".join(
            field for field in FIELDS if field not in LLVM_DIRECT_FIELDS))
    for row in rows:
        name = names[int(row["instruction"], 0)]
        row_covered, row_failures = audit_row(row, name)
        covered.update(field for field in row_covered if field in FIELDS)
        failures.extend((name, row["raw"],) + failure for failure in row_failures)
        if row_covered == {"encoding_identity_only"}:
            identity_only[name] += 1
    print(f"rows={len(rows)}")
    llvm_failures_count = sum(1 for failure in failures if failure and failure[0] in {"LLVM", "LLVM_FIELD"})
    field_failures = len(failures) - decoder_contract_failures - llvm_failures_count
    print(f"field_checks={sum(covered.values())} failures={field_failures}")
    print(f"llvm_failures={llvm_failures_count}")
    print(f"total_failures={len(failures)}")
    print("coverage=" + ",".join(f"{field}:{covered[field]}" for field in FIELDS))
    print("identity_only=" + ",".join(f"{name}:{count}" for name, count in sorted(identity_only.items())))
    field_failure_counts = Counter()
    field_failure_by_instruction = Counter()
    for failure in failures:
        if not failure or failure[0] in {"LLVM", "LLVM_FIELD", "decoder"}:
            continue
        if len(failure) >= 4:
            field_failure_by_instruction[failure[0]] += 1
            field_failure_counts[failure[2]] += 1
    print("field_failure_counts=" + ",".join(f"{field}:{count}" for field, count in sorted(field_failure_counts.items())))
    print("field_failure_instructions=" + ",".join(f"{name}:{count}" for name, count in sorted(field_failure_by_instruction.items())))
    for failure in failures[:100]: print("FAIL", *failure, sep="\t")
    field_failures_seen = 0
    for failure in failures:
        if failure and failure[0] not in {"LLVM", "decoder"}:
            print("FIELD_FAIL", *failure, sep="\t")
            field_failures_seen += 1
            if field_failures_seen >= 100:
                break
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
