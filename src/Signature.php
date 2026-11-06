<?php
declare(strict_types=1);
namespace GlanceWidgets;

const TIMESTAMP_HEADER = 'X-GlanceWidgets-Timestamp';
const SIGNATURE_HEADER = 'X-GlanceWidgets-Signature';
const DEFAULT_SIGNATURE_TOLERANCE_MS = 300000;

function sign(string|int|float $timestamp, string $secret): string
{
    $timestamp = is_string($timestamp) ? $timestamp : Numbers::text($timestamp);
    return base64_encode(hash_hmac('sha256', $timestamp . "\n" . $secret, $secret, true));
}

function verifySignature(array|\stdClass $request, string $secret, array|\stdClass $options = []): bool
{
    $request = objectFields($request);
    $options = objectFields($options);
    $timestamp = $request['timestamp'] ?? null;
    $signature = $request['signature'] ?? null;
    if (!is_string($timestamp) || !is_string($signature) || !preg_match('/\A[0-9]+\z/', $timestamp)) {
        return false;
    }
    $now = $options['now'] ?? microtime(true) * 1000;
    $tolerance = $options['toleranceMs'] ?? DEFAULT_SIGNATURE_TOLERANCE_MS;
    if (abs($now - (float)$timestamp) > $tolerance) {
        return false;
    }
    return hash_equals(sign($timestamp, $secret), $signature);
}
