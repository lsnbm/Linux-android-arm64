package mcp

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"strings"

	"github.com/mark3labs/mcp-go/mcp"
	"github.com/mark3labs/mcp-go/server"

	"luckystar/internal/bridge"
)

const (
	DefaultBindHost = "127.0.0.1"
	DefaultBindPort = 14447
	DefaultPath     = "/mcp"
)

type Config struct {
	BindHost       string
	BindPort       int
	Path           string
	AndroidHost    string
	AndroidPort    int
	AndroidTimeout float64
}

// Server is a runnable MCP streamable-HTTP server.
type Server struct {
	cfg      Config
	client   *bridge.Client
	httpSrv  *http.Server
	listener net.Listener
	done     chan error
}

// Run starts the MCP server and blocks until it is stopped or fails.
func Run(cfg Config) error {
	srv, err := Start(cfg)
	if err != nil {
		return err
	}
	defer srv.Stop()
	return <-srv.done
}

// Start launches the MCP server in the background.
func Start(cfg Config) (*Server, error) {
	client := bridge.NewClient(cfg.AndroidHost, cfg.AndroidPort, cfg.AndroidTimeout)

	mcpServer := server.NewMCPServer(
		"NativeHttpBridge Android MCP",
		"1.0.0",
	)
	registerTools(mcpServer, client)
	registerResource(mcpServer, client)

	endpointPath := normalizePath(cfg.Path)
	opts := []server.StreamableHTTPOption{server.WithEndpointPath(endpointPath)}
	if cfg.BindHost == "0.0.0.0" || cfg.BindHost == "::" {
		opts = append(opts, server.WithDisableLocalhostProtection(true))
	}
	handler := server.NewStreamableHTTPServer(mcpServer, opts...)

	listener, err := net.Listen("tcp", fmt.Sprintf("%s:%d", cfg.BindHost, cfg.BindPort))
	if err != nil {
		return nil, err
	}

	s := &Server{
		cfg:      cfg,
		client:   client,
		httpSrv:  &http.Server{Handler: handler},
		listener: listener,
		done:     make(chan error, 1),
	}
	go func() {
		err := s.httpSrv.Serve(listener)
		if err != nil && err != http.ErrServerClosed {
			s.done <- err
			return
		}
		s.done <- nil
	}()
	fmt.Printf("[MCP] Server started:\n")
	fmt.Printf("  Streamable HTTP: %s\n", s.URL())
	return s, nil
}

// Stop shuts the MCP HTTP server down.
func (s *Server) Stop() error {
	return s.httpSrv.Close()
}

// URL returns the endpoint clients should connect to.
func (s *Server) URL() string {
	port := s.cfg.BindPort
	if s.listener != nil {
		if addr, ok := s.listener.Addr().(*net.TCPAddr); ok {
			port = addr.Port
		}
	}
	return fmt.Sprintf("http://%s:%d%s", displayHost(s.cfg.BindHost), port, normalizePath(s.cfg.Path))
}

func normalizePath(path string) string {
	normalized := strings.TrimSpace(path)
	if normalized == "" {
		normalized = DefaultPath
	}
	if !strings.HasPrefix(normalized, "/") {
		normalized = "/" + normalized
	}
	if len(normalized) > 1 {
		normalized = strings.TrimRight(normalized, "/")
	}
	if normalized == "" {
		normalized = DefaultPath
	}
	return normalized
}

func displayHost(host string) string {
	if host == "0.0.0.0" || host == "::" {
		return "127.0.0.1"
	}
	return host
}

// argsOf extracts the tool arguments map, tolerating a nil payload.
func argsOf(request mcp.CallToolRequest) (map[string]any, error) {
	if request.Params.Arguments == nil {
		return map[string]any{}, nil
	}
	args, ok := request.Params.Arguments.(map[string]any)
	if !ok {
		return nil, fmt.Errorf("invalid arguments payload")
	}
	return args, nil
}

// handleOp builds a tool handler that calls a bridge operation and returns
// the response envelope as JSON text.
func handleOp(client *bridge.Client, operation string, build func(args map[string]any) (map[string]any, error)) server.ToolHandlerFunc {
	return func(ctx context.Context, request mcp.CallToolRequest) (*mcp.CallToolResult, error) {
		args, err := argsOf(request)
		if err != nil {
			return nil, err
		}
		params, err := build(args)
		if err != nil {
			return nil, fmt.Errorf("%s", err.Error())
		}
		resp, err := client.CallOperation(operation, params)
		if err != nil {
			return nil, err
		}
		payload, err := json.Marshal(resp.ToDict())
		if err != nil {
			return nil, err
		}
		return mcp.NewToolResultText(string(payload)), nil
	}
}

// registerResource exposes the android://connection resource.
func registerResource(srv *server.MCPServer, client *bridge.Client) {
	resource := mcp.NewResource(
		"android://connection",
		"Android Bridge Connection",
		mcp.WithResourceDescription("Current Android HTTP connection settings used by this MCP server."),
		mcp.WithMIMEType("application/json"),
	)
	srv.AddResource(resource, func(ctx context.Context, request mcp.ReadResourceRequest) ([]mcp.ResourceContents, error) {
		payload, err := json.Marshal(client.ConnectionState())
		if err != nil {
			return nil, err
		}
		return []mcp.ResourceContents{
			&mcp.TextResourceContents{
				URI:      "android://connection",
				MIMEType: "application/json",
				Text:     string(payload),
			},
		}, nil
	})
}
