<?php
declare(strict_types=1);
namespace GlanceWidgets;

const HEATMAP_DEFAULTS = ['missing' => 'empty', 'calendarLevels' => 5];
const CALENDAR_HEATMAP_DEFAULTS = ['startWeekday' => 0, 'orientation' => 'weekColumns',
    'weekdayLabels' => ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'],
    'monthLabels' => ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']];
const LABELED_CALENDAR_HEATMAP_DEFAULTS = ['id' => 'calendar', 'fontSizeRatio' => .05,
    'labelOpacity' => .72, 'zIndex' => 0];

function heatScale(array $values, array $theme, array $options, ?int $defaultLevels = null): \Closure
{
    $stops = $options['scale'] ?? [$theme['palette']['off'], $theme['palette']['on']];
    if (count($stops) < 2) {
        throw new \InvalidArgumentException('Heat scale needs at least two colors');
    }
    $levels = $options['levels'] ?? $defaultLevels;
    if ($levels !== null && integerValue($levels, 'levels') < 2) {
        throw new \InvalidArgumentException('Levels must be integer of at least two');
    }
    $present = array_values(array_filter($values, static fn($value) => $value !== null));
    foreach ($present as $value) {
        finiteNumber($value, 'Heat value');
    }
    $minimum = $options['min'] ?? ($present ? min($present) : 0);
    $maximum = $options['max'] ?? ($present ? max($present) : 0);
    if ($maximum < $minimum) {
        throw new \InvalidArgumentException('Heat max must not be below min');
    }
    return static function ($value) use ($minimum, $maximum, $levels, $stops): ?string {
        if ($value === null) {
            return null;
        }
        $fraction = $maximum > $minimum ? clamp(($value - $minimum) / ($maximum - $minimum), 0, 1) : 0;
        if ($levels !== null) {
            $fraction = min($levels - 1, floor($fraction * $levels)) / ($levels - 1);
        }
        $position = $fraction * (count($stops) - 1);
        $index = min(count($stops) - 2, (int)floor($position));
        return mixColors($stops[$index], $stops[$index + 1], $position - $index);
    };
}

function paintHeat(Matrix $matrix, int $x, int $y, mixed $value, callable $colorOf, mixed $missing): void
{
    $color = $colorOf($value);
    if ($color !== null) {
        $matrix->set($x, $y, $color);
    } elseif ($missing !== EMPTY_PIXEL) {
        $matrix->set($x, $y, $missing);
    }
}

function heatmap(array $values, array|\stdClass $options = []): array
{
    $options = objectFields($options);
    $rows = count($values);
    $columns = $rows ? count($values[0]) : 0;
    if (!$rows || !$columns) {
        throw new \InvalidArgumentException('Heatmap needs a nonempty rectangular grid');
    }
    $flat = [];
    foreach ($values as $row) {
        if (count($row) !== $columns) {
            throw new \InvalidArgumentException('Heatmap row widths must match');
        }
        $flat = array_merge($flat, $row);
    }
    $theme = resolveTheme($options['theme'] ?? null);
    $colorOf = heatScale($flat, $theme, $options);
    $plan = planGrid(regionOf($options, 'heatmap'), compact('columns', 'rows'), $theme, 'heatmap');
    $matrix = new Matrix($columns, $rows,
        themeOptions($theme, ['default' => EMPTY_PIXEL, 'layout' => $plan['layout']]));
    foreach ($values as $y => $row) {
        foreach ($row as $x => $value) {
            paintHeat($matrix, $x, $y, $value, $colorOf, $options['missing'] ?? EMPTY_PIXEL);
        }
    }
    return $matrix->toPart();
}

function calendarLayout(int $count, array $options): array
{
    if ($count === 0) {
        throw new \InvalidArgumentException('Calendar needs at least one day');
    }
    $weekday = integerValue($options['startWeekday'] ?? 0, 'startWeekday');
    $orientation = $options['orientation'] ?? 'weekColumns';
    if ($weekday < 0 || $weekday > 6 || !in_array($orientation, ['weekColumns', 'weekRows'], true)) {
        throw new \InvalidArgumentException('Invalid calendar weekday or orientation');
    }
    $start = $options['start'] ?? '';
    if (!preg_match('/\A([0-9]{4})-([0-9]{2})-([0-9]{2})\z/', $start, $match)
        || (int)$match[1] < 100 || !checkdate((int)$match[2], (int)$match[3], (int)$match[1])) {
        throw new \InvalidArgumentException('Calendar start must be a real YYYY-MM-DD date');
    }
    $beginning = new \DateTimeImmutable($start, new \DateTimeZone('UTC'));
    $leading = ((int)$beginning->format('w') - $weekday + 7) % 7;
    $days = $slots = [];
    for ($index = 0; $index < $count; $index++) {
        $days[] = $beginning->modify('+' . $index . ' days');
        $slots[] = ['week' => intdiv($leading + $index, 7), 'day' => ($leading + $index) % 7];
    }
    return ['days' => $days, 'slots' => $slots, 'weeks' => $slots[$count - 1]['week'] + 1,
        'startWeekday' => $weekday, 'vertical' => $orientation === 'weekRows'];
}

function calendarMatrix(array $values, array $layout, array $options): Matrix
{
    $theme = resolveTheme($options['theme'] ?? null);
    $colorOf = heatScale($values, $theme, $options, 5);
    $size = $layout['vertical'] ? ['columns' => 7, 'rows' => $layout['weeks']]
        : ['columns' => $layout['weeks'], 'rows' => 7];
    $plan = planGrid(regionOf($options, 'calendarHeatmap'), $size, $theme, 'calendarHeatmap');
    $matrix = new Matrix($plan['columns'], $plan['rows'],
        themeOptions($theme, ['default' => EMPTY_PIXEL, 'layout' => $plan['layout']]));
    foreach ($values as $index => $value) {
        $slot = $layout['slots'][$index];
        $x = $slot[$layout['vertical'] ? 'day' : 'week'];
        $y = $slot[$layout['vertical'] ? 'week' : 'day'];
        paintHeat($matrix, $x, $y, $value, $colorOf, $options['missing'] ?? EMPTY_PIXEL);
    }
    return $matrix;
}

function calendarHeatmap(array $days, array|\stdClass $options): array
{
    $options = objectFields($options);
    return calendarMatrix($days, calendarLayout(count($days), $options), $options)->toPart();
}

function calendarMonthWeeks(array $layout, int $minimumGap): array
{
    $labels = [];
    foreach ($layout['days'] as $index => $day) {
        $week = $layout['slots'][$index]['week'];
        if ((int)$day->format('j') === 1
            && (!$labels || $week - $labels[count($labels) - 1]['week'] >= $minimumGap)) {
            $labels[] = ['week' => $week, 'month' => (int)$day->format('n') - 1];
        }
    }
    return $labels;
}

function labeledCalendarHeatmap(array $days, array|\stdClass $options): array
{
    $options = objectFields($options);
    $canvas = requireCanvas($options['canvas'] ?? null, 'labeledCalendarHeatmap');
    [$id, $path] = resolveIdentity($options, 'calendar', 'labeledCalendarHeatmap');
    $theme = resolveTheme($options['theme'] ?? null);
    $layout = calendarLayout(count($days), $options);
    $weekdayNames = $options['weekdayLabels'] ?? CALENDAR_HEATMAP_DEFAULTS['weekdayLabels'];
    $monthNames = $options['monthLabels'] ?? CALENDAR_HEATMAP_DEFAULTS['monthLabels'];
    if (count($weekdayNames) !== 7 || count($monthNames) !== 12) {
        throw new \InvalidArgumentException('Calendar requires 7 weekday and 12 month names');
    }
    $weekdays = $options['showWeekdayLabels'] ?? true;
    $months = $options['showMonthLabels'] ?? true;
    $z = $options['zIndex'] ?? 0;
    $ratio = $options['fontSizeRatio'] ?? .05;
    $points = fontPtOf($ratio, $canvas);
    $rowPt = $points * 1.2 * 1.3;
    $gapPt = $points * .4;
    $widest = static fn($names) => max(array_map(static fn($name) => estimateTextWidthPt($name, $points), $names));
    $vertical = $layout['vertical'];
    $sideReserve = ($weekdays && !$vertical ? $widest($weekdayNames) : 0)
        + ($months && $vertical ? $widest($monthNames) : 0);
    $topReserve = ($weekdays && $vertical) || ($months && !$vertical) ? $rowPt : 0;
    $area = resolveArea($options['frame'] ?? null, $canvas);
    $areaPt = $area->toPt();
    $gridBox = $area->insetPt(['left' => $sideReserve > 0 ? $sideReserve + $gapPt : 0,
        'top' => $topReserve > 0 ? $topReserve + $gapPt : 0]);
    $grid = emittedRegion($gridBox, $canvas);
    $matrix = calendarMatrix($days, $layout, array_replace($options, ['frame' => $gridBox, 'canvas' => $canvas]));
    $placed = place($matrix->toPart(), ['id' => $id . ':grid', 'frame' => $grid['frame'],
        'path' => $path, 'zIndex' => $z]);
    $geometry = fitMatrix($grid['rect'], ['columns' => $matrix->columns, 'rows' => $matrix->rows], $matrix->layout);
    $color = objectFields($options['colors'] ?? [])['label'] ?? $theme['palette']['on'];
    $layers = [$placed];
    $add = static function ($role, $rect, $content, $horizontal)
        use (&$layers, $id, $canvas, $ratio, $color, $z): void {
        $layers[] = labelLayer(['id' => $id . ':' . $role, 'box' => Box::fromPt($rect, $canvas),
            'content' => $content, 'horizontal' => $horizontal, 'vertical' => 'center',
            'fontSizeRatio' => $ratio, 'color' => $color, 'opacity' => .72, 'zIndex' => $z + 1]);
    };
    $left = static fn($x, $width) => max($areaPt['x'], min($x, $areaPt['x'] + $areaPt['width'] - $width));
    $top = static fn($y, $height) => max($areaPt['y'], min($y, $areaPt['y'] + $areaPt['height'] - $height));
    if ($weekdays) {
        for ($day = 1; $day < 7; $day += 2) {
            $name = $weekdayNames[($layout['startWeekday'] + $day) % 7];
            $width = estimateTextWidthPt($name, $points);
            $cell = $vertical ? cellRect($geometry, $day, 0) : cellRect($geometry, 0, $day);
            $rect = $vertical ? ['x' => $left($cell['x'] + $cell['width'] / 2 - $width / 2, $width),
                'y' => $areaPt['y'], 'width' => $width, 'height' => $rowPt]
                : ['x' => $areaPt['x'], 'y' => $top($cell['y'] + $cell['height'] / 2 - $rowPt / 2, $rowPt),
                    'width' => $widest($weekdayNames), 'height' => $rowPt];
            $add('weekday-' . $day, $rect, $name, $vertical ? 'center' : 'right');
        }
    }
    if ($months) {
        $pitch = $vertical ? $geometry['cellHeight'] + $geometry['gapY']
            : $geometry['cellWidth'] + $geometry['gapX'];
        $minimumGap = max(1, floatToInt(ceil(fdiv($vertical ? $rowPt : $widest($monthNames), $pitch))));
        $labels = calendarMonthWeeks($layout, $minimumGap);
        foreach ($labels as $label) {
            $week = $label['week'];
            $name = $monthNames[$label['month']];
            $cell = $vertical ? cellRect($geometry, 0, $week) : cellRect($geometry, $week, 0);
            $width = estimateTextWidthPt($name, $points);
            $rect = $vertical ? ['x' => $areaPt['x'],
                'y' => $top($cell['y'] + $cell['height'] / 2 - $rowPt / 2, $rowPt),
                'width' => $widest($monthNames), 'height' => $rowPt]
                : ['x' => $left($cell['x'], $width), 'y' => $areaPt['y'], 'width' => $width, 'height' => $rowPt];
            $add('month-' . $week, $rect, $name, $vertical ? 'right' : 'left');
        }
    }
    return \GlanceWidgets\Internal\combine($layers);
}
