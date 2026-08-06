package ui

import (
	"encoding/binary"
	"fmt"
	"math"
	"regexp"
	"strconv"
	"strings"
)

// ---- generic helpers ----

func toInt64(v any) int64 {
	switch val := v.(type) {
	case int:
		return int64(val)
	case int64:
		return val
	case int32:
		return int64(val)
	case float64:
		return int64(val)
	case float32:
		return int64(val)
	case string:
		if n, err := strconv.ParseInt(strings.TrimSpace(val), 0, 64); err == nil {
			return n
		}
	case bool:
		if val {
			return 1
		}
	}
	return 0
}

func toBool(v any) bool {
	switch val := v.(type) {
	case bool:
		return val
	case int64:
		return val != 0
	case float64:
		return val != 0
	case string:
		switch strings.ToLower(strings.TrimSpace(val)) {
		case "true", "1", "yes", "on":
			return true
		}
	}
	return false
}

func toString(v any) string {
	if v == nil {
		return ""
	}
	return fmt.Sprint(v)
}

// parseBase0 parses an integer with auto base detection (0x hex, 0 octal, else decimal).
func parseBase0(s string) (int64, error) {
	return strconv.ParseInt(strings.TrimSpace(s), 0, 64)
}

// parseAddressExpression splits "0x12345678+0xA8" style expressions on
// '+'/'-' and sums the parts (first part is the base address).
var addrSepPattern = regexp.MustCompile(`([+-])`)

func parseAddressExpression(s string) (int64, error) {
	text := strings.TrimSpace(s)
	if text == "" {
		return 0, fmt.Errorf("empty address")
	}
	parts := addrSepPattern.Split(text, -1)
	total := int64(0)
	offset := 0
	for i, part := range parts {
		part = strings.TrimSpace(part)
		if part == "" {
			return 0, fmt.Errorf("invalid address expression: %s", s)
		}
		value, err := parseBase0(part)
		if err != nil {
			return 0, fmt.Errorf("invalid address part: %s", part)
		}
		if i == 0 {
			total = value
			offset = len(parts[0])
			continue
		}
		if offset < len(text) && text[offset] == '-' {
			total -= value
		} else {
			total += value
		}
		offset += len(part)
	}
	return total, nil
}

func hexToBytes(hexText string) []byte {
	clean := strings.Map(func(r rune) rune {
		if (r >= '0' && r <= '9') || (r >= 'a' && r <= 'f') || (r >= 'A' && r <= 'F') {
			return r
		}
		return -1
	}, hexText)
	if clean == "" || len(clean)%2 != 0 {
		return nil
	}
	out := make([]byte, len(clean)/2)
	for i := 0; i < len(out); i++ {
		hi := hexDigit(clean[i*2])
		lo := hexDigit(clean[i*2+1])
		if hi < 0 || lo < 0 {
			return nil
		}
		out[i] = byte(hi<<4 | lo)
	}
	return out
}

func hexDigit(c byte) int {
	switch {
	case c >= '0' && c <= '9':
		return int(c - '0')
	case c >= 'a' && c <= 'f':
		return int(c-'a') + 10
	case c >= 'A' && c <= 'F':
		return int(c-'A') + 10
	}
	return -1
}

// ---- formatting helpers (mirror LuckyStar.py) ----

func formatAddr(v any) string {
	return fmt.Sprintf("0x%016X", toInt64(v))
}

func formatAddrShort(v any) string {
	return fmt.Sprintf("0x%X", toInt64(v))
}

func formatProt(v any) string {
	prot := toInt64(v)
	parts := []string{"r", "w", "x"}
	text := ""
	for i, p := range parts {
		if prot&(1<<i) != 0 {
			text += p
		} else {
			text += "-"
		}
	}
	return fmt.Sprintf("%s(%d)", text, prot)
}

func formatSize(size int64) string {
	units := []string{"B", "KiB", "MiB", "GiB", "TiB"}
	value := float64(size)
	unit := 0
	for value >= 1024 && unit < len(units)-1 {
		value /= 1024
		unit++
	}
	if unit == 0 {
		return fmt.Sprintf("%.0fB", value)
	}
	return fmt.Sprintf("%.2f%s", value, units[unit])
}

// ---- typed dump helpers ----

var endian = binary.LittleEndian

func renderHexDump(addr int64, data []byte) string {
	var sb strings.Builder
	sb.WriteString("OFFSET  ADDRESS             BYTES                                            ASCII\n")
	sb.WriteString("------  ------------------  -----------------------------------------------  ----------------\n")
	for offset := 0; offset < len(data); offset += 16 {
		end := offset + 16
		if end > len(data) {
			end = len(data)
		}
		chunk := data[offset:end]
		hexParts := make([]string, len(chunk))
		asciiParts := make([]byte, len(chunk))
		for i, b := range chunk {
			hexParts[i] = fmt.Sprintf("%02X", b)
			if b >= 32 && b <= 126 {
				asciiParts[i] = b
			} else {
				asciiParts[i] = '.'
			}
		}
		hexPart := strings.Join(hexParts, " ")
		fmt.Fprintf(&sb, "+%04X   0x%016X  %-47s  %-16s\n", offset, addr+int64(offset), hexPart, string(asciiParts))
	}
	return sb.String()
}

func renderHexadecimalDump(addr int64, data []byte) string {
	var sb strings.Builder
	sb.WriteString("OFFSET  ADDRESS             VALUE (U64 LE)\n")
	sb.WriteString("------  ------------------  ------------------\n")
	for offset := 0; offset < len(data); offset += 8 {
		end := offset + 8
		if end > len(data) {
			end = len(data)
		}
		chunk := data[offset:end]
		if len(chunk) == 8 {
			value := endian.Uint64(chunk)
			fmt.Fprintf(&sb, "+%04X   0x%016X  0x%016X\n", offset, addr+int64(offset), value)
		} else {
			hexParts := make([]string, len(chunk))
			for i, b := range chunk {
				hexParts[i] = fmt.Sprintf("%02X", b)
			}
			fmt.Fprintf(&sb, "+%04X   0x%016X  %s\n", offset, addr+int64(offset), strings.Join(hexParts, " "))
		}
	}
	return sb.String()
}

func renderTypedDump(addr int64, data []byte, format string) string {
	typeInfo := map[string][2]any{
		"i8":  {1, "I8"},
		"i16": {2, "I16"},
		"i32": {4, "I32"},
		"i64": {8, "I64"},
		"f32": {4, "FLOAT"},
		"f64": {8, "DOUBLE"},
	}
	info, ok := typeInfo[format]
	if !ok {
		return renderHexDump(addr, data)
	}
	unit := info[0].(int)
	typeLabel := info[1].(string)

	var sb strings.Builder
	fmt.Fprintf(&sb, "OFFSET  ADDRESS             VALUE (%s)\n", typeLabel)
	sb.WriteString("------  ------------------  ------------------------\n")
	limit := len(data) - len(data)%unit
	for offset := 0; offset < limit; offset += unit {
		chunk := data[offset : offset+unit]
		var valueText string
		switch format {
		case "i8":
			valueText = strconv.FormatInt(int64(int8(chunk[0])), 10)
		case "i16":
			valueText = strconv.FormatInt(int64(int16(endian.Uint16(chunk))), 10)
		case "i32":
			valueText = strconv.FormatInt(int64(int32(endian.Uint32(chunk))), 10)
		case "i64":
			valueText = strconv.FormatInt(int64(endian.Uint64(chunk)), 10)
		case "f32":
			valueText = strconv.FormatFloat(float64(math.Float32frombits(endian.Uint32(chunk))), 'g', 9, 32)
		case "f64":
			valueText = strconv.FormatFloat(math.Float64frombits(endian.Uint64(chunk)), 'g', 17, 64)
		}
		fmt.Fprintf(&sb, "+%04X   0x%016X  %s\n", offset, addr+int64(offset), valueText)
	}
	if sb.Len() == 0 {
		sb.WriteString("(no data)\n")
	}
	return sb.String()
}

const browserDisasmCacheLines = 25

func extractDisasmWindow(snapshot map[string]any) ([]map[string]any, int64) {
	baseAddr := toInt64(snapshot["base"])
	disasmList, _ := snapshot["disasm"].([]any)
	if len(disasmList) == 0 {
		return nil, baseAddr
	}
	endIdx := len(disasmList)
	if endIdx > browserDisasmCacheLines {
		endIdx = browserDisasmCacheLines
	}
	var windowItems []map[string]any
	for _, item := range disasmList[:endIdx] {
		if m, ok := item.(map[string]any); ok {
			windowItems = append(windowItems, m)
		}
	}
	if len(windowItems) == 0 {
		return nil, baseAddr
	}
	visibleAddr := toInt64(windowItems[0]["address"])
	if visibleAddr == 0 {
		visibleAddr = baseAddr
	}
	return windowItems, visibleAddr
}

func renderDisasmDump(snapshot map[string]any) string {
	var sb strings.Builder
	sb.WriteString("ADDRESS             BYTES                    INSTRUCTION\n")
	sb.WriteString("------------------  -----------------------  ----------------------------------------\n")
	windowItems, _ := extractDisasmWindow(snapshot)
	if len(windowItems) == 0 {
		sb.WriteString("(no data)\n")
		return sb.String()
	}
	for _, item := range windowItems {
		addressHex := toString(item["address_hex"])
		if addressHex == "" {
			addressHex = "0x0"
		}
		bytesHex := toString(item["bytes_hex"])
		mnemonic := strings.TrimSpace(toString(item["mnemonic"]))
		opStr := strings.TrimSpace(toString(item["op_str"]))
		line := fmt.Sprintf("%-18s  %-23s  %s", addressHex, bytesHex, mnemonic)
		if opStr != "" {
			line += " " + opStr
		}
		sb.WriteString(line + "\n")
	}
	return sb.String()
}

func formatSigResult(data map[string]any) string {
	var lines []string
	count := toInt64(data["count"])
	returnedCount := toInt64(data["returned_count"])
	truncated := toBool(data["truncated"])
	changedCount, hasChanged := data["changed_count"], data["changed_count"] != nil
	totalCount, hasTotal := data["total_count"], data["total_count"] != nil
	if hasChanged && hasTotal {
		lines = append(lines, fmt.Sprintf("过滤变化: %d/%d", toInt64(changedCount), toInt64(totalCount)))
	}
	lines = append(lines,
		fmt.Sprintf("匹配数量: %d", count),
		fmt.Sprintf("返回数量: %d", returnedCount),
		fmt.Sprintf("是否截断: %s", map[bool]string{true: "是", false: "否"}[truncated]),
	)
	if file, ok := data["file"]; ok {
		lines = append(lines, "文件: "+toString(file))
	}
	if pattern := strings.TrimSpace(toString(data["pattern"])); pattern != "" {
		lines = append(lines, "特征码: "+pattern)
	}
	if r, ok := data["range"]; ok {
		lines = append(lines, "偏移: "+toString(r))
	}
	if oldSig, ok := data["old_signature"]; ok {
		lines = append(lines, "旧特征: "+toString(oldSig))
	}
	if newSig, ok := data["new_signature"]; ok {
		lines = append(lines, "新特征: "+toString(newSig))
	}
	lines = append(lines, "", "【匹配地址】")
	matches, _ := data["matches"].([]any)
	if len(matches) == 0 {
		lines = append(lines, "无")
	} else {
		for idx, item := range matches {
			if m, ok := item.(map[string]any); ok {
				lines = append(lines, fmt.Sprintf("%04d. %s", idx+1, toString(m["addr_hex"])))
			} else {
				lines = append(lines, fmt.Sprintf("%04d. %s", idx+1, toString(item)))
			}
		}
	}
	return strings.Join(lines, "\n")
}
