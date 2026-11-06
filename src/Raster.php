<?php
declare(strict_types=1);
namespace GlanceWidgets;

const RASTER_MAX_EXTENT = 4096;
const RASTER_MAX_COORDINATE = 1 << 24;
const ARC_DIRECTION_SCALE = 1 << 20;

/** Integer cell-center rasterization, clipped before scanning bounded shapes. */
final class Raster
{
    public static function line(int $x, int $y, int $endX, int $endY, callable $emit): void
    {
        $deltaX = abs($endX - $x);
        $deltaY = -abs($endY - $y);
        $stepX = $x < $endX ? 1 : -1;
        $stepY = $y < $endY ? 1 : -1;
        $error = $deltaX + $deltaY;
        while (true) {
            $emit($x, $y);
            if ($x === $endX && $y === $endY) {
                return;
            }
            $twice = 2 * $error;
            if ($twice >= $deltaY) {
                $error += $deltaY;
                $x += $stepX;
            }
            if ($twice <= $deltaX) {
                $error += $deltaX;
                $y += $stepY;
            }
        }
    }

    public static function mask(
        int $x, int $y, int $width, int $height, callable $contains, bool $fill,
        int $columns, int $rows, callable $emit
    ): void {
        if ($width < 1 || $height < 1 || $width > RASTER_MAX_EXTENT || $height > RASTER_MAX_EXTENT) {
            throw new \InvalidArgumentException('Shape dimensions must be 1..4096');
        }
        $inside = static fn($column, $row) => $column >= 0 && $column < $width && $row >= 0
            && $row < $height && $contains($column, $row);
        for ($row = max(0, -$y); $row < min($height, $rows - $y); $row++) {
            for ($column = max(0, -$x); $column < min($width, $columns - $x); $column++) {
                $edge = !$inside($column - 1, $row) || !$inside($column + 1, $row)
                    || !$inside($column, $row - 1) || !$inside($column, $row + 1);
                if ($inside($column, $row) && ($fill || $edge)) {
                    $emit($x + $column, $y + $row);
                }
            }
        }
    }

    public static function arc(
        int $centerX, int $centerY, int $radius, float $start, float $end, int $thickness,
        int $columns, int $rows, callable $emit
    ): void {
        if ($radius < 0 || $radius > RASTER_MAX_EXTENT || $thickness < 1 || $thickness > RASTER_MAX_EXTENT) {
            throw new \InvalidArgumentException('Invalid arc radius/thickness');
        }
        finiteNumber($start, 'Arc angle');
        finiteNumber($end, 'Arc angle');
        $sweep = $end - $start;
        if ($sweep <= 0) {
            return;
        }
        $direction = static fn($angle) => [jsRound(sin($angle * M_PI / 180) * ARC_DIRECTION_SCALE),
            -jsRound(cos($angle * M_PI / 180) * ARC_DIRECTION_SCALE)];
        [$startX, $startY] = $direction($start);
        [$endX, $endY] = $direction($end);
        $hole = $radius - $thickness;
        $inner = $hole >= 0 ? $hole * $hole + $hole : -1;
        for ($y = max(0, $centerY - $radius); $y <= min($rows - 1, $centerY + $radius); $y++) {
            for ($x = max(0, $centerX - $radius); $x <= min($columns - 1, $centerX + $radius); $x++) {
                $deltaX = $x - $centerX;
                $deltaY = $y - $centerY;
                $after = $startX * $deltaY - $startY * $deltaX >= 0;
                $before = $deltaX * $endY - $deltaY * $endX >= 0;
                $segment = $sweep >= 360 || ($sweep < 180 ? $after && $before : $after || $before);
                $distance = $deltaX * $deltaX + $deltaY * $deltaY;
                if ($inner < $distance && $distance <= $radius * $radius + $radius && $segment) {
                    $emit($x, $y);
                }
            }
        }
    }

    public static function polygon(array $points, bool $fill, int $columns, int $rows, callable $emit): void
    {
        if (count($points) < 3) {
            throw new \InvalidArgumentException('Polygon needs at least three points');
        }
        foreach ($points as $index => $point) {
            if (!is_array($point) || count($point) !== 2) {
                throw new \InvalidArgumentException('Polygon points must be coordinate pairs');
            }
            foreach ($point as $coordinate) {
                if (abs(integerValue($coordinate, 'Polygon coordinate')) > RASTER_MAX_COORDINATE) {
                    throw new \InvalidArgumentException('Polygon coordinate exceeds permitted range');
                }
            }
            $points[$index] = array_map(__NAMESPACE__ . '\\integerValue', $point);
        }
        $edges = [];
        foreach ($points as $index => $point) {
            $next = $points[($index + 1) % count($points)];
            $edges[] = [$point, $next];
            self::line($point[0], $point[1], $next[0], $next[1], $emit);
        }
        if (!$fill) {
            return;
        }
        $xs = array_column($points, 0);
        $ys = array_column($points, 1);
        for ($y = max(0, min($ys)); $y <= min($rows - 1, max($ys)); $y++) {
            $centerY = 2 * $y + 1;
            for ($x = max(0, min($xs)); $x <= min($columns - 1, max($xs)); $x++) {
                $right = $through = 0;
                foreach ($edges as [$first, $second]) {
                    if ((2 * $first[1] <= $centerY) === (2 * $second[1] <= $centerY)) {
                        continue;
                    }
                    $deltaY = $second[1] - $first[1];
                    $cross = (2 * $first[0] - 2 * $x - 1) * $deltaY
                        + ($centerY - 2 * $first[1]) * ($second[0] - $first[0]);
                    if ($cross === 0) {
                        $through++;
                    } elseif (($cross > 0) === ($deltaY > 0)) {
                        $right++;
                    }
                }
                if ($right % 2 || ($right + $through) % 2) {
                    $emit($x, $y);
                }
            }
        }
    }
}
