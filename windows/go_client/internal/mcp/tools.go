package mcp

import (
	"context"
	"encoding/json"
	"fmt"
	"regexp"
	"strconv"
	"strings"

	"github.com/mark3labs/mcp-go/mcp"
	"github.com/mark3labs/mcp-go/server"

	"luckystar/internal/bridge"
)

const maxAddressValue = int64(2_147_483_647)

// ---- argument helpers ----

func argString(args map[string]any, key string) string {
	if v, ok := args[key]; ok && v != nil {
		return fmt.Sprint(v)
	}
	return ""
}

func argInt(args map[string]any, key string) (int64, error) {
	v, ok := args[key]
	if !ok || v == nil {
		return 0, fmt.Errorf("%s is required", key)
	}
	switch val := v.(type) {
	case float64:
		return int64(val), nil
	case int64:
		return val, nil
	case string:
		return strconv.ParseInt(strings.TrimSpace(val), 10, 64)
	}
	return 0, fmt.Errorf("%s must be an integer", key)
}

func argBool(args map[string]any, key string) (bool, error) {
	v, ok := args[key]
	if !ok || v == nil {
		return false, fmt.Errorf("%s is required", key)
	}
	switch val := v.(type) {
	case bool:
		return val, nil
	case float64:
		return val != 0, nil
	case string:
		switch strings.ToLower(strings.TrimSpace(val)) {
		case "true", "1", "yes", "on":
			return true, nil
		case "false", "0", "no", "off":
			return false, nil
		}
	}
	return false, fmt.Errorf("%s must be a boolean", key)
}

func addressArg(args map[string]any, key string) (string, error) {
	v, ok := args[key]
	if !ok || v == nil {
		return "", fmt.Errorf("%s is required", key)
	}
	return bridge.FormatAddress(v), nil
}

// ---- schema option helpers ----

func addressParam(name string, required bool) mcp.ToolOption {
	opts := []mcp.PropertyOption{
		mcp.Description("Target address as an integer or 0x-prefixed string."),
	}
	if required {
		opts = append(opts, mcp.Required())
	}
	return mcp.WithString(name, opts...)
}

func optionalString(name, description string) mcp.ToolOption {
	return mcp.WithString(name, mcp.Description(description))
}

func requiredString(name, description string) mcp.ToolOption {
	return mcp.WithString(name, mcp.Required(), mcp.Description(description))
}

func requiredInt(name, description string, minVal, maxVal int64) mcp.ToolOption {
	opts := []mcp.PropertyOption{mcp.Required(), mcp.Description(description)}
	if minVal != 0 {
		opts = append(opts, mcp.Min(minVal))
	}
	if maxVal != 0 {
		opts = append(opts, mcp.Max(maxVal))
	}
	return mcp.WithNumber(name, opts...)
}

func enumParam(name string, required bool, values ...string) mcp.ToolOption {
	opts := []mcp.PropertyOption{mcp.Enum(values...)}
	if required {
		opts = append(opts, mcp.Required())
	}
	return mcp.WithString(name, opts...)
}

// ---- tool specs ----

type toolSpec struct {
	name        string
	description string
	operation   string
	params      []mcp.ToolOption
	build       func(args map[string]any) (map[string]any, error)
}

var scanStartModes = []string{"unknown", "equal", "greater", "less", "range", "pointer", "string"}
var scanRefineModes = []string{"equal", "greater", "less", "increased", "decreased", "changed", "unchanged", "range", "pointer", "string"}
var valueTypes = []string{"i8", "i16", "i32", "i64", "f32", "f64"}
var resultValueTypes = []string{"i8", "i16", "i32", "i64", "f32", "f64", "string"}
var savedKinds = []string{"numeric", "pointer", "text"}
var pointerModes = []string{"module", "manual", "array"}
var viewerFormats = []string{"hexadecimal", "hex", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"}
var breakpointModes = []string{"hwbp", "ptebp", "stepbp"}

var breakpointRecordFieldPattern = regexp.MustCompile(
	`(?i)^(?:(?:(?:op|mask)\.)?(?:pc|hit_count|lr|sp|pstate|orig_x0|syscallno|fpsr|fpcr|x(?:[0-9]|[12][0-9])|[qv](?:[0-9]|[12][0-9]|3[01]))|mask(?:[0-9]|1[0-7]|[._](?:[0-9]|1[0-7])|\[(?:[0-9]|1[0-7])\]))$`)

func noParams(args map[string]any) (map[string]any, error) {
	return map[string]any{}, nil
}

func buildToolSpecs() []toolSpec {
	return []toolSpec{
		{
			name:        "android_bridge_ping",
			description: "Diagnose bridge reachability; normal tools connect automatically and do not require this first.",
			operation:   "bridge.ping",
			build:       noParams,
		},
		{
			name:        "android_target_set_pid",
			description: "Bind all scan, viewer, and breakpoint operations to a known PID.",
			operation:   "target.select",
			params:      []mcp.ToolOption{requiredInt("pid", "Process ID to target.", 1, maxAddressValue)},
			build: func(args map[string]any) (map[string]any, error) {
				pid, err := argInt(args, "pid")
				if err != nil {
					return nil, err
				}
				return map[string]any{"pid": pid}, nil
			},
		},
		{
			name:        "android_target_attach_package",
			description: "Resolve a package name to PID and make that process the current target.",
			operation:   "target.attach",
			params:      []mcp.ToolOption{requiredString("package_name", "Android package name.")},
			build: func(args map[string]any) (map[string]any, error) {
				return map[string]any{"package_name": argString(args, "package_name")}, nil
			},
		},
		{
			name:        "android_target_find_pid",
			description: "Resolve a package name to PID without changing the current target.",
			operation:   "target.find",
			params:      []mcp.ToolOption{requiredString("package_name", "Android package name.")},
			build: func(args map[string]any) (map[string]any, error) {
				return map[string]any{"package_name": argString(args, "package_name")}, nil
			},
		},
		{
			name:        "android_target_current",
			description: "Read the current target process bound inside the Android HTTP bridge.",
			operation:   "target.get",
			build:       noParams,
		},
		{
			name:        "android_env_get_params",
			description: "Read ARM64 environment parameters for the current target process.",
			operation:   "env.read",
			params:      []mcp.ToolOption{optionalString("thread_name", "Optional thread name (task->comm).")},
			build: func(args map[string]any) (map[string]any, error) {
				return map[string]any{"thread_name": strings.TrimSpace(argString(args, "thread_name"))}, nil
			},
		},
		{
			name:        "android_module_address",
			description: "Resolve a module segment start or end address from the current target process.",
			operation:   "module.resolve",
			params: []mcp.ToolOption{
				requiredString("module_name", "Module name, e.g. libil2cpp.so."),
				mcp.WithNumber("segment_index", mcp.DefaultNumber(0), mcp.Min(0), mcp.Description("Segment index.")),
				enumParam("which", false, "start", "end"),
			},
			build: func(args map[string]any) (map[string]any, error) {
				which := strings.ToLower(strings.TrimSpace(argString(args, "which")))
				if which == "" {
					which = "start"
				}
				if which != "start" && which != "end" {
					return nil, fmt.Errorf("which must be 'start' or 'end'")
				}
				segmentIndex := int64(0)
				if v, ok := args["segment_index"]; ok && v != nil {
					switch val := v.(type) {
					case float64:
						segmentIndex = int64(val)
					case int64:
						segmentIndex = val
					}
				}
				return map[string]any{
					"module_name":   argString(args, "module_name"),
					"segment_index": segmentIndex,
					"which":         which,
				}, nil
			},
		},
		{
			name:        "android_memory_dump",
			description: "Dump a module name or a half-open address range such as 0x5000-0x6000.",
			operation:   "memory.dump",
			params:      []mcp.ToolOption{requiredString("target", "Module name or start-end address range.")},
			build: func(args map[string]any) (map[string]any, error) {
				target := strings.TrimSpace(argString(args, "target"))
				if target == "" {
					return nil, fmt.Errorf("target must be a module name or start-end address range")
				}
				return map[string]any{"target": target}, nil
			},
		},
		{
			name:        "android_memory_scan_start",
			description: "Start a scan. equal/greater/less/pointer/string need value; range also needs range_max; unknown needs neither.",
			operation:   "scan.start",
			params: []mcp.ToolOption{
				enumParam("mode", true, scanStartModes...),
				mcp.WithString("value_type", mcp.DefaultString("i32"), mcp.Enum(valueTypes...)),
				optionalString("value", "Scan value."),
				optionalString("range_max", "Range end for range mode."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				valueType := strings.TrimSpace(argString(args, "value_type"))
				if valueType == "" {
					valueType = "i32"
				}
				return bridge.BuildScanParams(valueType, argString(args, "mode"), argString(args, "value"), argString(args, "range_max"), true)
			},
		},
		{
			name:        "android_memory_scan_refine",
			description: "Refine results. Value modes need value, range also needs range_max, and history modes need neither.",
			operation:   "scan.refine",
			params: []mcp.ToolOption{
				enumParam("mode", true, scanRefineModes...),
				mcp.WithString("value_type", mcp.DefaultString("i32"), mcp.Enum(valueTypes...)),
				optionalString("value", "Scan value."),
				optionalString("range_max", "Range end for range mode."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				valueType := strings.TrimSpace(argString(args, "value_type"))
				if valueType == "" {
					valueType = "i32"
				}
				return bridge.BuildScanParams(valueType, argString(args, "mode"), argString(args, "value"), argString(args, "range_max"), false)
			},
		},
		{
			name:        "android_memory_scan_results",
			description: "Read one result page; value_type must match the active scan (string for string scans).",
			operation:   "scan.results",
			params: []mcp.ToolOption{
				mcp.WithNumber("start", mcp.DefaultNumber(0), mcp.Min(0), mcp.Description("Page start index.")),
				mcp.WithNumber("count", mcp.DefaultNumber(100), mcp.Min(1), mcp.Max(200), mcp.Description("Page size 1..200.")),
				mcp.WithString("value_type", mcp.DefaultString("i32"), mcp.Enum(resultValueTypes...)),
			},
			build: func(args map[string]any) (map[string]any, error) {
				count := int64(100)
				if v, ok := args["count"]; ok && v != nil {
					switch val := v.(type) {
					case float64:
						count = int64(val)
					case int64:
						count = val
					}
				}
				if count <= 0 || count > 200 {
					return nil, fmt.Errorf("count must be in 1..200")
				}
				start := int64(0)
				if v, ok := args["start"]; ok && v != nil {
					switch val := v.(type) {
					case float64:
						start = int64(val)
					case int64:
						start = val
					}
				}
				valueType := strings.TrimSpace(argString(args, "value_type"))
				if valueType == "" {
					valueType = "i32"
				}
				return map[string]any{"start": start, "count": count, "value_type": valueType}, nil
			},
		},
		{
			name:        "android_memory_scan_status",
			description: "Read the current memory scan progress and result count.",
			operation:   "scan.get",
			build:       noParams,
		},
		{
			name:        "android_memory_scan_clear",
			description: "Clear the current memory scan result set.",
			operation:   "scan.clear",
			build:       noParams,
		},
		{
			name:        "android_memory_read",
			description: "Read 1 to 1048576 raw bytes from any valid target address.",
			operation:   "memory.read",
			params: []mcp.ToolOption{
				addressParam("address", true),
				requiredInt("size", "Number of bytes to read.", 1, 1_048_576),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				size, err := argInt(args, "size")
				if err != nil {
					return nil, err
				}
				if size <= 0 || size > 1024*1024 {
					return nil, fmt.Errorf("size must be in 1..1048576")
				}
				return map[string]any{"address": address, "size": size}, nil
			},
		},
		{
			name:        "android_memory_write",
			description: "Write up to 1048576 bytes as even-length hex digits (whitespace allowed, no 0x prefix).",
			operation:   "memory.write",
			params: []mcp.ToolOption{
				addressParam("address", true),
				requiredString("data_hex", "Even-length hex digits."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				normalized := strings.Join(strings.Fields(argString(args, "data_hex")), "")
				if normalized == "" {
					return nil, fmt.Errorf("data_hex must not be empty")
				}
				if len(normalized)%2 != 0 {
					return nil, fmt.Errorf("data_hex must contain complete bytes")
				}
				if len(normalized)/2 > 1024*1024 {
					return nil, fmt.Errorf("data_hex must contain at most 1048576 bytes")
				}
				return map[string]any{"address": address, "data_hex": normalized}, nil
			},
		},
		{
			name:        "android_saved_list",
			description: "Return the server-owned saved address list with current values and lock states.",
			operation:   "saved.list",
			build:       noParams,
		},
		{
			name:        "android_saved_add",
			description: "Save an address. pointer forces i64; text forces i8 and uses text_length; numeric uses value_type.",
			operation:   "saved.add",
			params: []mcp.ToolOption{
				addressParam("address", true),
				mcp.WithString("value_type", mcp.DefaultString("i32"), mcp.Enum(valueTypes...)),
				mcp.WithString("value_kind", mcp.DefaultString("numeric"), mcp.Enum(savedKinds...)),
				mcp.WithNumber("text_length", mcp.DefaultNumber(64), mcp.Min(1), mcp.Max(256)),
				optionalString("note", "Optional note for the saved address."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				valueType := strings.TrimSpace(argString(args, "value_type"))
				if valueType == "" {
					valueType = "i32"
				}
				valueKind := strings.TrimSpace(argString(args, "value_kind"))
				if valueKind == "" {
					valueKind = "numeric"
				}
				textLength := int64(64)
				if v, ok := args["text_length"]; ok && v != nil {
					switch val := v.(type) {
					case float64:
						textLength = int64(val)
					case int64:
						textLength = val
					}
				}
				return bridge.BuildSavedAddParams(address, valueType, valueKind, int(textLength), argString(args, "note"))
			},
		},
		{
			name:        "android_saved_remove",
			description: "Remove one address and its associated saved lock.",
			operation:   "saved.remove",
			params:      []mcp.ToolOption{addressParam("address", true)},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				return map[string]any{"address": address}, nil
			},
		},
		{
			name:        "android_saved_write",
			description: "Write a saved address using its server-owned type and update its lock value.",
			operation:   "saved.write",
			params: []mcp.ToolOption{
				addressParam("address", true),
				requiredString("value", "New value."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				return map[string]any{"address": address, "value": argString(args, "value")}, nil
			},
		},
		{
			name:        "android_saved_set_note",
			description: "Set or clear the note for one server-owned saved address.",
			operation:   "saved.note.set",
			params: []mcp.ToolOption{
				addressParam("address", true),
				requiredString("note", "Note text; empty clears it."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				return map[string]any{"address": address, "note": argString(args, "note")}, nil
			},
		},
		{
			name:        "android_saved_set_locked",
			description: "Set lock state. An empty value locks the current value; value is ignored when unlocking.",
			operation:   "saved.lock.set",
			params: []mcp.ToolOption{
				addressParam("address", true),
				mcp.WithBoolean("locked", mcp.Required(), mcp.Description("Lock state.")),
				optionalString("value", "Value to lock; empty locks the current value."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				locked, err := argBool(args, "locked")
				if err != nil {
					return nil, err
				}
				value := ""
				if locked {
					value = strings.TrimSpace(argString(args, "value"))
				}
				return map[string]any{"address": address, "locked": locked, "value": value}, nil
			},
		},
		{
			name:        "android_saved_offset",
			description: "Apply a signed hexadecimal byte offset to all server-owned saved addresses.",
			operation:   "saved.offset",
			params:      []mcp.ToolOption{requiredString("offset", "Signed hex offset, e.g. +0x100 or -0x10.")},
			build: func(args map[string]any) (map[string]any, error) {
				return map[string]any{"offset": strings.TrimSpace(argString(args, "offset"))}, nil
			},
		},
		{
			name:        "android_saved_clear",
			description: "Clear the server-owned saved address list and associated saved locks.",
			operation:   "saved.clear",
			build:       noParams,
		},
		{
			name:        "android_pointer_status",
			description: "Read current pointer scan task state and preserved result count.",
			operation:   "pointer.get",
			build:       noParams,
		},
		{
			name:        "android_pointer_scan",
			description: "Start a pointer scan. manual requires manual_base; array requires array_base and array_count.",
			operation:   "pointer.scan",
			params: []mcp.ToolOption{
				addressParam("target", true),
				requiredInt("depth", "Pointer depth 1..16.", 1, 16),
				requiredInt("max_offset", "Maximum byte offset.", 1, maxAddressValue),
				mcp.WithString("mode", mcp.DefaultString("module"), mcp.Enum(pointerModes...)),
				optionalString("manual_base", "Required for manual mode."),
				optionalString("array_base", "Required for array mode."),
				mcp.WithNumber("array_count", mcp.Min(1), mcp.Max(1_000_000), mcp.Description("Required for array mode.")),
				optionalString("module_filter", "Optional module filter, e.g. libil2cpp.so."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				target, err := addressArg(args, "target")
				if err != nil {
					return nil, err
				}
				depth, err := argInt(args, "depth")
				if err != nil {
					return nil, err
				}
				if depth <= 0 || depth > 16 {
					return nil, fmt.Errorf("depth must be in 1..16")
				}
				maxOffset, err := argInt(args, "max_offset")
				if err != nil {
					return nil, err
				}
				if maxOffset <= 0 {
					return nil, fmt.Errorf("max_offset must be greater than 0")
				}
				mode := strings.ToLower(strings.TrimSpace(argString(args, "mode")))
				if mode == "" {
					mode = "module"
				}
				if mode != "module" && mode != "manual" && mode != "array" {
					return nil, fmt.Errorf("mode must be one of: module, manual, array")
				}
				params := map[string]any{
					"target":     target,
					"depth":      depth,
					"max_offset": maxOffset,
				}
				if mode != "module" {
					params["mode"] = mode
				}
				if filter := strings.TrimSpace(argString(args, "module_filter")); filter != "" {
					params["module_filter"] = filter
				}
				switch mode {
				case "manual":
					manualBase := strings.TrimSpace(argString(args, "manual_base"))
					if manualBase == "" {
						return nil, fmt.Errorf("manual mode requires manual_base")
					}
					params["manual_base"] = manualBase
				case "array":
					arrayBase := strings.TrimSpace(argString(args, "array_base"))
					count, ok := args["array_count"]
					if arrayBase == "" || !ok || count == nil {
						return nil, fmt.Errorf("array mode requires array_base and array_count")
					}
					var arrayCount int64
					switch val := count.(type) {
					case float64:
						arrayCount = int64(val)
					case int64:
						arrayCount = val
					}
					if arrayCount <= 0 || arrayCount > 1_000_000 {
						return nil, fmt.Errorf("array_count must be in 1..1000000")
					}
					params["array_base"] = arrayBase
					params["array_count"] = arrayCount
				}
				return params, nil
			},
		},
		{
			name:        "android_pointer_merge",
			description: "Merge all saved pointer bin files by keeping chains with matching offset structure.",
			operation:   "pointer.merge",
			build:       noParams,
		},
		{
			name:        "android_pointer_export",
			description: "Export the merged pointer bin data into a human-readable text file.",
			operation:   "pointer.export",
			build:       noParams,
		},
		{
			name:        "android_memory_view_open",
			description: "Open an address and return its freshly read 100-byte snapshot.",
			operation:   "viewer.open",
			params: []mcp.ToolOption{
				addressParam("address", true),
				mcp.WithString("view_format", mcp.DefaultString("hexadecimal"), mcp.Enum(viewerFormats...)),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				viewFormat, err := bridge.NormalizeViewFormat(argString(args, "view_format"))
				if err != nil {
					return nil, err
				}
				return map[string]any{"address": address, "view_format": viewFormat}, nil
			},
		},
		{
			name:        "android_memory_view_offset",
			description: "Move by an exact byte offset such as '+0x20' or '-0x10' and return the fresh snapshot.",
			operation:   "viewer.seek",
			params:      []mcp.ToolOption{requiredString("offset", "Signed byte offset.")},
			build: func(args map[string]any) (map[string]any, error) {
				return map[string]any{"offset": strings.TrimSpace(argString(args, "offset"))}, nil
			},
		},
		{
			name:        "android_memory_view_set_format",
			description: "Change the viewer format and return the freshly decoded snapshot.",
			operation:   "viewer.format",
			params:      []mcp.ToolOption{enumParam("view_format", true, viewerFormats...)},
			build: func(args map[string]any) (map[string]any, error) {
				viewFormat, err := bridge.NormalizeViewFormat(argString(args, "view_format"))
				if err != nil {
					return nil, err
				}
				return map[string]any{"view_format": viewFormat}, nil
			},
		},
		{
			name:        "android_memory_view_read",
			description: "Refresh and return the current 100-byte snapshot; disasm also returns decoded instructions.",
			operation:   "viewer.refresh",
			build:       noParams,
		},
		{
			name:        "android_breakpoint_get",
			description: "Return the current breakpoint mode and records.",
			operation:   "breakpoint.get",
			build:       noParams,
		},
		{
			name:        "android_breakpoint_set",
			description: "Set breakpoints using mode hwbp/ptebp/stepbp and fully specified point objects. " +
				"Every point object requires: address (nonzero integer or 0x string), bp_type " +
				"(read|write|read_write|execute), bp_scope (main|other|all) and length (integer 1..8). " +
				`Example: [{"address":"0x7A12345678","bp_type":"execute","bp_scope":"all","length":4}]`,
			operation: "breakpoint.set",
			params: []mcp.ToolOption{
				enumParam("mode", true, breakpointModes...),
				mcp.WithArray("points",
					mcp.Required(),
					mcp.MinItems(1),
					mcp.MaxItems(16),
					mcp.Description("Array of 1..16 breakpoint point objects."),
				),
			},
			build: func(args map[string]any) (map[string]any, error) {
				mode := strings.ToLower(strings.TrimSpace(argString(args, "mode")))
				if mode != "hwbp" && mode != "ptebp" && mode != "stepbp" {
					return nil, fmt.Errorf("mode must be one of: hwbp, ptebp, stepbp")
				}
				rawPoints, ok := args["points"].([]any)
				if !ok || len(rawPoints) == 0 {
					return nil, fmt.Errorf("points is required with at least 1 item")
				}
				points := make([]map[string]any, 0, len(rawPoints))
				for _, raw := range rawPoints {
					point, ok := raw.(map[string]any)
					if !ok {
						return nil, fmt.Errorf("points must contain objects with address, bp_type, bp_scope, length")
					}
					points = append(points, point)
				}
				normalized, err := bridge.NormalizeBreakpointPoints(points)
				if err != nil {
					return nil, err
				}
				return map[string]any{"mode": mode, "points": normalized}, nil
			},
		},
		{
			name:        "android_breakpoint_clear",
			description: "Clear the currently active hwbp, ptebp, or stepbp mode.",
			operation:   "breakpoint.clear",
			build:       noParams,
		},
		{
			name:        "android_breakpoint_record_update",
			description: "Patch a flattened record field such as pc, x0..x29, q0..q31, mask0..mask17, or op.<register>.",
			operation:   "breakpoint_record.update",
			params: []mcp.ToolOption{
				requiredInt("index", "Flattened record index.", 0, 0),
				requiredString("field", "Flattened field name."),
				requiredString("value", "New value as hex string or integer."),
			},
			build: func(args map[string]any) (map[string]any, error) {
				index, err := argInt(args, "index")
				if err != nil {
					return nil, err
				}
				if index < 0 {
					return nil, fmt.Errorf("index must be >= 0")
				}
				field := strings.TrimSpace(argString(args, "field"))
				if !breakpointRecordFieldPattern.MatchString(field) {
					return nil, fmt.Errorf("field does not match the allowed register field pattern")
				}
				var value string
				if v, ok := args["value"]; ok {
					switch val := v.(type) {
					case float64:
						value = fmt.Sprintf("0x%X", int64(val))
					case int64:
						value = fmt.Sprintf("0x%X", val)
					default:
						value = strings.TrimSpace(fmt.Sprint(val))
					}
				}
				if value == "" {
					return nil, fmt.Errorf("value is required")
				}
				return map[string]any{"index": index, "field": field, "value": value}, nil
			},
		},
		{
			name:        "android_syscall_stop",
			description: "Stop syscall monitoring for the current target PID.",
			operation:   "syscall.stop",
			build:       noParams,
		},
		{
			name:        "android_syscall_log",
			description: "Read all currently available lsdriver syscall log lines.",
			operation:   "syscall.read",
			build:       noParams,
		},
		{
			name:        "android_cntvct_stop",
			description: "Stop CNTVCT_EL0 read monitoring for the current target PID.",
			operation:   "cntvct.stop",
			build:       noParams,
		},
		{
			name:        "android_cntvct_log",
			description: "Read all currently available lsdriver CNTVCT monitor log lines.",
			operation:   "cntvct.read",
			build:       noParams,
		},
		{
			name:        "android_signature_scan_address",
			description: "Generate an Android-side signature using range_size bytes before and after the address.",
			operation:   "signature.create",
			params: []mcp.ToolOption{
				addressParam("address", true),
				requiredInt("range_size", "Bytes before/after the address 1..1200.", 1, 1200),
				mcp.WithString("file_name", mcp.DefaultString("Signature.txt")),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				rangeSize, err := argInt(args, "range_size")
				if err != nil {
					return nil, err
				}
				if rangeSize <= 0 || rangeSize > 1200 {
					return nil, fmt.Errorf("range_size must be in 1..1200")
				}
				fileName := strings.TrimSpace(argString(args, "file_name"))
				if fileName == "" {
					fileName = "Signature.txt"
				}
				return map[string]any{"address": address, "range": rangeSize, "file_name": fileName}, nil
			},
		},
		{
			name:        "android_signature_scan_file",
			description: "Scan using an Android-side signature file; relative names may resolve under /data/akernel/.",
			operation:   "signature.scan",
			params:      []mcp.ToolOption{mcp.WithString("file_name", mcp.DefaultString("Signature.txt"))},
			build: func(args map[string]any) (map[string]any, error) {
				fileName := strings.TrimSpace(argString(args, "file_name"))
				if fileName == "" {
					fileName = "Signature.txt"
				}
				return map[string]any{"file_name": fileName}, nil
			},
		},
		{
			name:        "android_signature_scan_pattern",
			description: "Scan a pattern such as '48 8B ?? FFh'; '?' and '??' are wildcard bytes.",
			operation:   "signature.match",
			params: []mcp.ToolOption{
				requiredString("pattern", "Hex pattern with wildcards."),
				mcp.WithNumber("range_offset", mcp.DefaultNumber(0), mcp.Min(-2_147_483_648), mcp.Max(2_147_483_647)),
			},
			build: func(args map[string]any) (map[string]any, error) {
				pattern := strings.TrimSpace(argString(args, "pattern"))
				if pattern == "" {
					return nil, fmt.Errorf("pattern must not be empty")
				}
				rangeOffset := int64(0)
				if v, ok := args["range_offset"]; ok && v != nil {
					switch val := v.(type) {
					case float64:
						rangeOffset = int64(val)
					case int64:
						rangeOffset = val
					}
				}
				return map[string]any{"pattern": pattern, "range_offset": rangeOffset}, nil
			},
		},
		{
			name:        "android_signature_filter",
			description: "Filter changed bytes in an Android-side signature file at the supplied address.",
			operation:   "signature.filter",
			params: []mcp.ToolOption{
				addressParam("address", true),
				mcp.WithString("file_name", mcp.DefaultString("Signature.txt")),
			},
			build: func(args map[string]any) (map[string]any, error) {
				address, err := addressArg(args, "address")
				if err != nil {
					return nil, err
				}
				fileName := strings.TrimSpace(argString(args, "file_name"))
				if fileName == "" {
					fileName = "Signature.txt"
				}
				return map[string]any{"address": address, "file_name": fileName}, nil
			},
		},
	}
}

// registerTools registers every bridge operation as an MCP tool.
func registerTools(srv *server.MCPServer, client *bridge.Client) {
	specs := buildToolSpecs()

	// configuration tools
	registerConfigureTool(srv, client)
	registerDiscoverTool(srv, client)

	for _, spec := range specs {
		opts := []mcp.ToolOption{mcp.WithDescription(spec.description)}
		opts = append(opts, spec.params...)
		tool := mcp.NewTool(spec.name, opts...)
		srv.AddTool(tool, handleOp(client, spec.operation, spec.build))
	}

	// memory.map with scan regions stripped for MCP clients
	registerMemoryRegionsTool(srv, client)

	// start operations also append the current log (mirrors LuckyStarMcp.py)
	registerStartWithLogTool(srv, client, "android_syscall_start", "Start syscall monitoring and return the currently available log.", "syscall")
	registerStartWithLogTool(srv, client, "android_cntvct_start", "Start CNTVCT_EL0 read monitoring and return the currently available log.", "cntvct")
}

// registerMemoryRegionsTool exposes memory.map without the scan regions.
func registerMemoryRegionsTool(srv *server.MCPServer, client *bridge.Client) {
	tool := mcp.NewTool(
		"android_memory_regions",
		mcp.WithDescription("Fetch module and segment information only; scan regions are not returned to MCP clients."),
	)
	srv.AddTool(tool, func(ctx context.Context, request mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		resp, err := client.CallOperation("memory.map", nil)
		if err != nil {
			return nil, err
		}
		payload := resp.ToDict()
		if data, ok := payload["data"].(map[string]any); ok {
			delete(data, "regions")
			delete(data, "region_count")
		}
		text, err := json.Marshal(payload)
		if err != nil {
			return nil, err
		}
		return mcp.NewToolResultText(string(text)), nil
	})
}

// registerStartWithLogTool registers a "start" tool that merges the log
// from the matching read operation into the response data.
func registerStartWithLogTool(srv *server.MCPServer, client *bridge.Client, name, description, operation string) {
	tool := mcp.NewTool(name, mcp.WithDescription(description))
	srv.AddTool(tool, func(ctx context.Context, request mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		startResp, err := client.CallOperation(operation+".start", nil)
		if err != nil {
			return nil, err
		}
		payload := startResp.ToDict()
		logResp, logErr := client.CallOperation(operation+".read", nil)
		if logErr == nil && logResp.Ok {
			if data, ok := payload["data"].(map[string]any); ok {
				if logData, ok := logResp.Data.(map[string]any); ok {
					data["log"] = fmt.Sprint(logData["log"])
					data["line_count"] = logData["line_count"]
				}
			}
		}
		text, err := json.Marshal(payload)
		if err != nil {
			return nil, err
		}
		return mcp.NewToolResultText(string(text)), nil
	})
}

func registerConfigureTool(srv *server.MCPServer, client *bridge.Client) {
	tool := mcp.NewTool(
		"configure_android_bridge",
		mcp.WithDescription("Configure the bridge using host='auto', a host/IP plus port, or a full HTTP(S) Tunnel URL."),
		mcp.WithString("host", mcp.DefaultString("auto"), mcp.Description("Target Android IP/host, a full HTTP(S) Tunnel URL, or 'auto' for LAN discovery.")),
		mcp.WithNumber("port", mcp.DefaultNumber(bridge.DefaultPort), mcp.Min(1), mcp.Max(65535)),
		mcp.WithNumber("timeout_seconds", mcp.DefaultNumber(bridge.DefaultTimeoutSeconds), mcp.Min(0.01), mcp.Description("Timeout in seconds for Android HTTP bridge requests.")),
	)
	srv.AddTool(tool, func(ctx context.Context, request mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		args, err := argsOf(request)
		if err != nil {
			return nil, err
		}
		host := strings.TrimSpace(argString(args, "host"))
		if host == "" {
			host = "auto"
		}
		port := int64(bridge.DefaultPort)
		if v, ok := args["port"]; ok && v != nil {
			switch val := v.(type) {
			case float64:
				port = int64(val)
			case int64:
				port = val
			}
		}
		timeout := bridge.DefaultTimeoutSeconds
		if v, ok := args["timeout_seconds"]; ok && v != nil {
			switch val := v.(type) {
			case float64:
				timeout = val
			case int64:
				timeout = float64(val)
			}
		}
		if err := client.Configure(host, int(port), timeout); err != nil {
			return nil, err
		}
		payload, _ := json.Marshal(client.ConnectionState())
		return mcp.NewToolResultText(string(payload)), nil
	})
}

func registerDiscoverTool(srv *server.MCPServer, client *bridge.Client) {
	tool := mcp.NewTool(
		"discover_android_bridges",
		mcp.WithDescription("Discover Android HTTP bridge candidates on the LAN and show the current bridge state."),
	)
	srv.AddTool(tool, func(ctx context.Context, request mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		payload, err := json.Marshal(client.Discover())
		if err != nil {
			return nil, err
		}
		return mcp.NewToolResultText(string(payload)), nil
	})
}
