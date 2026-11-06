<?php
declare(strict_types=1);
namespace GlanceWidgets;

function parseColor(string $color): array
{
    $whitespace = '[\x{0009}-\x{000D}\x{0020}\x{00A0}\x{1680}\x{2000}-\x{200A}'
        . '\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}]';
    $color = preg_replace('/\A' . $whitespace . '+|' . $whitespace . '+\z/u', '', $color);
    if ($color === null || !preg_match('/\A#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})\z/', $color, $match)) {
        throw new \InvalidArgumentException('Invalid color: expected #RGB, #RRGGBB or #AARRGGBB');
    }
    $hex = $match[1];
    if (strlen($hex) === 3) {
        $hex = $hex[0] . $hex[0] . $hex[1] . $hex[1] . $hex[2] . $hex[2];
    }
    if (strlen($hex) === 6) {
        $hex = 'FF' . $hex;
    }
    return array_combine(['a', 'r', 'g', 'b'], array_map('hexdec', str_split($hex, 2)));
}

function formatColor(array|\stdClass $color): string
{
    $color = objectFields($color);
    $channels = [];
    foreach (['r', 'g', 'b', 'a'] as $key) {
        $channels[$key] = clamp(jsRound(finiteNumber($color[$key], 'Color channel')), 0, 255);
    }
    return '#' . ($channels['a'] === 255 ? '' : sprintf('%02X', $channels['a']))
        . sprintf('%02X%02X%02X', $channels['r'], $channels['g'], $channels['b']);
}

function normalizeColor(string $color): string
{
    return formatColor(parseColor($color));
}

function isAppColor(mixed $color): bool
{
    return is_string($color) && preg_match('/\A#?(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})\z/', $color) === 1;
}

function rgb(float $red, float $green, float $blue, float $alpha = 1): string
{
    return formatColor(['r' => $red, 'g' => $green, 'b' => $blue, 'a' => $alpha * 255]);
}

function withAlpha(string $color, float $alpha): string
{
    return formatColor(array_replace(parseColor($color), ['a' => $alpha * 255]));
}

function mixColors(string $start, string $end, float $fraction): string
{
    $left = parseColor($start);
    $right = parseColor($end);
    $fraction = clamp($fraction, 0, 1);
    foreach ($left as $key => $value) {
        $left[$key] = $value + ($right[$key] - $value) * $fraction;
    }
    return formatColor($left);
}
