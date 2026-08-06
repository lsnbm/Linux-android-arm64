package bridge

import (
	"testing"
	"time"
)

func TestDiscoverLanDevices(t *testing.T) {
	start := time.Now()
	devices := DiscoverLanDevices()
	t.Logf("discovery took %s, found %d devices", time.Since(start), len(devices))
	for _, d := range devices {
		t.Logf("  %s [%s]", d.Host, d.Mac)
	}
}
