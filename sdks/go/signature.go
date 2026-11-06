package glance

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"fmt"
	"math"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const TimestampHeader = "X-GlanceWidgets-Timestamp"
const SignatureHeader = "X-GlanceWidgets-Signature"
const DefaultSignatureToleranceMs = 300000

var timestampPattern = regexp.MustCompile(`^[0-9]+$`)

func sign(timestamp any, secret string) string {
	if _, isText := timestamp.(string); !isText {
		timestamp = numberString(number(timestamp))
	}
	mac := hmac.New(sha256.New, []byte(secret))
	fmt.Fprintf(mac, "%v\n%s", timestamp, secret)
	return base64.StdEncoding.EncodeToString(mac.Sum(nil))
}

// ECMAScript decimal/exponent thresholds, with shortest round-trip digits.
func numberString(value float64) string {
	if value == 0 {
		return "0"
	}
	if math.IsInf(value, 1) {
		return "Infinity"
	}
	if math.IsInf(value, -1) {
		return "-Infinity"
	}
	if math.IsNaN(value) {
		return "NaN"
	}
	if math.Abs(value) >= 1e-6 && math.Abs(value) < 1e21 {
		return strconv.FormatFloat(value, 'f', -1, 64)
	}
	formatted := strconv.FormatFloat(value, 'g', -1, 64)
	parts := strings.Split(formatted, "e")
	if len(parts) == 2 {
		exponent, _ := strconv.Atoi(parts[1])
		return fmt.Sprintf("%se%+d", parts[0], exponent)
	}
	return formatted
}
func verifySignature(request Object, secret string, o Object) bool {
	timestamp, tok := request["timestamp"].(string)
	signature, sok := request["signature"].(string)
	if !tok || !sok || !timestampPattern.MatchString(timestamp) {
		return false
	}
	ts, err := strconv.ParseFloat(timestamp, 64)
	if err != nil || math.Abs(n(o, "now", float64(time.Now().UnixMilli()))-ts) > n(o, "toleranceMs", DefaultSignatureToleranceMs) {
		return false
	}
	return hmac.Equal([]byte(signature), []byte(sign(timestamp, secret)))
}
func Sign(timestamp any, secret string) (string, error) {
	return safe(func() string { return sign(timestamp, secret) })
}
func VerifySignature(request Object, secret string, options ...Object) bool {
	ok, err := safe(func() bool { return verifySignature(request, secret, opt(options)) })
	return err == nil && ok
}
