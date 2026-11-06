<?php
declare(strict_types=1);
namespace GlanceWidgets;

const STATUS_STRIP_DEFAULTS = ['cellWidth' => 1, 'cellGap' => 1, 'rows' => 5,
    'colors' => ['ok' => '#34C759', 'warn' => '#FF9F0A', 'down' => '#FF453A'], 'unknownState' => 'unknown',
    'names' => ['ok' => 'OK', 'warn' => 'Warn', 'down' => 'Down', 'unknown' => 'No data']];
const LABELED_STATUS_STRIP_DEFAULTS = ['id' => 'statusStrip', 'fontSizeRatio' => .05,
    'labelOpacity' => .72, 'zIndex' => 0];

function stateStyle(?string $state, array $theme, array $colors): mixed
{
    $state ??= 'unknown';
    if (array_key_exists($state, $colors)) {
        return $colors[$state];
    }
    return $state === 'unknown' ? $theme['default']
        : throw new \InvalidArgumentException('No color for state ' . $state);
}

function statusMatrix(array $states, array $options): Matrix
{
    if (!$states) {
        throw new \InvalidArgumentException('Status strip needs at least one state');
    }
    $width = positiveInteger($options['cellWidth'] ?? 1, 'cellWidth');
    $gap = integerValue($options['cellGap'] ?? 1, 'cellGap');
    if ($gap < 0) {
        throw new \InvalidArgumentException('cellGap must not be negative');
    }
    if (isset($options['rows'])) {
        positiveInteger($options['rows'], 'rows');
    }
    $theme = resolveTheme($options['theme'] ?? null);
    $colors = array_replace(STATUS_STRIP_DEFAULTS['colors'], objectFields($options['colors'] ?? []));
    $region = regionOf($options, 'statusStrip');
    $columns = count($states) * $width + (count($states) - 1) * $gap;
    $plan = planGrid($region, ['columns' => $columns, 'rows' => $options['rows'] ?? ($region === null ? 5 : null)],
        $theme, 'statusStrip');
    $matrix = new Matrix($plan['columns'], $plan['rows'],
        themeOptions($theme, ['default' => EMPTY_PIXEL, 'layout' => $plan['layout']]));
    foreach ($states as $index => $state) {
        $matrix->rect($index * ($width + $gap), 0, $width, $plan['rows'], stateStyle($state, $theme, $colors));
    }
    return $matrix;
}

function statusStrip(array $states, array|\stdClass $options = []): array
{
    return statusMatrix($states, objectFields($options))->toPart();
}

function labeledStatusStrip(array $states, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'labeledStatusStrip');
    [$id, $path] = resolveIdentity($options, 'statusStrip', 'labeledStatusStrip');
    $theme = resolveTheme($options['theme'] ?? null);
    $colors = objectFields($options['colors'] ?? []);
    $labelColor = $colors['label'] ?? $theme['palette']['on'];
    unset($colors['label']);
    $colors = array_replace(STATUS_STRIP_DEFAULTS['colors'], $colors);
    $names = array_replace(STATUS_STRIP_DEFAULTS['names'], objectFields($options['names'] ?? []));
    $z = $options['zIndex'] ?? 0;
    $ratio = $options['fontSizeRatio'] ?? .05;
    $legend = $options['legend'] ?? false;
    $listed = $legend === false ? [] : (is_array($legend) ? $legend
        : array_values(array_unique(array_map(static fn($state) => $state ?? 'unknown', $states))));
    $points = fontPtOf($ratio, $canvas);
    $rowPt = textBoxHeightPt($points);
    $gapPt = $points * .4;
    $hasLabels = array_key_exists('startLabel', $options) || array_key_exists('endLabel', $options);
    $reserved = ($hasLabels ? $gapPt + $rowPt : 0) + ($listed ? $gapPt + $rowPt : 0);
    $area = resolveArea($options['frame'] ?? null, $canvas);
    $rect = $area->toPt();
    if ($reserved >= $rect['height']) {
        throw new \InvalidArgumentException('Area too small for labels');
    }
    $stripBox = $area->insetPt(['bottom' => $reserved]);
    $matrix = statusMatrix($states, array_replace($options, ['frame' => $stripBox, 'canvas' => $canvas]));
    $placed = place($matrix->toPart(), ['id' => $id . ':strip', 'path' => $path, 'zIndex' => $z,
        'frame' => emittedRegion($stripBox, $canvas)['frame']]);
    $layers = [$placed];
    $add = static function ($role, $left, $top, $width, $content, $horizontal, $color = null)
        use (&$layers, $id, $canvas, $rowPt, $ratio, $labelColor, $z): void {
        $box = Box::fromPt(['x' => $left, 'y' => $top, 'width' => $width, 'height' => $rowPt], $canvas);
        $layers[] = labelLayer(['id' => $id . ':' . $role, 'box' => $box, 'content' => $content,
            'horizontal' => $horizontal, 'vertical' => 'center', 'fontSizeRatio' => $ratio,
            'color' => $color ?? $labelColor, 'opacity' => .72, 'zIndex' => $z + 1]);
    };
    $top = $rect['y'] + $rect['height'] - $reserved;
    if ($hasLabels) {
        $top += $gapPt;
        $half = $rect['width'] / 2;
        if (array_key_exists('startLabel', $options)) {
            $add('start', $rect['x'], $top, $half, $options['startLabel'], 'left');
        }
        if (array_key_exists('endLabel', $options)) {
            $add('end', $rect['x'] + $half, $top, $half, $options['endLabel'], 'right');
        }
        $top += $rowPt;
    }
    if ($listed) {
        $top += $gapPt;
        $slot = $rect['width'] / count($listed);
        $dotWidth = estimateTextWidthPt('●', $points);
        $nameOffset = $dotWidth + $points * .25;
        foreach ($listed as $index => $state) {
            $left = $rect['x'] + $index * $slot;
            $style = stateStyle($state, $theme, $colors);
            $swatch = is_string($style) ? $style : $theme['default']['color'];
            $add('legend-dot-' . $index, $left, $top, $dotWidth, '●', 'left', $swatch);
            $add('legend-name-' . $index, $left + $nameOffset, $top, max(0, $slot - $nameOffset),
                $names[$state] ?? $state, 'left');
        }
    }
    return \GlanceWidgets\Internal\combine($layers);
}
