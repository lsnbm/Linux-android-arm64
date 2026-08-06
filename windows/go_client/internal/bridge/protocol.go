package bridge

import (
	"fmt"
	"strconv"
	"strings"
)

var ScanHistoryModes = map[string]bool{
	"increased": true, "decreased": true, "changed": true, "unchanged": true,
}

var ScanValueModes = map[string]bool{
	"equal": true, "greater": true, "less": true, "range": true, "pointer": true, "string": true,
}

var ScanModeAliases = map[string]string{
	"eq": "equal", "gt": "greater", "lt": "less",
	"inc": "increased", "dec": "decreased", "chg": "changed", "unchg": "unchanged",
	"ptr": "pointer", "str": "string",
}

var ValueTypeAliases = map[string]string{
	"i8": "i8", "int8": "i8",
	"i16": "i16", "int16": "i16",
	"i32": "i32", "int32": "i32",
	"i64": "i64", "int64": "i64",
	"f32": "f32", "float": "f32",
	"f64": "f64", "double": "f64",
}

var ValueTypeLabels = map[string]string{
	"i8": "I8", "i16": "I16", "i32": "I32", "i64": "I64", "f32": "Float", "f64": "Double",
}

var SavedValueKinds = map[string]bool{"numeric": true, "pointer": true, "text": true}

var ViewerFormatTokens = map[string]bool{
	"hexadecimal": true, "hex": true, "i8": true, "i16": true, "i32": true,
	"i64": true, "f32": true, "f64": true, "disasm": true,
}

// FormatAddress renders an address as a 0x-prefixed uppercase hex string.
func FormatAddress(v any) string {
	switch val := v.(type) {
	case int:
		return fmt.Sprintf("0x%X", val)
	case int64:
		return fmt.Sprintf("0x%X", val)
	case int32:
		return fmt.Sprintf("0x%X", val)
	case uint64:
		return fmt.Sprintf("0x%X", val)
	case string:
		return strings.TrimSpace(val)
	default:
		if f, ok := v.(float64); ok {
			return fmt.Sprintf("0x%X", int64(f))
		}
		return strings.TrimSpace(fmt.Sprint(v))
	}
}

func NormalizeViewFormat(viewFormat string) (string, error) {
	token := strings.ToLower(strings.TrimSpace(viewFormat))
	if !ViewerFormatTokens[token] {
		return "", fmt.Errorf("view_format must be one of: hexadecimal, hex, i8, i16, i32, i64, f32, f64, disasm")
	}
	return token, nil
}

func NormalizeScanMode(mode string) string {
	token := strings.ToLower(strings.TrimSpace(mode))
	if alias, ok := ScanModeAliases[token]; ok {
		return alias
	}
	return token
}

func NormalizeValueType(valueType string) (string, error) {
	token := strings.ToLower(strings.TrimSpace(valueType))
	normalized, ok := ValueTypeAliases[token]
	if !ok {
		return "", fmt.Errorf("value_type must be one of: i8, i16, i32, i64, f32, f64")
	}
	return normalized, nil
}

func NormalizeSavedKind(valueKind string) (string, error) {
	token := strings.ToLower(strings.TrimSpace(valueKind))
	if token == "" {
		token = "numeric"
	}
	aliases := map[string]string{"number": "numeric", "ptr": "pointer", "string": "text"}
	if alias, ok := aliases[token]; ok {
		token = alias
	}
	if !SavedValueKinds[token] {
		return "", fmt.Errorf("value_kind must be one of: numeric, pointer, text")
	}
	return token, nil
}

func ParseBool(v any) (bool, error) {
	switch val := v.(type) {
	case bool:
		return val, nil
	case int64:
		return val != 0, nil
	case float64:
		return val != 0, nil
	case string:
		switch strings.ToLower(strings.TrimSpace(val)) {
		case "true", "1", "yes", "on":
			return true, nil
		case "false", "0", "no", "off", "":
			return false, nil
		}
	}
	return false, fmt.Errorf("invalid boolean value: %v", v)
}

// BuildSavedAddParams builds the saved.add request payload.
func BuildSavedAddParams(address any, valueType, valueKind string, textLength int, note string) (map[string]any, error) {
	kind, err := NormalizeSavedKind(valueKind)
	if err != nil {
		return nil, err
	}
	typeToken := ""
	switch kind {
	case "pointer":
		typeToken = "i64"
	case "text":
		typeToken = "i8"
	default:
		typeToken, err = NormalizeValueType(valueType)
		if err != nil {
			return nil, err
		}
	}
	if textLength < 1 || textLength > 256 {
		return nil, fmt.Errorf("text_length must be in 1..256")
	}
	return map[string]any{
		"address":    FormatAddress(address),
		"value_type": typeToken,
		"value_kind": kind,
		"text_length": textLength,
		"note":       note,
	}, nil
}

// BuildScanParams builds the scan.start / scan.refine request payload.
func BuildScanParams(valueType, mode, value, rangeMax string, isFirst bool) (map[string]any, error) {
	modeToken := NormalizeScanMode(mode)
	valueToken := strings.TrimSpace(value)
	rangeToken := strings.TrimSpace(rangeMax)

	if isFirst && ScanHistoryModes[modeToken] {
		return nil, fmt.Errorf("first scan cannot use increased, decreased, changed, or unchanged")
	}
	if !isFirst && modeToken == "unknown" {
		return nil, fmt.Errorf("unknown initial value can only be used for the first scan")
	}
	if ScanValueModes[modeToken] && valueToken == "" {
		return nil, fmt.Errorf("value is required for mode '%s'", modeToken)
	}
	if modeToken == "range" && rangeToken == "" {
		return nil, fmt.Errorf("range_max is required for mode 'range'")
	}

	params := map[string]any{"mode": modeToken}
	switch modeToken {
	case "pointer":
		params["value_type"] = "i64"
	case "string":
		// no value_type
	default:
		vt, err := NormalizeValueType(valueType)
		if err != nil {
			return nil, err
		}
		params["value_type"] = vt
	}
	if ScanValueModes[modeToken] {
		params["value"] = valueToken
	}
	if modeToken == "range" {
		params["range_max"] = rangeToken
	}
	return params, nil
}

// NormalizeBreakpointPoints validates and normalizes breakpoint point objects.
func NormalizeBreakpointPoints(points []map[string]any) ([]map[string]any, error) {
	if len(points) == 0 {
		return nil, fmt.Errorf("points must not be empty")
	}
	if len(points) > 16 {
		return nil, fmt.Errorf("points must be at most 16")
	}
	normalized := make([]map[string]any, 0, len(points))
	for _, point := range points {
		length, err := toInt(point["length"])
		if err != nil || length < 1 || length > 8 {
			return nil, fmt.Errorf("breakpoint length must be in 1..8")
		}
		normalized = append(normalized, map[string]any{
			"address":  FormatAddress(point["address"]),
			"bp_type":  point["bp_type"],
			"bp_scope": point["bp_scope"],
			"length":   length,
		})
	}
	return normalized, nil
}

type SavedAddressState struct {
	Address         int64
	AddressHex      string
	ValueType       string
	ValueTypeLabel  string
	ValueKind       string
	TextLength      int
	Note            string
	Value           string
	Locked          bool
	LockValue       string
}

// ParseSavedStates parses the saved.list response data into states.
func ParseSavedStates(data map[string]any) ([]SavedAddressState, error) {
	rawItems, ok := data["items"].([]any)
	if !ok {
		return nil, fmt.Errorf("saved state response items must be a list")
	}
	states := make([]SavedAddressState, 0, len(rawItems))
	for _, raw := range rawItems {
		item, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		valueType, err := NormalizeValueType(str(fmt.Sprint(item["value_type"])))
		if err != nil {
			return nil, err
		}
		valueKind, err := NormalizeSavedKind(fmt.Sprint(item["value_kind"]))
		if err != nil {
			valueKind = "numeric"
		}
		address, _ := toInt(item["address"])
		addressHex := str(item["address_hex"])
		if addressHex == "" {
			addressHex = FormatAddress(address)
		}
		textLength := 64
		if n, err := toInt(item["text_length"]); err == nil {
			textLength = int(n)
			if textLength < 1 {
				textLength = 1
			}
			if textLength > 256 {
				textLength = 256
			}
		}
		locked, err := ParseBool(item["locked"])
		if err != nil {
			locked = false
		}
		states = append(states, SavedAddressState{
			Address:        address,
			AddressHex:     addressHex,
			ValueType:      valueType,
			ValueTypeLabel: firstNonEmpty(str(item["value_type_label"]), ValueTypeLabels[valueType]),
			ValueKind:      valueKind,
			TextLength:     textLength,
			Note:           str(item["note"]),
			Value:          str(item["value"]),
			Locked:         locked,
			LockValue:      str(item["lock_value"]),
		})
	}
	return states, nil
}

func toInt(v any) (int64, error) {
	switch val := v.(type) {
	case int:
		return int64(val), nil
	case int64:
		return val, nil
	case int32:
		return int64(val), nil
	case float64:
		return int64(val), nil
	case string:
		return strconv.ParseInt(strings.TrimSpace(val), 0, 64)
	default:
		return 0, fmt.Errorf("cannot parse %v", v)
	}
}

func str(v any) string {
	if v == nil {
		return ""
	}
	return fmt.Sprint(v)
}

func firstNonEmpty(a, b string) string {
	if a != "" {
		return a
	}
	return b
}
