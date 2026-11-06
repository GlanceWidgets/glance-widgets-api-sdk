<?php
declare(strict_types=1);
namespace GlanceWidgets;

function layoutText(string $text, array|\stdClass $options = []): array
{
    $options = objectFields($options);
    $font = resolveFont($options['font'] ?? null);
    $fixed = ($options['monospace'] ?? false) ? $font->monospaceAdvance() : null;
    $spacing = integerValue($options['letterSpacing'] ?? $font->spacing, 'Letter spacing');
    $lineSpacing = integerValue($options['lineSpacing'] ?? 1, 'Line spacing');
    $maxWidth = isset($options['maxWidth']) ? positiveInteger($options['maxWidth'], 'maxWidth') : null;
    $boxWidth = integerValue($options['boxWidth'] ?? $maxWidth ?? 1, 'boxWidth');
    $align = $options['align'] ?? 'left';
    if (!in_array($align, ['left', 'center', 'right'], true)) {
        throw new \InvalidArgumentException('Invalid text alignment');
    }
    $widthOf = static function (array $characters) use ($font, $spacing, $fixed): int {
        $width = $characters ? $spacing * (count($characters) - 1) : 0;
        foreach ($characters as $character) {
            $width += $fixed ?? $font->glyphFor($character)['advance'];
        }
        return $width;
    };
    $lines = [];
    foreach (explode("\n", $text) as $paragraph) {
        if ($maxWidth === null) {
            $lines[] = codepoints($paragraph);
            continue;
        }
        $current = null;
        foreach (explode(' ', $paragraph) as $word) {
            $rest = codepoints($word);
            $candidate = $current === null ? $rest : array_merge($current, [' '], $rest);
            if ($widthOf($candidate) <= $maxWidth) {
                $current = $candidate;
                continue;
            }
            if ($current !== null) {
                $lines[] = $current;
            }
            while (count($rest) > 1 && $widthOf($rest) > $maxWidth) {
                $length = 1;
                while ($length < count($rest) && $widthOf(array_slice($rest, 0, $length + 1)) <= $maxWidth) {
                    $length++;
                }
                $lines[] = array_slice($rest, 0, $length);
                $rest = array_slice($rest, $length);
            }
            $current = $rest;
        }
        $lines[] = $current ?? [];
    }
    $glyphs = [];
    $width = 0;
    $pitch = $font->lineHeight + $lineSpacing;
    foreach ($lines as $index => $characters) {
        $lineWidth = $widthOf($characters);
        $width = max($width, $lineWidth);
        $pen = match ($align) {
            'left' => 0, 'right' => $boxWidth - $lineWidth, default => (int)floor(($boxWidth - $lineWidth) / 2)
        };
        foreach ($characters as $character) {
            $glyph = $font->glyphFor($character);
            $glyphs[] = ['glyph' => $glyph, 'x' => $pen + ($fixed !== null ? intdiv($fixed - $glyph['width'], 2) : 0), 'y' => $index * $pitch];
            $pen += ($fixed ?? $glyph['advance']) + $spacing;
        }
    }
    $lastHeight = $font->height;
    foreach ($lines[count($lines) - 1] as $character) {
        $lastHeight = max($lastHeight, $font->glyphFor($character)['inkBottom']);
    }
    $height = (count($lines) - 1) * $pitch + $lastHeight;
    $lines = array_map(static fn($characters) => implode('', $characters), $lines);
    return compact('width', 'height', 'lines', 'glyphs');
}

function measureText(string $text, string|array|\stdClass|PixelFont|null $font = null,
    array|\stdClass $options = []): array
{
    $layout = layoutText($text, array_replace(objectFields($options), ['font' => $font]));
    return array_intersect_key($layout, array_flip(['width', 'height', 'lines']));
}

function textMatrix(string $text, array|\stdClass $options = []): Matrix
{
    $options = objectFields($options);
    $metrics = measureText($text, $options['font'] ?? null, $options);
    $matrixOptions = array_intersect_key($options, array_flip(['default', 'base', 'layout']));
    $matrix = new Matrix(max(1, $metrics['width']), max(1, $metrics['height']), $matrixOptions);
    return $matrix->text($text, 0, 0, array_replace($options, ['boxWidth' => $metrics['width']]));
}
