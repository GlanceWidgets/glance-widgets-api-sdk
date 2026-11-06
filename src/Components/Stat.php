<?php
declare(strict_types=1);
namespace GlanceWidgets;

const STAT_DEFAULTS = ['id' => 'stat', 'deltaDecimals' => 1, 'arrow' => 'icon', 'valueStyle' => 'text',
    'align' => 'left', 'fontSizeRatio' => .08, 'pixelFont' => '5x7', 'pixelUnitFont' => '3x5', 'zIndex' => 0,
    'labelOpacity' => .72, 'valueColor' => '#FFFFFF', 'labelColor' => '#FFFFFF',
    'upIcon' => 'ph_arrowUp', 'downIcon' => 'ph_arrowDown', 'flatIcon' => 'ph_minus'];

function statArrowMatrix(string $direction, string $color, array $options): Matrix
{
    $rows = ['..#..', '.###.', '#####', '..#..', '..#..'];
    if ($direction === 'down') {
        $rows = array_reverse($rows);
    } elseif ($direction === 'flat') {
        $rows = ['.....', '.....', '#####', '.....', '.....'];
    }
    $matrix = contentMatrix(5, 5, resolveTheme($options['theme'] ?? null));
    foreach ($rows as $y => $row) {
        foreach (str_split($row) as $x => $character) {
            if ($character === '#') {
                $matrix->set($x, $y, $color);
            }
        }
    }
    return $matrix;
}

function statPixelValue(string $value, array $rect, string $color, array $theme,
    array $canvas, array $options): array
{
    $font = $options['pixelFont'] ?? '5x7';
    $main = measureText($value, $font);
    $unitFont = $options['pixelUnitFont'] ?? '3x5';
    $small = isset($options['unit']) ? measureText($options['unit'], $unitFont) : null;
    $content = contentMatrix($main['width'] + ($small === null ? 0 : 1 + $small['width']),
        max($main['height'], $small['height'] ?? 0), $theme);
    $content->text($value, 0, 0, ['font' => $font, 'style' => $color]);
    if ($small !== null) {
        $colors = objectFields($options['colors'] ?? []);
        $content->text($options['unit'], $main['width'] + 1, max(0, $main['height'] - $small['height']),
            ['font' => $unitFont, 'style' => $colors['value'] ?? $theme['palette']['dim']]);
    }
    $region = emittedRegion(Box::fromPt($rect, $canvas), $canvas);
    $fit = array_intersect_key($options, array_flip(['maxCellSize']));
    $fit += ['theme' => $theme, 'align' => ($options['align'] ?? 'left') === 'center' ? 'center' : 'left',
        'valign' => 'center'];
    return ['matrix' => fitPixelContent($content, $region['rect'], $fit), 'frame' => $region['frame']];
}

function stat(string|float|int $value, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'stat');
    [$id, $path] = resolveIdentity($options, 'stat', 'stat');
    if (!is_string($value)) {
        finiteNumber($value, 'Stat value');
    }
    $valueText = is_string($value) ? $value : Numbers::label($value, $options['decimals'] ?? defaultDecimals($value));
    $theme = resolveTheme($options['theme'] ?? null);
    $pixel = ($options['valueStyle'] ?? 'text') === 'pixel';
    $center = ($options['align'] ?? 'left') === 'center';
    $z = $options['zIndex'] ?? 0;
    $colors = objectFields($options['colors'] ?? []);
    $valueColor = $colors['value'] ?? ($pixel ? $theme['palette']['on'] : '#FFFFFF');
    $delta = $options['delta'] ?? null;
    $direction = $delta === null ? null : ($delta > 0 ? 'up' : ($delta < 0 ? 'down' : 'flat'));
    $deltaContent = $options['deltaText'] ?? ($delta === null ? null : ($delta > 0 ? '+' : '')
        . Numbers::label($delta, $options['deltaDecimals'] ?? 1) . ($options['deltaSuffix'] ?? ''));
    $arrowKind = $options['arrow'] ?? 'icon';
    $present = isset($options['label']) ? ['label', 'value'] : ['value'];
    if ($deltaContent !== null) {
        $present[] = 'delta';
    }
    $weights = ['label' => 2, 'value' => 5, 'delta' => 2];
    $boxes = resolveArea($options['frame'] ?? null, $canvas)->rows(array_map(
        static fn($row) => $weights[$row], $present));
    $rowBox = static fn($row) => $boxes[array_search($row, $present, true)]->toPt();
    $layers = [];
    $horizontal = $center ? 'center' : 'left';
    $textLayer = static function ($role, $rect, $content, $alignment, $points, $color, $opacity, $weight)
        use ($id, $canvas, $z): array {
        return labelLayer(['id' => $id . ':' . $role, 'box' => Box::fromPt($rect, $canvas), 'content' => $content,
            'horizontal' => $alignment, 'vertical' => 'center', 'fontSizeRatio' => fontRatioOf($points, $canvas),
            'color' => $color, 'opacity' => $opacity, 'zIndex' => $z, 'fontWeight' => $weight]);
    };
    $usedPt = static fn($points) => fontPtOf(fontRatioOf($points, $canvas), $canvas);
    $fitFont = static fn($height, $width, $content) => $usedPt(min($height / (1.2 * 1.5),
        fontPtForWidth($content, $width), fontPtOf($options['fontSizeRatio'] ?? .08, $canvas)));
    if (isset($options['label'])) {
        $rect = $rowBox('label');
        $points = $fitFont($rect['height'], $rect['width'], $options['label']);
        $layers[] = $textLayer('label', $rect, $options['label'], $horizontal, $points,
            $colors['label'] ?? '#FFFFFF', .72, '400');
    }
    $valueRect = $rowBox('value');
    if ($pixel) {
        $fitted = statPixelValue($valueText, $valueRect, $valueColor, $theme, $canvas, $options);
        $placed = place($fitted['matrix']->toPart(), ['id' => $id . ':value', 'frame' => $fitted['frame'],
            'path' => $path, 'zIndex' => $z]);
        $layers[] = $placed;
    } else {
        $unit = $options['unit'] ?? null;
        $valueEm = textAdvanceEm($valueText);
        $widthEm = $unit === null ? $valueEm * 1.05
            : max($valueEm * 1.05, $valueEm + .15 + textAdvanceEm($unit) * 1.05 * .45);
        $fitWidth = max(0, $valueRect['width'] - 1) / max(1e-9, $widthEm);
        $points = $usedPt(min($valueRect['height'] / 1.32, $fitWidth));
        $valueWidth = textAdvanceEm($valueText) * $points;
        $unitFont = $usedPt($points * .45);
        $unitGap = $points * .15;
        $unitWidth = $unit === null ? 0 : textAdvanceEm($unit) * $unitFont;
        $total = $valueWidth + ($unit === null ? 0 : $unitGap + $unitWidth);
        $start = $center ? $valueRect['x'] + ($valueRect['width'] - $total) / 2 : $valueRect['x'];
        $boxHeight = $points * 1.32;
        $top = $valueRect['y'] + ($valueRect['height'] - $boxHeight) / 2;
        $layers[] = $textLayer('value', ['x' => $start, 'y' => $top,
            'width' => estimateTextWidthPt($valueText, $points), 'height' => $boxHeight],
            $valueText, 'left', $points, $valueColor, 1, '700');
        if ($unit !== null) {
            $height = $unitFont * 1.32;
            $rect = ['x' => $start + $valueWidth + $unitGap, 'y' => $top + $boxHeight - $height,
                'width' => estimateTextWidthPt($unit, $unitFont), 'height' => $height];
            $layers[] = $textLayer('unit', $rect, $unit, 'left', $unitFont, $valueColor, .72, '400');
        }
    }
    if ($deltaContent !== null) {
        $rect = $rowBox('delta');
        $good = $direction === null || $direction === 'flat' ? null
            : (($direction === 'up') !== ($options['invertDelta'] ?? false));
        $status = $good === null ? 'neutral' : ($good ? 'good' : 'bad');
        $color = $colors[$status] ?? STATUS_COLORS[$status];
        $showArrow = $arrowKind !== 'none' && $direction !== null;
        $points = $fitFont($rect['height'], $rect['width'] - ($showArrow ? 1.2 * $rect['height'] : 0), $deltaContent);
        $arrowPt = $showArrow ? $points * 1.2 : 0;
        $gap = $showArrow ? $points * .4 : 0;
        $total = $arrowPt + $gap + textAdvanceEm($deltaContent) * $points;
        $start = $center ? $rect['x'] + ($rect['width'] - $total) / 2 : $rect['x'];
        if ($showArrow) {
            $square = ['x' => $start, 'y' => $rect['y'] + ($rect['height'] - $arrowPt) / 2,
                'width' => $arrowPt, 'height' => $arrowPt];
            if ($arrowKind === 'icon') {
                $name = $direction === 'up' ? 'ph_arrowUp' : ($direction === 'down' ? 'ph_arrowDown' : 'ph_minus');
                $layers[] = icon(['id' => $id . ':arrow', 'name' => $name, 'frame' => Box::fromPt($square, $canvas),
                    'zIndex' => $z, 'style' => ['color' => $color]]);
            } else {
                $region = emittedRegion(Box::fromPt($square, $canvas), $canvas);
                $placed = place(statArrowMatrix($direction, $color, $options)->toPart(),
                    ['id' => $id . ':arrow', 'frame' => $region['frame'], 'path' => $path . 'Arrow', 'zIndex' => $z]);
                $layers[] = $placed;
            }
        }
        $textRect = ['x' => $start + $arrowPt + $gap, 'y' => $rect['y'],
            'width' => estimateTextWidthPt($deltaContent, $points), 'height' => $rect['height']];
        $layers[] = $textLayer('delta', $textRect, $deltaContent, 'left', $points, $color, 1, '600');
    }
    return \GlanceWidgets\Internal\combine($layers);
}
