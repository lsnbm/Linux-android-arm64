package mcp

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"time"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
)

const testProtocolVersion = "2025-06-18"

// mockBridge mimics the Android HTTP bridge RPC endpoint.
func mockBridge(t *testing.T, record *sync.Map) *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/api/rpc" {
			t.Errorf("unexpected path %s", r.URL.Path)
			w.WriteHeader(404)
			return
		}
		var req map[string]any
		json.NewDecoder(r.Body).Decode(&req)
		operation, _ := req["operation"].(string)
		record.Store(operation, req["params"])

		data := map[string]any{"pid": 4242}
		switch operation {
		case "scan.get":
			data = map[string]any{"scanning": false, "progress": 100, "count": 42}
		case "scan.results":
			data = map[string]any{
				"start": 0, "total_count": 42,
				"items": []any{map[string]any{"addr_hex": "0x7A00000000", "value": "100"}},
			}
		case "memory.map":
			data = map[string]any{
				"status": 0, "module_count": 1, "region_count": 2,
				"modules": []any{},
				"regions": []any{map[string]any{"start": 1, "end": 2}},
			}
		case "syscall.read":
			data = map[string]any{"log": "line1\nline2", "line_count": 2}
		case "breakpoint.set":
			data = map[string]any{"ok": true}
		}
		payload := map[string]any{
			"ok": true, "operation": operation, "data": data,
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(payload)
	}))
}

var testSessionID string

func rpcCall(t *testing.T, url string, id int, method string, params any) map[string]any {
	t.Helper()
	body := map[string]any{"jsonrpc": "2.0", "id": id, "method": method}
	if params != nil {
		body["params"] = params
	}
	raw, _ := json.Marshal(body)
	req, err := http.NewRequest(http.MethodPost, url, bytes.NewReader(raw))
	if err != nil {
		t.Fatal(err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json, text/event-stream")
	if testSessionID != "" {
		req.Header.Set("mcp-session-id", testSessionID)
	}
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		t.Fatalf("%s request failed: %v", method, err)
	}
	defer resp.Body.Close()
	if id := resp.Header.Get("mcp-session-id"); id != "" && testSessionID == "" {
		testSessionID = id
	}
	payload, _ := io.ReadAll(resp.Body)
	var result map[string]any
	if err := json.Unmarshal(payload, &result); err != nil {
		t.Fatalf("bad JSON response (%d): %s", resp.StatusCode, string(payload))
	}
	return result
}

func freePort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := ln.Addr().(*net.TCPAddr).Port
	ln.Close()
	return port
}

func TestMCPServerEndToEnd(t *testing.T) {
	var recorded sync.Map
	mock := mockBridge(t, &recorded)
	defer mock.Close()

	port := freePort(t)
	cfg := Config{
		BindHost:       "127.0.0.1",
		BindPort:       port,
		Path:           "/mcp",
		AndroidHost:    "127.0.0.1",
		AndroidPort:    mock.Listener.Addr().(*net.TCPAddr).Port,
		AndroidTimeout: 4,
	}
	done := make(chan error, 1)
	go func() { done <- Run(cfg) }()
	t.Cleanup(func() {
		select {
		case <-done:
		case <-time.After(200 * time.Millisecond):
		}
	})

	base := fmt.Sprintf("http://127.0.0.1:%d/mcp", port)

	// initialize
	initResult := rpcCall(t, base, 1, "initialize", map[string]any{
		"protocolVersion": testProtocolVersion,
		"capabilities":    map[string]any{},
		"clientInfo":      map[string]any{"name": "test", "version": "1.0"},
	})
	if _, ok := initResult["error"]; ok {
		t.Fatalf("initialize failed: %v", initResult["error"])
	}
	rpcCall(t, base, 2, "notifications/initialized", map[string]any{})

	// tools/list
	listResult := rpcCall(t, base, 3, "tools/list", nil)
	if errText, ok := listResult["error"]; ok {
		t.Fatalf("tools/list failed: %v", errText)
	}
	toolsRaw, ok := listResult["result"].(map[string]any)["tools"].([]any)
	if !ok {
		t.Fatalf("tools/list result missing tools: %v", listResult)
	}
	if len(toolsRaw) != 48 {
		t.Fatalf("expected 48 tools, got %d", len(toolsRaw))
	}
	toolNames := map[string]bool{}
	for _, raw := range toolsRaw {
		tool := raw.(map[string]any)
		toolNames[tool["name"].(string)] = true
	}
	for _, name := range []string{
		"configure_android_bridge", "discover_android_bridges", "android_bridge_ping",
		"android_target_set_pid", "android_memory_scan_start", "android_memory_scan_refine",
		"android_memory_read", "android_saved_add", "android_pointer_scan",
		"android_breakpoint_set", "android_breakpoint_record_update",
		"android_syscall_start", "android_syscall_log", "android_cntvct_start",
		"android_signature_scan_pattern", "android_memory_view_open",
	} {
		if !toolNames[name] {
			t.Fatalf("missing tool %s", name)
		}
	}

	// tools/call: simple op
	ping := rpcCall(t, base, 4, "tools/call", map[string]any{
		"name": "android_bridge_ping", "arguments": map[string]any{},
	})
	if errText, ok := ping["error"]; ok {
		t.Fatalf("ping failed: %v", errText)
	}
	content := ping["result"].(map[string]any)["content"].([]any)
	text := content[0].(map[string]any)["text"].(string)
	if !strings.Contains(text, `"operation":"bridge.ping"`) {
		t.Fatalf("unexpected ping response: %s", text)
	}

	// tools/call: scan start with param validation
	scan := rpcCall(t, base, 5, "tools/call", map[string]any{
		"name": "android_memory_scan_start",
		"arguments": map[string]any{"mode": "equal", "value": "100"},
	})
	if errText, ok := scan["error"]; ok {
		t.Fatalf("scan failed: %v", errText)
	}
	sent, ok := recorded.Load("scan.start")
	if !ok {
		t.Fatal("mock bridge did not receive scan.start")
	}
	params := sent.(map[string]any)
	if params["mode"] != "equal" || params["value_type"] != "i32" || params["value"] != "100" {
		t.Fatalf("unexpected scan params: %v", params)
	}

	// tools/call: invalid input returns error
	bad := rpcCall(t, base, 6, "tools/call", map[string]any{
		"name": "android_memory_scan_start",
		"arguments": map[string]any{"mode": "increased"},
	})
	if _, ok := bad["error"]; !ok {
		t.Fatalf("history mode on start must fail, got %v", bad)
	}

	// tools/call: memory regions strip regions
	regions := rpcCall(t, base, 7, "tools/call", map[string]any{
		"name": "android_memory_regions", "arguments": map[string]any{},
	})
	content = regions["result"].(map[string]any)["content"].([]any)
	text = content[0].(map[string]any)["text"].(string)
	if strings.Contains(text, `"regions"`) {
		t.Fatalf("regions must be stripped: %s", text)
	}

	// tools/call: syscall start merges the log
	syscall := rpcCall(t, base, 8, "tools/call", map[string]any{
		"name": "android_syscall_start", "arguments": map[string]any{},
	})
	content = syscall["result"].(map[string]any)["content"].([]any)
	text = content[0].(map[string]any)["text"].(string)
	if !strings.Contains(text, `"log":"line1\nline2"`) {
		t.Fatalf("syscall start must merge log: %s", text)
	}

	// resources/list
	resList := rpcCall(t, base, 9, "resources/list", nil)
	resRaw := resList["result"].(map[string]any)["resources"].([]any)
	if len(resRaw) != 1 {
		t.Fatalf("expected 1 resource, got %d", len(resRaw))
	}

	// resources/read
	resRead := rpcCall(t, base, 10, "resources/read", map[string]any{
		"uri": "android://connection",
	})
	contents := resRead["result"].(map[string]any)["contents"].([]any)
	if len(contents) != 1 {
		t.Fatalf("expected 1 resource content, got %d", len(contents))
	}
	resText := contents[0].(map[string]any)["text"].(string)
	if !strings.Contains(resText, `"host":"127.0.0.1"`) {
		t.Fatalf("unexpected connection state: %s", resText)
	}
}

func TestStartStopRestart(t *testing.T) {
	port := freePort(t)
	cfg := Config{
		BindHost: "127.0.0.1", BindPort: port, Path: "/mcp",
		AndroidHost: "auto", AndroidPort: 9494, AndroidTimeout: 4,
	}

	// start
	srv, err := Start(cfg)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(srv.URL(), fmt.Sprintf(":%d/mcp", port)) {
		t.Fatalf("unexpected URL %s", srv.URL())
	}
	// second start on the same port must fail
	if _, err := Start(cfg); err == nil {
		t.Fatal("second Start on the same port must fail")
	}

	// stop and restart on the same port
	if err := srv.Stop(); err != nil {
		t.Fatal(err)
	}
	srv2, err := Start(cfg)
	if err != nil {
		t.Fatalf("restart after Stop failed: %v", err)
	}
	srv2.Stop()
}
