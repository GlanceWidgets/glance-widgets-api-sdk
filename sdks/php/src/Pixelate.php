<?php
declare(strict_types=1);
namespace GlanceWidgets;

const DEFAULT_ALPHA_THRESHOLD = 128;

function pixelate(array|\stdClass $image, array|\stdClass $options): Matrix
{
    $image = objectFields($image);
    $options = objectFields($options);
    $width = positiveInteger($image['width'], 'Image width');
    $height = positiveInteger($image['height'], 'Image height');
    $data = $image['data'];
    if (count($data) !== $width * $height * 4) {
        throw new \InvalidArgumentException('Invalid RGBA data length');
    }
    if (($options['dither'] ?? 'none') !== 'none') {
        throw new \InvalidArgumentException('Unsupported dither');
    }
    $palette = [];
    foreach ($options['palette'] ?? [] as $color) {
        $palette[] = [normalizeColor($color), parseColor($color)];
    }
    if (array_key_exists('palette', $options) && !$palette) {
        throw new \InvalidArgumentException('Palette must not be empty');
    }
    $matrix = new Matrix($options['columns'], $options['rows'], $options['matrix'] ?? []);
    $divide = static fn($numerator, $denominator) => intdiv(2 * $numerator + $denominator, 2 * $denominator);
    for ($row = 0; $row < $matrix->rows; $row++) {
        $top = intdiv($row * $height, $matrix->rows);
        $bottom = max($top + 1, intdiv(($row + 1) * $height, $matrix->rows));
        for ($column = 0; $column < $matrix->columns; $column++) {
            $left = intdiv($column * $width, $matrix->columns);
            $right = max($left + 1, intdiv(($column + 1) * $width, $matrix->columns));
            $sums = [0, 0, 0];
            $alpha = 0;
            for ($y = $top; $y < $bottom; $y++) {
                for ($x = $left; $x < $right; $x++) {
                    $index = ($y * $width + $x) * 4;
                    $opacity = $data[$index + 3];
                    $alpha += $opacity;
                    for ($channel = 0; $channel < 3; $channel++) {
                        $sums[$channel] += $data[$index + $channel] * $opacity;
                    }
                }
            }
            if ($divide($alpha, ($bottom - $top) * ($right - $left)) < ($options['alphaThreshold'] ?? 128)) {
                continue;
            }
            $channels = $alpha ? array_map(static fn($sum) => $divide($sum, $alpha), $sums) : [0, 0, 0];
            $average = array_combine(['r', 'g', 'b'], $channels) + ['a' => 255];
            $color = formatColor($average);
            $bestDistance = INF;
            foreach ($palette as [$candidate, $rgba]) {
                $distance = 0;
                foreach (['r', 'g', 'b'] as $channel) {
                    $distance += ($rgba[$channel] - $average[$channel]) ** 2;
                }
                if ($distance < $bestDistance) {
                    $color = $candidate;
                    $bestDistance = $distance;
                }
            }
            $matrix->set($column, $row, $color);
        }
    }
    return $matrix;
}
