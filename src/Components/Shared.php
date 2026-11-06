<?php
declare(strict_types=1);
namespace GlanceWidgets;

const TEXT_LINE_HEIGHT = 1.2;
const LABEL_BOX_LINES = 1.5;
const LABEL_GAP_EM = .4;

/** Layout and typography shared by semantic components. */
function resolveTheme(string|array|\stdClass|null $theme = null): array
{
    return is_string($theme) || $theme === null ? getTheme($theme ?? DEFAULT_THEME_NAME) : objectFields($theme);
}

function resolveArea(Box|array|\stdClass|null $frame, array $canvas): Box
{
    if ($frame === null) {
        return Box::canvas($canvas);
    }
    return $frame instanceof Box ? new Box($frame->left, $frame->top, $frame->width, $frame->height, $canvas)
        : Box::fromFrame($frame, $canvas);
}

function emittedRegion(Box $box, array $canvas): array
{
    $frame = $box->toFrame();
    return ['frame' => $frame, 'rect' => resolveFrame($frame, $canvas)];
}

function regionOf(array $options, string $operation): ?array
{
    $frame = $options['frame'] ?? null;
    $canvas = $options['canvas'] ?? null;
    if ($canvas === null) {
        if ($frame instanceof Box && $frame->canvasSize !== null) {
            return $frame->toPt();
        }
        if ($frame !== null) {
            throw new \InvalidArgumentException($operation . ': frame needs a canvas');
        }
        return null;
    }
    $canvas = requireCanvas($canvas, $operation);
    return emittedRegion(resolveArea($frame, $canvas), $canvas)['rect'];
}

function planGrid(?array $region, array $size, array $theme, string $operation): array
{
    if ($region === null) {
        if (!isset($size['columns']) || !isset($size['rows'])) {
            throw new \InvalidArgumentException($operation . ': both grid counts required without canvas');
        }
        return $size + ['layout' => $theme['layout']];
    }
    return solveGrid(array_filter($size, static fn($value) => $value !== null)
        + ['region' => $region, 'gapMode' => $theme['layout']['gapMode'], 'clampAspect' => true]);
}

function pixelLooks(array $theme, array|\stdClass $colors = []): array
{
    $colors = objectFields($colors);
    $off = ($colors['off'] ?? '') !== '' ? array_replace($theme['default'], ['color' => $colors['off']]) : $theme['default'];
    if (array_key_exists('off', $colors) && $colors['off'] === null) {
        $off = EMPTY_PIXEL;
    }
    return ['on' => $colors['on'] ?? $theme['palette']['on'], 'off' => $off];
}

function textAdvanceEm(string $content): float
{
    static $advances;
    $advances ??= dataFile('text-advances.json');
    $total = 0;
    foreach (codepoints($content) as $character) {
        $byte = ord($character[0]);
        $advance = strlen($character) === 1 && $byte >= 32 && $byte < 127
            ? $advances['ascii'][$byte - 32] : ($advances['extra'][$character] ?? 1000);
        $total += $advance + 40;
    }
    return $total / 1000;
}

function estimateTextWidthPt(string $content, float $fontPt): float
{
    return textAdvanceEm($content) * 1.05 * $fontPt + 1;
}

function fontPtForWidth(string $content, float $width): float
{
    return max(0, $width - 1) / max(1e-9, textAdvanceEm($content) * 1.05);
}

function textBoxHeightPt(float $fontPt): float
{
    return 1.2 * $fontPt + 1;
}

function fontPtOf(float $ratio, array $canvas): float
{
    return $ratio * min($canvas['width'], $canvas['height']);
}

function fontRatioOf(float $points, array $canvas): float
{
    return clamp($points / min($canvas['width'], $canvas['height']), .02, .28);
}

function resolveIdentity(array $options, string $default, string $operation): array
{
    $id = $options['id'] ?? $default;
    if (($options['path'] ?? null) === null && !preg_match('/\A[A-Za-z_][A-Za-z0-9_]*\z/', $id)) {
        throw new \InvalidArgumentException($operation . ': non-identifier id needs an explicit path');
    }
    return [$id, $options['path'] ?? 'data.' . $id];
}

function defaultDecimals(float|int ...$values): int
{
    foreach ($values as $value) {
        if (floor($value) != $value) {
            return 1;
        }
    }
    return 0;
}

function fractionOf(float|int $value, float|int $minimum, float|int $maximum): float
{
    finiteNumber($value);
    return $maximum > $minimum ? clamp(($value - $minimum) / ($maximum - $minimum), 0, 1) : 0;
}

function requireRange(string $operation, float|int $minimum, float|int $maximum): void
{
    finiteNumber($minimum, 'Minimum');
    finiteNumber($maximum, 'Maximum');
    if ($maximum <= $minimum) {
        throw new \InvalidArgumentException($operation . ': max must be greater than min');
    }
}

function labelLayer(array $specification): array
{
    $style = ['color' => $specification['color'], 'textOpacity' => $specification['opacity'],
        'fontSizeRatio' => $specification['fontSizeRatio'], 'horizontalAlignment' => $specification['horizontal'],
        'verticalAlignment' => $specification['vertical']];
    if (isset($specification['fontWeight'])) {
        $style['fontWeight'] = $specification['fontWeight'];
    }
    return text(['id' => $specification['id'], 'frame' => $specification['box'],
        'text' => $specification['content'], 'zIndex' => $specification['zIndex'], 'style' => $style]);
}

function percentLabel(float $fraction): string
{
    return Numbers::text(jsRound($fraction * 100)) . '%';
}

namespace GlanceWidgets\Internal;

/** Assemble component fragments using the standard response merge rules. */
function combine(array $parts): array
{
    $merged = \GlanceWidgets\merge(...$parts);
    return ['layers' => $merged['layers'] ?? [], 'data' => $merged['data'] ?? new \stdClass()];
}
