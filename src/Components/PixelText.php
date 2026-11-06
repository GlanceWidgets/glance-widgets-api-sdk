<?php
declare(strict_types=1);
namespace GlanceWidgets;

const PIXEL_TEXT_DEFAULTS = ['font' => '5x7', 'align' => 'center', 'valign' => 'center', 'unlit' => true];

function contentMatrix(int $columns, int $rows, array $theme): Matrix
{
    return new Matrix(max(1, $columns), max(1, $rows), themeOptions($theme, ['default' => EMPTY_PIXEL]));
}

function fitPixelContent(Matrix $content, array $region, array $options): Matrix
{
    $theme = $options['theme'];
    $gap = GAP_MODE_RATIOS[$theme['layout']['gapMode']];
    $cellFor = static fn($extent, $count) => $extent / ($count + ($count - 1) * $gap);
    $cell = min($cellFor($region['width'], $content->columns), $cellFor($region['height'], $content->rows));
    if (isset($options['maxCellSize'])) {
        if ($options['maxCellSize'] <= 0) {
            throw new \InvalidArgumentException('maxCellSize must be positive');
        }
        $cell = min($cell, $options['maxCellSize'] / (1 + $gap));
    }
    $count = static fn($extent) => floatToInt(floor(($extent / $cell + $gap) / (1 + $gap) + 1e-9));
    $columns = max($content->columns, $count($region['width']));
    $rows = max($content->rows, $count($region['height']));
    if ($columns * $rows > MAX_MATRIX_CELLS) {
        throw new \InvalidArgumentException('Pixel content exceeds 4096 cells');
    }
    $solved = solveGrid(['region' => $region, 'columns' => $columns, 'rows' => $rows,
        'gapMode' => $theme['layout']['gapMode']]);
    $matrix = new Matrix($columns, $rows, themeOptions($theme, [
        'default' => ($options['unlit'] ?? false) ? $theme['default'] : EMPTY_PIXEL, 'layout' => $solved['layout']]));
    $offset = static fn($slack, $position, $start, $end) => $position === $start ? 0
        : ($position === $end ? $slack : (int)floor($slack / 2));
    return $matrix->blit($content,
        $offset($columns - $content->columns, $options['align'] ?? 'center', 'left', 'right'),
        $offset($rows - $content->rows, $options['valign'] ?? 'center', 'top', 'bottom'));
}

function pixelTextContent(string $content, array $options): Matrix
{
    if ($content === '') {
        throw new \InvalidArgumentException('Pixel text must not be empty');
    }
    $theme = resolveTheme($options['theme'] ?? null);
    $font = $options['font'] ?? '5x7';
    $spacing = array_intersect_key($options, array_flip(['monospace', 'letterSpacing', 'lineSpacing']));
    $metrics = measureText($content, $font, $spacing);
    $target = contentMatrix($metrics['width'], $metrics['height'], $theme);
    $textOptions = $spacing + ['font' => $font, 'boxWidth' => $metrics['width'],
        'align' => $options['lineAlign'] ?? $options['align'] ?? 'center'];
    if (($options['color'] ?? null) !== null) {
        $textOptions['style'] = $options['color'];
    }
    return $target->text($content, 0, 0, $textOptions);
}

function fittedText(string $content, array $region, array $options): Matrix
{
    $fit = array_intersect_key($options, array_flip(['maxCellSize']));
    $fit += ['theme' => resolveTheme($options['theme'] ?? null), 'align' => $options['align'] ?? 'center',
        'valign' => $options['valign'] ?? 'center', 'unlit' => $options['unlit'] ?? true];
    return fitPixelContent(pixelTextContent($content, $options), $region, $fit);
}

function pixelTextPanel(string $content, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'pixelTextPanel');
    $region = emittedRegion(resolveArea($options['frame'] ?? null, $canvas), $canvas)['rect'];
    return fittedText($content, $region, $options)->toPart();
}

function pixelTextRoot(string $content, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'pixelTextRoot');
    return fittedText($content, ['x' => 0, 'y' => 0, 'width' => $canvas['width'], 'height' => $canvas['height']],
        $options)->toRoot();
}
