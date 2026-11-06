<?php
declare(strict_types=1);
namespace GlanceWidgets;

/**
 * Modern (layer-built, not matrix) components: progressBar, ringProgress, barChart, battery, signal.
 * Thin pieces are drawn with `line` (stroke 0.5-8 pt) and `curve` (0.25-12 pt) layers with round caps.
 */
const MODERN_COLORS = ['fill' => '#0A84FF', 'track' => '#8E8E93', 'label' => '#FFFFFF'];
const TRACK_OPACITY = .3;
const LINE_STROKE_MAX_PT = 8;
const CURVE_STROKE_MAX_PT = 12;
const PROGRESS_BAR_DEFAULTS = ['id' => 'progress', 'min' => 0, 'max' => 100, 'orientation' => 'horizontal',
    'thickness' => 6, 'labelPosition' => 'end', 'fontSizeRatio' => .07, 'labelOpacity' => 1, 'zIndex' => 0];
const RING_PROGRESS_DEFAULTS = ['id' => 'ring', 'min' => 0, 'max' => 100, 'startAngle' => 0,
    'thicknessRatio' => .1, 'thicknessMin' => 3, 'labelOpacity' => 1, 'zIndex' => 0];
const BAR_CHART_DEFAULTS = ['id' => 'barChart', 'fontSizeRatio' => .06, 'barShareOfSlot' => .5,
    'baselineWidth' => 1, 'baselineOpacity' => .3, 'labelOpacity' => .72, 'zIndex' => 0,
    'colors' => ['fill' => '#0A84FF', 'track' => '#8E8E93', 'label' => '#FFFFFF', 'positive' => '#0A84FF',
        'negative' => '#FF453A', 'baseline' => '#FFFFFF']];
const BATTERY_DEFAULTS = ['id' => 'battery', 'min' => 0, 'max' => 100, 'height' => 12, 'widthRatio' => 2.3,
    'fontSizeRatio' => .07, 'zIndex' => 0];
const BAD_LEVEL = .2;
const WARN_LEVEL = .4;
const SIGNAL_DEFAULTS = ['id' => 'signal', 'min' => 0, 'max' => 100, 'bars' => 4, 'maxHeight' => 14,
    'barWidthRatio' => .26, 'gapRatio' => .5, 'shortestRatio' => .35, 'zIndex' => 0];
const LABEL_MAX_SHARE_DIVISOR = 3;
const RING_SAMPLE_STEP_DEGREES = 2;
const RING_X_EXTREME = 90;
const RING_HALF_TURN = 180;
const RING_LABEL_WIDTH_OF_RADIUS = 1.6;
const RING_LABEL_HEIGHT_OF_RADIUS = 1.2;
const BATTERY_HEIGHT_MIN_PT = 5;
const BATTERY_INSET_PT = 2;
const BATTERY_TAB_HEIGHT_RATIO = .5;
const BATTERY_TAB_WIDTH_RATIO = .25;
const BATTERY_TAB_GAP_PT = 1;
const CURVE_POINT_DECIMALS = 1e3;

function requireStroke(string $what, string $name, float|int $value, float|int $max): void
{
    if (!is_finite($value) || $value < .5 || $value > $max) {
        throw new \InvalidArgumentException("$what: $name must be between 0.5 and $max pt, got $value");
    }
}

function minCapsuleLength(array $canvas, string $direction, float|int $width): float|int
{
    return $width + FRAME_SIZE_MIN * ($direction === 'horizontal' ? $canvas['width'] : $canvas['height']);
}

/** A round-capped line whose visible extent is exactly [from, to]. Keys: id canvas direction at from to width color opacity zIndex. */
function capsule(array $spec): array
{
    $canvas = $spec['canvas'];
    $width = $spec['width'];
    $horizontal = $spec['direction'] === 'horizontal';
    $at = $spec['at'];
    $start = $spec['from'] + $width / 2;
    $end = max($spec['to'] - $width / 2, $start + FRAME_SIZE_MIN * ($horizontal ? $canvas['width'] : $canvas['height']));
    $frame = $horizontal
        ? new Box($start / $canvas['width'], $at / $canvas['height'], ($end - $start) / $canvas['width'], 0)
        : new Box($at / $canvas['width'], $start / $canvas['height'], 0, ($end - $start) / $canvas['height']);
    return line(['id' => $spec['id'], 'frame' => $frame, 'direction' => $spec['direction'], 'zIndex' => $spec['zIndex'],
        'style' => ['color' => $spec['color'], 'opacity' => $spec['opacity'], 'strokeWidth' => $width, 'cap' => 'round']]);
}

function fillLength(float|int $fraction, float|int $full, float|int $minimum): float|int
{
    return $fraction <= 0 ? 0 : min($full, max($minimum, $fraction * $full));
}

/** A horizontal round-capped bar up to 12 pt thick: a 2-point curve whose visible extent is exactly [from, to]. Keys: id canvas from to at width color opacity zIndex. */
function curveCapsule(array $spec): array
{
    $canvas = $spec['canvas'];
    $width = $spec['width'];
    $half = $width / 2;
    $round = static fn($value) => jsRound($value * CURVE_POINT_DECIMALS) / CURVE_POINT_DECIMALS;
    $frameWidth = max($spec['to'] - $spec['from'], $width + .05, FRAME_SIZE_MIN * $canvas['width']);
    $frameHeight = max($width, FRAME_SIZE_MIN * $canvas['height']);
    return curve(['id' => $spec['id'], 'frame' => Box::fromPt(['x' => $spec['from'], 'y' => $spec['at'] - $frameHeight / 2,
        'width' => $frameWidth, 'height' => $frameHeight], $canvas), 'zIndex' => $spec['zIndex'],
        'data' => ['mode' => 'samples', 'points' => [[$round($half), .5], [$round($frameWidth - $half), .5]],
            'domain' => [0, $round($frameWidth)], 'yDomain' => [0, 1], 'interpolation' => 'linear'],
        'style' => ['color' => $spec['color'], 'opacity' => $spec['opacity'], 'strokeWidth' => $width, 'lineCap' => 'round']]);
}

function progressBar(float $value, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'progressBar');
    [$id] = resolveIdentity($options, PROGRESS_BAR_DEFAULTS['id'], 'progressBar');
    $min = $options['min'] ?? 0;
    $max = $options['max'] ?? 100;
    requireRange('progressBar', $min, $max);
    $thickness = $options['thickness'] ?? PROGRESS_BAR_DEFAULTS['thickness'];
    requireStroke('progressBar', 'thickness', $thickness, LINE_STROKE_MAX_PT);
    $direction = $options['orientation'] ?? 'horizontal';
    $vertical = $direction === 'vertical';
    $colors = array_replace(MODERN_COLORS, objectFields($options['colors'] ?? []));
    $z = $options['zIndex'] ?? 0;
    $fraction = fractionOf($value, $min, $max);
    $areaPt = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    [$from, $to] = $vertical ? [$areaPt['y'], $areaPt['y'] + $areaPt['height']]
        : [$areaPt['x'], $areaPt['x'] + $areaPt['width']];
    $labelLayers = [];
    $label = $options['label'] ?? false;
    if ($label !== false) {
        $content = is_string($label) ? $label : percentLabel($fraction);
        $atEnd = ($options['labelPosition'] ?? 'end') === 'end';
        $lineBox = TEXT_LINE_HEIGHT * LABEL_BOX_LINES;
        $fitAcross = $vertical ? fontPtForWidth($content, $areaPt['width']) : $areaPt['height'] / $lineBox;
        $fitAlong = $vertical ? $areaPt['height'] / ($lineBox * LABEL_MAX_SHARE_DIVISOR)
            : fontPtForWidth($content, $areaPt['width'] / LABEL_MAX_SHARE_DIVISOR);
        $requested = fontPtOf($options['fontSizeRatio'] ?? .07, $canvas);
        $points = fontPtOf(fontRatioOf(min($requested, $fitAcross, $fitAlong), $canvas), $canvas);
        $slot = $vertical ? $points * $lineBox : estimateTextWidthPt($content, $points);
        $reserved = $slot + $points * LABEL_GAP_EM;
        $rect = $vertical
            ? ['x' => $areaPt['x'], 'y' => $atEnd ? $to - $slot : $from, 'width' => $areaPt['width'], 'height' => $slot]
            : ['x' => $atEnd ? $to - $slot : $from, 'y' => $areaPt['y'], 'width' => $slot, 'height' => $areaPt['height']];
        if ($atEnd) {
            $to -= $reserved;
        } else {
            $from += $reserved;
        }
        $labelLayers[] = labelLayer(['id' => $id . ':label', 'box' => Box::fromPt($rect, $canvas),
            'content' => $content, 'horizontal' => $vertical ? 'center' : ($atEnd ? 'right' : 'left'),
            'vertical' => 'center', 'fontSizeRatio' => fontRatioOf($points, $canvas), 'color' => $colors['label'],
            'opacity' => 1, 'zIndex' => $z + 1]);
    }
    $at = $vertical ? $areaPt['x'] + $areaPt['width'] / 2 : $areaPt['y'] + $areaPt['height'] / 2;
    $minimum = minCapsuleLength($canvas, $direction, $thickness);
    if (!($to - $from >= $minimum)) {
        throw new \InvalidArgumentException("progressBar: the area is too short for a $thickness pt bar");
    }
    $spec = ['canvas' => $canvas, 'direction' => $direction, 'at' => $at, 'width' => $thickness, 'zIndex' => $z];
    $layers = [capsule($spec + ['id' => $id . ':track', 'from' => $from, 'to' => $to, 'color' => $colors['track'],
        'opacity' => TRACK_OPACITY])];
    $length = fillLength($fraction, $to - $from, $minimum);
    if ($length > 0) {
        $fromStart = $vertical === ($options['reverse'] ?? false);
        [$start, $end] = $fromStart ? [$from, $from + $length] : [$to - $length, $to];
        $layers[] = capsule(['id' => $id . ':fill', 'from' => $start, 'to' => $end, 'color' => $colors['fill'],
            'opacity' => 1, 'zIndex' => $z + 1] + $spec);
    }
    array_push($layers, ...$labelLayers);
    return \GlanceWidgets\Internal\combine($layers);
}

function ringUnit(float|int $value): float|int
{
    return jsRound(min(1, max(-1, $value)) * 1e4) / 1e4;
}

/** The arc from `from` to `to` (degrees clockwise from 12 o'clock) as x-ascending polylines on the unit circle. */
function arcPieces(float|int $from, float|int $to): array
{
    $bounds = [$from];
    for ($angle = ceil(($from - RING_X_EXTREME) / RING_HALF_TURN) * RING_HALF_TURN + RING_X_EXTREME;
        $angle < $to; $angle += RING_HALF_TURN) {
        if ($angle > $from) {
            $bounds[] = $angle;
        }
    }
    $bounds[] = $to;
    $pieces = [];
    for ($index = 0; $index + 1 < count($bounds); $index++) {
        [$a, $b] = [$bounds[$index], $bounds[$index + 1]];
        if (!($b - $a > 1e-9)) {
            continue;
        }
        $count = max(2, (int)ceil(($b - $a) / RING_SAMPLE_STEP_DEGREES) + 1);
        $points = [];
        for ($step = 0; $step < $count; $step++) {
            $radians = (($a + (($b - $a) * $step) / ($count - 1)) * M_PI) / RING_HALF_TURN;
            $points[] = [ringUnit(sin($radians)), ringUnit(cos($radians))];
        }
        if ($points[0][0] > $points[count($points) - 1][0]) {
            $points = array_reverse($points);
        }
        $pieces[] = $points;
    }
    return $pieces;
}

function ringProgress(float $value, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'ringProgress');
    [$id] = resolveIdentity($options, RING_PROGRESS_DEFAULTS['id'], 'ringProgress');
    $min = $options['min'] ?? 0;
    $max = $options['max'] ?? 100;
    requireRange('ringProgress', $min, $max);
    $start = $options['startAngle'] ?? 0;
    if (!is_finite($start)) {
        throw new \InvalidArgumentException("ringProgress: startAngle must be finite, got $start");
    }
    $colors = array_replace(MODERN_COLORS, objectFields($options['colors'] ?? []));
    $z = $options['zIndex'] ?? 0;
    $fraction = fractionOf($value, $min, $max);
    $areaPt = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    $side = min($areaPt['width'], $areaPt['height']);
    $ringPt = ['x' => $areaPt['x'] + ($areaPt['width'] - $side) / 2, 'y' => $areaPt['y'] + ($areaPt['height'] - $side) / 2,
        'width' => $side, 'height' => $side];
    $ringBox = Box::fromPt($ringPt, $canvas);
    $thickness = $options['thickness'] ?? min(CURVE_STROKE_MAX_PT,
        max(RING_PROGRESS_DEFAULTS['thicknessMin'], jsRound($side * RING_PROGRESS_DEFAULTS['thicknessRatio'])));
    requireStroke('ringProgress', 'thickness', $thickness, CURVE_STROKE_MAX_PT);
    if (!($thickness < $side / 2)) {
        throw new \InvalidArgumentException("ringProgress: a $thickness pt ring does not fit $side pt");
    }
    $pieces = static fn($from, $to, $role, $color, $opacity, $zIndex): array => array_map(
        static fn($points, $index) => curve(['id' => $id . ':' . $role . '-' . $index, 'frame' => $ringBox,
            'zIndex' => $zIndex, 'data' => ['mode' => 'samples', 'points' => $points, 'domain' => [-1, 1],
                'yDomain' => [-1, 1], 'interpolation' => 'linear'],
            'style' => ['color' => $color, 'opacity' => $opacity, 'strokeWidth' => $thickness,
                'lineCap' => 'round', 'plotPadding' => $thickness / 2]]),
        arcPieces($from, $to), array_keys(arcPieces($from, $to)));
    $layers = $pieces(-RING_X_EXTREME, 360 - RING_X_EXTREME, 'track', $colors['track'], TRACK_OPACITY, $z);
    if ($fraction > 0) {
        $sweep = $fraction * 360;
        [$from, $to] = ($options['counterClockwise'] ?? false) ? [$start - $sweep, $start] : [$start, $start + $sweep];
        array_push($layers, ...$pieces($from, $to, 'arc', $colors['fill'], 1, $z + 1));
    }
    $label = $options['label'] ?? true;
    if ($label !== false) {
        $content = is_string($label) ? $label : percentLabel($fraction);
        $holeRadius = max(1, $side / 2 - $thickness);
        $holeWidth = RING_LABEL_WIDTH_OF_RADIUS * $holeRadius;
        $holeHeight = RING_LABEL_HEIGHT_OF_RADIUS * $holeRadius;
        $requested = !isset($options['fontSizeRatio'])
            ? min($holeHeight / (TEXT_LINE_HEIGHT * LABEL_BOX_LINES), fontPtForWidth($content, $holeWidth))
            : fontPtOf($options['fontSizeRatio'], $canvas);
        $points = fontPtOf(fontRatioOf($requested, $canvas), $canvas);
        $boxWidth = max($holeWidth, estimateTextWidthPt($content, $points));
        $boxHeight = max($holeHeight, textBoxHeightPt($points));
        $centerX = $ringPt['x'] + $side / 2;
        $centerY = $ringPt['y'] + $side / 2;
        $layers[] = labelLayer(['id' => $id . ':label', 'box' => Box::fromPt(['x' => $centerX - $boxWidth / 2,
            'y' => $centerY - $boxHeight / 2, 'width' => $boxWidth, 'height' => $boxHeight], $canvas),
            'content' => $content, 'horizontal' => 'center', 'vertical' => 'center',
            'fontSizeRatio' => fontRatioOf($points, $canvas), 'color' => $colors['label'], 'opacity' => 1,
            'zIndex' => $z + 2]);
    }
    return \GlanceWidgets\Internal\combine($layers);
}

function barChart(array $values, array|\stdClass $options): array
{
    $options = objectFields($options);
    if (!$values) {
        throw new \InvalidArgumentException('barChart: needs at least one value');
    }
    foreach ($values as $value) {
        if (!is_finite($value)) {
            throw new \InvalidArgumentException('barChart: values must be finite');
        }
    }
    $canvas = requireCanvas($options['canvas'] ?? null, 'barChart');
    [$id] = resolveIdentity($options, BAR_CHART_DEFAULTS['id'], 'barChart');
    $colors = array_replace(BAR_CHART_DEFAULTS['colors'], objectFields($options['colors'] ?? []));
    $z = $options['zIndex'] ?? 0;
    $categories = $options['categories'] ?? [];
    if (count($categories) > 0 && count($categories) !== count($values)) {
        throw new \InvalidArgumentException('barChart: categories must have one entry per value');
    }
    $low = $options['min'] ?? min(array_merge([0], $values));
    $high = $options['max'] ?? max(array_merge([0], $values));
    if (!isset($options['min']) && !isset($options['max']) && $low == $high) {
        $high = $low + 1;
    }
    if (!($high > $low)) {
        throw new \InvalidArgumentException("barChart: max ($high) must be greater than min ($low)");
    }
    $ratio = $options['fontSizeRatio'] ?? BAR_CHART_DEFAULTS['fontSizeRatio'];
    $fontPt = $ratio * min($canvas['width'], $canvas['height']);
    $areaPt = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    $labelRow = count($categories) > 0 ? $fontPt * LABEL_GAP_EM + textBoxHeightPt($fontPt) : 0;
    $plotHeight = $areaPt['height'] - $labelRow;
    $slot = $areaPt['width'] / count($values);
    $barWidth = $options['barWidth'] ?? min(LINE_STROKE_MAX_PT, $slot * BAR_CHART_DEFAULTS['barShareOfSlot']);
    requireStroke('barChart', 'barWidth', $barWidth, LINE_STROKE_MAX_PT);
    $minimum = minCapsuleLength($canvas, 'vertical', $barWidth);
    if (!($plotHeight > $minimum)) {
        throw new \InvalidArgumentException('barChart: the area is too short');
    }
    $yOf = static fn($value) => $areaPt['y'] + (1 - fractionOf($value, $low, $high)) * $plotHeight;
    $zero = min($high, max($low, 0));
    $baseY = $yOf($zero);
    $layers = [];
    foreach ($values as $index => $value) {
        $clamped = min($high, max($low, $value));
        if ($clamped == $zero) {
            continue;
        }
        $valueY = $yOf($clamped);
        $positive = $clamped > $zero;
        [$from, $to] = $positive ? [min($valueY, $baseY - $minimum), $baseY] : [$baseY, max($valueY, $baseY + $minimum)];
        $layers[] = capsule(['id' => $id . ':bar-' . $index, 'canvas' => $canvas, 'direction' => 'vertical',
            'at' => $areaPt['x'] + ($index + .5) * $slot, 'from' => $from, 'to' => $to, 'width' => $barWidth,
            'color' => $positive ? $colors['positive'] : $colors['negative'], 'opacity' => 1, 'zIndex' => $z]);
    }
    if ($options['showBaseline'] ?? true) {
        $layers[] = horizontalLine(['id' => $id . ':baseline', 'at' => $baseY / $canvas['height'],
            'from' => $areaPt['x'] / $canvas['width'], 'to' => ($areaPt['x'] + $areaPt['width']) / $canvas['width'],
            'zIndex' => $z, 'style' => ['color' => $colors['baseline'],
                'strokeWidth' => BAR_CHART_DEFAULTS['baselineWidth'], 'opacity' => BAR_CHART_DEFAULTS['baselineOpacity']]]);
    }
    foreach ($categories as $index => $category) {
        $layers[] = labelLayer(['id' => $id . ':category-' . $index, 'box' => Box::fromPt(
            ['x' => $areaPt['x'] + $index * $slot, 'y' => $areaPt['y'] + $plotHeight + $fontPt * LABEL_GAP_EM,
                'width' => $slot, 'height' => textBoxHeightPt($fontPt)], $canvas),
            'content' => $category, 'horizontal' => 'center', 'vertical' => 'top', 'fontSizeRatio' => $ratio,
            'color' => $colors['label'], 'opacity' => BAR_CHART_DEFAULTS['labelOpacity'], 'zIndex' => $z]);
    }
    return \GlanceWidgets\Internal\combine($layers);
}

function battery(float $level, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'battery');
    [$id] = resolveIdentity($options, BATTERY_DEFAULTS['id'], 'battery');
    $min = $options['min'] ?? 0;
    $max = $options['max'] ?? 100;
    requireRange('battery', $min, $max);
    $height = $options['height'] ?? BATTERY_DEFAULTS['height'];
    if (!($height >= BATTERY_HEIGHT_MIN_PT && $height <= CURVE_STROKE_MAX_PT)) {
        throw new \InvalidArgumentException('battery: height must be between ' . BATTERY_HEIGHT_MIN_PT . ' and '
            . CURVE_STROKE_MAX_PT . " pt, got $height");
    }
    $colors = objectFields($options['colors'] ?? []);
    $z = $options['zIndex'] ?? 0;
    $fraction = fractionOf($level, $min, $max);
    $track = $colors['track'] ?? MODERN_COLORS['track'];
    $levelColor = $colors['fill'] ?? ($fraction <= BAD_LEVEL ? ($colors['bad'] ?? STATUS_COLORS['bad'])
        : ($fraction <= WARN_LEVEL ? ($colors['warn'] ?? STATUS_COLORS['warn']) : ($colors['good'] ?? STATUS_COLORS['good'])));
    $areaPt = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    if ($areaPt['height'] < $height) {
        throw new \InvalidArgumentException("battery: the area must be at least $height pt high, got " . $areaPt['height']);
    }
    $tabWidth = $height * BATTERY_TAB_WIDTH_RATIO;
    $middle = $areaPt['y'] + $areaPt['height'] / 2;
    $reserved = 0;
    $labelLayers = [];
    $label = $options['label'] ?? false;
    if ($label !== false) {
        $content = is_string($label) ? $label : percentLabel($fraction);
        $requested = fontPtOf($options['fontSizeRatio'] ?? BATTERY_DEFAULTS['fontSizeRatio'], $canvas);
        $points = fontPtOf(fontRatioOf(min($requested, $height / (TEXT_LINE_HEIGHT * LABEL_BOX_LINES)), $canvas), $canvas);
        $slot = estimateTextWidthPt($content, $points);
        $reserved = $slot + $points * LABEL_GAP_EM;
        $labelLayers[] = labelLayer(['id' => $id . ':label', 'box' => Box::fromPt(['x' => $areaPt['x'] + $areaPt['width'] - $slot,
            'y' => $areaPt['y'], 'width' => $slot, 'height' => $areaPt['height']], $canvas),
            'content' => $content, 'horizontal' => 'right', 'vertical' => 'center',
            'fontSizeRatio' => fontRatioOf($points, $canvas), 'color' => $colors['label'] ?? MODERN_COLORS['label'],
            'opacity' => 1, 'zIndex' => $z + 1]);
    }
    $available = $areaPt['width'] - $reserved - BATTERY_TAB_GAP_PT - $tabWidth;
    $bodyWidth = min($available, $options['width'] ?? $height * BATTERY_DEFAULTS['widthRatio']);
    if (!($bodyWidth >= $height + .1)) {
        throw new \InvalidArgumentException("battery: the area is too narrow (body $bodyWidth pt)");
    }
    $left = $areaPt['x'];
    $spec = ['canvas' => $canvas, 'at' => $middle, 'color' => $track, 'opacity' => TRACK_OPACITY, 'zIndex' => $z];
    $layers = [
        curveCapsule(['id' => $id . ':body', 'from' => $left, 'to' => $left + $bodyWidth, 'width' => $height] + $spec),
        capsule(['id' => $id . ':tab', 'direction' => 'vertical', 'at' => $left + $bodyWidth + BATTERY_TAB_GAP_PT + $tabWidth / 2,
            'from' => $middle - ($height * BATTERY_TAB_HEIGHT_RATIO) / 2,
            'to' => $middle + ($height * BATTERY_TAB_HEIGHT_RATIO) / 2, 'width' => $tabWidth] + $spec),
    ];
    $thickness = $height - 2 * BATTERY_INSET_PT;
    $length = fillLength($fraction, $bodyWidth - 2 * BATTERY_INSET_PT, $thickness);
    if ($length > 0) {
        $layers[] = curveCapsule(['canvas' => $canvas, 'id' => $id . ':level', 'from' => $left + BATTERY_INSET_PT,
            'to' => $left + BATTERY_INSET_PT + $length, 'at' => $middle, 'width' => $thickness,
            'color' => $levelColor, 'opacity' => 1, 'zIndex' => $z + 1]);
    }
    array_push($layers, ...$labelLayers);
    return \GlanceWidgets\Internal\combine($layers);
}

function signal(float $level, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'signal');
    [$id] = resolveIdentity($options, SIGNAL_DEFAULTS['id'], 'signal');
    $min = $options['min'] ?? 0;
    $max = $options['max'] ?? 100;
    requireRange('signal', $min, $max);
    $bars = positiveInteger($options['bars'] ?? SIGNAL_DEFAULTS['bars'], 'bars');
    $areaPt = resolveArea($options['frame'] ?? null, $canvas)->toPt();
    $height = $options['height'] ?? min(SIGNAL_DEFAULTS['maxHeight'], $areaPt['height']);
    $width = $options['barWidth'] ?? $height * SIGNAL_DEFAULTS['barWidthRatio'];
    requireStroke('signal', 'barWidth', $width, LINE_STROKE_MAX_PT);
    $gap = $width * SIGNAL_DEFAULTS['gapRatio'];
    $minimum = minCapsuleLength($canvas, 'vertical', $width);
    if (!($height >= $minimum && $height <= $areaPt['height'])) {
        throw new \InvalidArgumentException("signal: height $height pt does not fit the area");
    }
    if ($bars * $width + ($bars - 1) * $gap > $areaPt['width']) {
        throw new \InvalidArgumentException('signal: the area is too narrow');
    }
    $colors = objectFields($options['colors'] ?? []);
    $fill = $colors['fill'] ?? MODERN_COLORS['label'];
    $track = $colors['track'] ?? $fill;
    $lit = jsRound(fractionOf($level, $min, $max) * $bars);
    $bottom = $areaPt['y'] + ($areaPt['height'] + $height) / 2;
    $layers = [];
    for ($index = 0; $index < $bars; $index++) {
        $share = $bars === 1 ? 1 : SIGNAL_DEFAULTS['shortestRatio'] + ((1 - SIGNAL_DEFAULTS['shortestRatio']) * $index) / ($bars - 1);
        $layers[] = capsule(['id' => $id . ':bar-' . $index, 'canvas' => $canvas, 'direction' => 'vertical',
            'at' => $areaPt['x'] + $width / 2 + $index * ($width + $gap),
            'from' => $bottom - max($minimum, $height * $share), 'to' => $bottom,
            'width' => $width, 'color' => $index < $lit ? $fill : $track, 'opacity' => $index < $lit ? 1 : TRACK_OPACITY,
            'zIndex' => $options['zIndex'] ?? 0]);
    }
    return \GlanceWidgets\Internal\combine($layers);
}
