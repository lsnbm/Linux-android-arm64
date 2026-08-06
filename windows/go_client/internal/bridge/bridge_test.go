package bridge

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"reflect"
	"testing"
)

func TestBuildScanParams(t *testing.T) {
	params, err := BuildScanParams("i32", "eq", "100", "", true)
	if err != nil {
		t.Fatal(err)
	}
	want := map[string]any{"mode": "equal", "value_type": "i32", "value": "100"}
	if !reflect.DeepEqual(params, want) {
		t.Fatalf("got %v want %v", params, want)
	}

	params, err = BuildScanParams("i64", "ptr", "7F12345678", "", false)
	if err != nil {
		t.Fatal(err)
	}
	if params["value_type"] != "i64" {
		t.Fatalf("pointer mode must force i64, got %v", params["value_type"])
	}

	if _, err = BuildScanParams("i32", "inc", "", "", true); err == nil {
		t.Fatal("history mode on first scan must fail")
	}
	if _, err = BuildScanParams("i32", "unknown", "", "", false); err == nil {
		t.Fatal("unknown mode on refine must fail")
	}
	if _, err = BuildScanParams("i32", "range", "10", "", true); err == nil {
		t.Fatal("range without range_max must fail")
	}
	params, err = BuildScanParams("i32", "range", "10", "0x1000", true)
	if err != nil {
		t.Fatal(err)
	}
	if params["range_max"] != "0x1000" {
		t.Fatalf("range_max missing: %v", params)
	}
	params, err = BuildScanParams("i32", "str", "hello", "", true)
	if err != nil {
		t.Fatal(err)
	}
	if params["mode"] != "string" {
		t.Fatalf("got %v", params)
	}
	if _, exists := params["value_type"]; exists {
		t.Fatal("string scan must not carry value_type")
	}
}

func TestBuildSavedAddParams(t *testing.T) {
	params, err := BuildSavedAddParams(0x1234, "i32", "numeric", 64, "")
	if err != nil {
		t.Fatal(err)
	}
	if params["address"] != "0x1234" || params["value_type"] != "i32" || params["value_kind"] != "numeric" {
		t.Fatalf("got %v", params)
	}
	params, err = BuildSavedAddParams(0x1234, "i32", "pointer", 64, "")
	if err != nil {
		t.Fatal(err)
	}
	if params["value_type"] != "i64" {
		t.Fatalf("pointer kind must force i64, got %v", params["value_type"])
	}
	params, err = BuildSavedAddParams(0x1234, "i32", "text", 64, "note")
	if err != nil {
		t.Fatal(err)
	}
	if params["value_type"] != "i8" || params["note"] != "note" {
		t.Fatalf("got %v", params)
	}
	if _, err = BuildSavedAddParams(0x1234, "i32", "numeric", 0, ""); err == nil {
		t.Fatal("text_length 0 must fail")
	}
}

func TestNormalizeBreakpointPoints(t *testing.T) {
	points := []map[string]any{
		{"address": 0x7A12345678, "bp_type": "execute", "bp_scope": "all", "length": 4},
	}
	normalized, err := NormalizeBreakpointPoints(points)
	if err != nil {
		t.Fatal(err)
	}
	if normalized[0]["address"] != "0x7A12345678" || normalized[0]["length"] != int64(4) {
		t.Fatalf("got %v", normalized)
	}
	if _, err = NormalizeBreakpointPoints(nil); err == nil {
		t.Fatal("empty points must fail")
	}
	if _, err = NormalizeBreakpointPoints([]map[string]any{{"address": 1, "bp_type": "read", "bp_scope": "all", "length": 9}}); err == nil {
		t.Fatal("length 9 must fail")
	}
}

func TestParseSavedStates(t *testing.T) {
	data := map[string]any{
		"items": []any{
			map[string]any{
				"address": 0x1234, "address_hex": "0x1234", "value_type": "i32",
				"value_type_label": "I32", "value_kind": "numeric", "text_length": 64,
				"note": "n", "value": "10", "locked": true, "lock_value": "10",
			},
		},
	}
	states, err := ParseSavedStates(data)
	if err != nil {
		t.Fatal(err)
	}
	if len(states) != 1 || states[0].Address != 0x1234 || !states[0].Locked || states[0].ValueTypeLabel != "I32" {
		t.Fatalf("got %+v", states)
	}
	if _, err = ParseSavedStates(map[string]any{"items": "bad"}); err == nil {
		t.Fatal("non-list items must fail")
	}
}

func TestBridgeRequest(t *testing.T) {
	var gotBody map[string]any
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/api/rpc" {
			t.Fatalf("unexpected path %s", r.URL.Path)
		}
		if r.Header.Get("User-Agent") != "LS-KTool-Windows-Bridge/1" {
			t.Fatalf("bad user agent %s", r.Header.Get("User-Agent"))
		}
		json.NewDecoder(r.Body).Decode(&gotBody)
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"ok":true,"operation":"bridge.ping","data":{"pid":123}}`))
	}))
	defer server.Close()

	b := New(DefaultTimeoutSeconds)
	if err := b.Connect(server.URL, 80); err != nil {
		t.Fatal(err)
	}
	resp, err := b.CallOperation("bridge.ping", nil)
	if err != nil {
		t.Fatal(err)
	}
	if !resp.Ok || resp.Operation != "bridge.ping" {
		t.Fatalf("got %+v", resp)
	}
	data, _ := resp.Data.(map[string]any)
	if n, err := toInt(data["pid"]); err != nil || n != 123 {
		t.Fatalf("got %v", data)
	}
	if gotBody["operation"] != "bridge.ping" {
		t.Fatalf("bad request body %v", gotBody)
	}
}

func TestBridgeErrors(t *testing.T) {
	b := New(DefaultTimeoutSeconds)
	if _, err := b.CallOperation("x", nil); err == nil {
		t.Fatal("call without connect must fail")
	} else if oe, ok := err.(*OpError); !ok || oe.Kind != KindConnection {
		t.Fatalf("got %v", err)
	}

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(500)
		w.Write([]byte(`{"error":"boom"}`))
	}))
	defer server.Close()
	if err := b.Connect(server.URL, 80); err != nil {
		t.Fatal(err)
	}
	if _, err := b.CallOperation("x", nil); err == nil {
		t.Fatal("500 must fail")
	}
}

func TestParseEndpoint(t *testing.T) {
	ep, err := parseEndpoint("192.168.1.10", 9494)
	if err != nil {
		t.Fatal(err)
	}
	if ep.scheme != "http" || ep.host != "192.168.1.10" || ep.port != 9494 || ep.rpcPath != "/api/rpc" {
		t.Fatalf("got %+v", ep)
	}
	ep, err = parseEndpoint("https://abc.trycloudflare.com", 9494)
	if err != nil {
		t.Fatal(err)
	}
	if ep.scheme != "https" || ep.port != 443 {
		t.Fatalf("got %+v", ep)
	}
	ep, err = parseEndpoint("http://host:8080/foo", 9494)
	if err != nil {
		t.Fatal(err)
	}
	if ep.port != 8080 || ep.rpcPath != "/foo/api/rpc" {
		t.Fatalf("got %+v", ep)
	}
	if _, err = parseEndpoint("ftp://x", 9494); err == nil {
		t.Fatal("ftp must fail")
	}
	if _, err = parseEndpoint("", 9494); err == nil {
		t.Fatal("empty host must fail")
	}
	if _, err = parseEndpoint("1.2.3.4", 0); err == nil {
		t.Fatal("port 0 must fail")
	}
}
