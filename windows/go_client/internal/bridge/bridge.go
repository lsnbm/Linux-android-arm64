package bridge

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	DefaultPort            = 9494
	DefaultTimeoutSeconds  = 4.0
	MinHTTPSTimeoutSeconds = 15.0
	MaxResponseBytes       = 16 * 1024 * 1024
	RPCPath                = "/api/rpc"
	UserAgent              = "LS-KTool-Windows-Bridge/1"
)

type ErrorKind int

const (
	KindGeneric ErrorKind = iota
	KindConnection
	KindOutcomeUnknown
	KindProtocol
)

type OpError struct {
	Kind ErrorKind
	Msg  string
}

func (e *OpError) Error() string { return e.Msg }

func NewError(kind ErrorKind, format string, a ...any) *OpError {
	return &OpError{Kind: kind, Msg: fmt.Sprintf(format, a...)}
}

// Response is the bridge envelope: {"ok", "operation", "error", "data"}.
type Response struct {
	Ok         bool
	Operation  string
	Data       any
	Error      string
	Connection map[string]any
}

// ToDict renders the envelope as a JSON-friendly map (mirrors BridgeResponse.to_dict).
func (r *Response) ToDict() map[string]any {
	payload := map[string]any{
		"ok":         r.Ok,
		"operation":  r.Operation,
		"connection": r.Connection,
	}
	if r.Data != nil {
		payload["data"] = r.Data
	}
	if r.Error != "" {
		payload["error"] = r.Error
	}
	return payload
}

type endpoint struct {
	scheme  string
	host    string
	port    int
	rpcPath string
}

func (e *endpoint) displayURL() string {
	defaultPort := 80
	if e.scheme == "https" {
		defaultPort = 443
	}
	authority := e.host
	if e.port != defaultPort {
		authority = fmt.Sprintf("%s:%d", e.host, e.port)
	}
	return e.scheme + "://" + authority
}

// Bridge is the Windows-side HTTP facade for the Android bridge.
// It mirrors AndroidHttpBridge from the original http_bridge.py.
type Bridge struct {
	mu      sync.Mutex
	ep      *endpoint
	timeout float64
	client  *http.Client
}

func New(timeoutSeconds float64) *Bridge {
	if timeoutSeconds <= 0 {
		timeoutSeconds = DefaultTimeoutSeconds
	}
	return &Bridge{timeout: timeoutSeconds, client: &http.Client{}}
}

func parseEndpoint(host string, port int) (*endpoint, error) {
	hostText := strings.TrimSpace(host)
	if hostText == "" {
		return nil, NewError(KindConnection, "host must not be empty")
	}
	if !strings.Contains(hostText, "://") {
		if port <= 0 || port > 65535 {
			return nil, NewError(KindConnection, "port must be in 1..65535")
		}
		return &endpoint{scheme: "http", host: hostText, port: port, rpcPath: RPCPath}, nil
	}

	parsed, err := url.Parse(hostText)
	if err != nil || (parsed.Scheme != "http" && parsed.Scheme != "https") || parsed.Hostname() == "" {
		return nil, NewError(KindConnection, "public endpoint must be an http:// or https:// URL")
	}
	if parsed.RawQuery != "" || parsed.Fragment != "" {
		return nil, NewError(KindConnection, "public endpoint must not contain query or fragment")
	}

	endpointPort := 80
	if parsed.Scheme == "https" {
		endpointPort = 443
	}
	if p := parsed.Port(); p != "" {
		if n, err := strconv.Atoi(p); err == nil {
			endpointPort = n
		}
	}
	path := strings.TrimRight(parsed.Path, "/")
	rpcPath := RPCPath
	if path != "" {
		if strings.HasSuffix(path, RPCPath) {
			rpcPath = path
		} else {
			rpcPath = path + RPCPath
		}
	}
	return &endpoint{scheme: parsed.Scheme, host: parsed.Hostname(), port: endpointPort, rpcPath: rpcPath}, nil
}

func (b *Bridge) IsConnected() bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.ep != nil
}

func (b *Bridge) Connect(host string, port int) error {
	ep, err := parseEndpoint(host, port)
	if err != nil {
		return err
	}
	b.mu.Lock()
	b.ep = ep
	b.mu.Unlock()
	return nil
}

func (b *Bridge) Disconnect() {
	b.mu.Lock()
	b.ep = nil
	b.mu.Unlock()
}

func (b *Bridge) URL() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.ep == nil {
		return ""
	}
	return b.ep.displayURL()
}

func (b *Bridge) Host() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.ep == nil {
		return ""
	}
	return b.ep.host
}

func (b *Bridge) Port() int {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.ep == nil {
		return DefaultPort
	}
	return b.ep.port
}

func (b *Bridge) CallOperation(operation string, params map[string]any) (*Response, error) {
	if params == nil {
		params = map[string]any{}
	}
	return b.request(map[string]any{"operation": strings.TrimSpace(operation), "params": params})
}

func (b *Bridge) request(req map[string]any) (*Response, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.ep == nil {
		return nil, NewError(KindConnection, "bridge endpoint is not configured")
	}
	ep := b.ep
	operation, _ := req["operation"].(string)

	body, err := json.Marshal(req)
	if err != nil {
		return nil, NewError(KindProtocol, "failed to encode request: %v", err)
	}

	timeout := b.timeout
	if ep.scheme == "https" && timeout < MinHTTPSTimeoutSeconds {
		timeout = MinHTTPSTimeoutSeconds
	}
	b.client.Timeout = time.Duration(timeout * float64(time.Second))

	httpReq, err := http.NewRequest(http.MethodPost, ep.displayURL()+ep.rpcPath, bytes.NewReader(body))
	if err != nil {
		return nil, NewError(KindConnection, "failed to build HTTP request: %v", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Accept", "application/json")
	httpReq.Header.Set("User-Agent", UserAgent)

	httpResp, err := b.client.Do(httpReq)
	if err != nil {
		var netErr net.Error
		if errors.As(err, &netErr) && netErr.Timeout() {
			hint := ""
			if ep.scheme == "https" {
				hint = "；Quick Tunnel 重启后域名会变化，请核对 /sdcard/log.txt 中最新公网地址"
			}
			return nil, NewError(KindOutcomeUnknown,
				"请求 %s 等待 %g 秒仍未收到响应，当前地址：%s%s；请求可能已在 Android 端执行",
				operation, timeout, ep.displayURL(), hint)
		}
		return nil, NewError(KindConnection, "HTTP request to %s failed: %v", ep.displayURL(), err)
	}
	defer httpResp.Body.Close()

	if cl := httpResp.Header.Get("Content-Length"); cl != "" {
		n, err := strconv.ParseInt(cl, 10, 64)
		if err != nil {
			return nil, NewError(KindProtocol, "android HTTP Content-Length is invalid")
		}
		if n > MaxResponseBytes {
			return nil, NewError(KindProtocol, "android HTTP response exceeds the maximum size")
		}
	}

	payload, err := io.ReadAll(io.LimitReader(httpResp.Body, MaxResponseBytes+1))
	if err != nil {
		return nil, NewError(KindConnection, "failed to read HTTP response: %v", err)
	}
	if len(payload) > MaxResponseBytes {
		return nil, NewError(KindProtocol, "android HTTP response exceeds the maximum size")
	}

	var obj map[string]any
	if err := json.Unmarshal(payload, &obj); err != nil {
		return nil, NewError(KindProtocol,
			"android HTTP response is not valid JSON (status %d): %s",
			httpResp.StatusCode, strings.TrimSpace(string(payload)))
	}
	if httpResp.StatusCode < 200 || httpResp.StatusCode >= 300 {
		errText := httpResp.Status
		if e, ok := obj["error"].(string); ok && e != "" {
			errText = e
		}
		return nil, NewError(KindConnection,
			"android HTTP request failed with status %d: %s", httpResp.StatusCode, errText)
	}

	ok, _ := obj["ok"].(bool)
	op, _ := obj["operation"].(string)
	if op == "" {
		op = operation
	}
	var errText string
	if !ok {
		errText, _ = obj["error"].(string)
	}
	return &Response{
		Ok:         ok,
		Operation:  op,
		Data:       obj["data"],
		Error:      errText,
		Connection: map[string]any{
			"host":           ep.displayURL(),
			"resolved_host":  ep.host,
			"port":           ep.port,
			"scheme":         ep.scheme,
			"timeout_seconds": timeout,
		},
	}, nil
}
