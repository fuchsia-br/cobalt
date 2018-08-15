package config_parser

import "testing"

func TestIdFromName(t *testing.T) {
	id := idFromName("test_name")
	expected := uint32(0x8b85b08d)
	if id != expected {
		t.Errorf("%x != %x", id, expected)
	}
}
