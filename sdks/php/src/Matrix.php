<?php
declare(strict_types=1);
namespace GlanceWidgets;

const EMPTY_PIXEL = 'empty';
define(__NAMESPACE__ . '\\EMPTY', EMPTY_PIXEL);
const DEFAULT_PIXEL_BASE = ['color' => '#FFFFFF', 'opacity' => 1, 'cornerRadius' => 0, 'rotation' => 0, 'halo' => 0];
const BATCH_MIN_OCCURRENCES = 2;

/** Mutable pixel grid. Drawing clips; explicit cell access rejects coordinates outside the grid. */
final class Matrix
{
    public readonly int $columns;
    public readonly int $rows;
    public readonly array $layout;
    public readonly array $base;
    private array|string $defaultStyle;
    private array $cells;

    public function __construct(int|float $columns, int|float $rows, array|\stdClass $options = [])
    {
        $columns = positiveInteger($columns, 'columns');
        $rows = positiveInteger($rows, 'rows');
        if ($columns < 1 || $rows < 1 || $columns * $rows > MAX_MATRIX_CELLS) {
            throw new \InvalidArgumentException('Matrix needs 1..4096 cells');
        }
        $options = objectFields($options);
        $this->columns = $columns;
        $this->rows = $rows;
        $layout = array_intersect_key(objectFields($options['layout'] ?? []), array_flip([
            'gapMode', 'edgeInsetRatio', 'cellAspectRatio', 'columnGapRatio', 'rowGapRatio']));
        $layout = array_filter($layout, static fn($value) => $value !== null);
        $this->layout = array_replace(['gapMode' => 'default', 'edgeInsetRatio' => 0, 'cellAspectRatio' => 1], $layout);
        if (!is_string($this->layout['gapMode']) || !array_key_exists($this->layout['gapMode'], GAP_MODE_RATIOS)) {
            throw new \InvalidArgumentException('Unknown gapMode');
        }
        foreach ($this->layout as $key => $number) {
            if ($key !== 'gapMode') {
                finiteNumber($number, 'Layout ' . $key);
            }
        }
        $this->defaultStyle = self::resolvePixel($options['default'] ?? EMPTY_PIXEL, DEFAULT_PIXEL_BASE);
        $visible = $this->defaultStyle === EMPTY_PIXEL ? DEFAULT_PIXEL_BASE : $this->defaultStyle;
        $this->base = self::resolvePixel(array_replace($visible, objectFields($options['base'] ?? [])),
            DEFAULT_PIXEL_BASE);
        $this->cells = array_fill(0, $columns * $rows, null);
    }

    private static function resolvePixel(mixed $value, array $base): array|string|null
    {
        if ($value === null || $value === EMPTY_PIXEL) {
            return $value;
        }
        $pixel = array_replace($base, is_string($value) ? ['color' => $value] : objectFields($value));
        foreach (['opacity', 'cornerRadius', 'rotation', 'halo'] as $field) {
            finiteNumber($pixel[$field], 'Pixel ' . $field);
        }
        return ['color' => normalizeColor($pixel['color']), 'opacity' => clamp($pixel['opacity'], 0, 1),
            'cornerRadius' => clamp($pixel['cornerRadius'], 0, .5), 'rotation' => $pixel['rotation'],
            'halo' => clamp($pixel['halo'], 0, 1)];
    }

    public function contains(int|float $x, int|float $y): bool
    {
        return is_finite((float)$x) && is_finite((float)$y) && floor($x) == $x && floor($y) == $y
            && $x >= 0 && $y >= 0 && $x < $this->columns && $y < $this->rows;
    }

    private function index(int|float $x, int|float $y): int
    {
        if (!$this->contains($x, $y)) {
            throw new \OutOfBoundsException('Cell outside matrix');
        }
        return (int)$y * $this->columns + (int)$x;
    }

    private function plot(int|float $x, int|float $y, mixed $pixel): void
    {
        if ($this->contains($x, $y)) {
            $this->cells[(int)$y * $this->columns + (int)$x] = $pixel;
        }
    }

    public function get(int|float $x, int|float $y): array|string
    {
        return $this->cells[$this->index($x, $y)] ?? $this->defaultStyle;
    }

    public function isSet(int|float $x, int|float $y): bool
    {
        return $this->cells[$this->index($x, $y)] !== null;
    }

    public function set(int|float $x, int|float $y, mixed $style): self
    {
        $this->cells[$this->index($x, $y)] = self::resolvePixel($style, $this->base);
        return $this;
    }

    public function fill(mixed $style): self
    {
        $this->cells = array_fill(0, count($this->cells), self::resolvePixel($style, $this->base));
        return $this;
    }

    public function clear(): self
    {
        return $this->fill(null);
    }

    public function blit(self $other, int|float $x, int|float $y): self
    {
        for ($row = 0; $row < $other->rows; $row++) {
            for ($column = 0; $column < $other->columns; $column++) {
                $pixel = $other->cells[$row * $other->columns + $column];
                if ($pixel !== null && $pixel !== EMPTY_PIXEL) {
                    $this->plot($x + $column, $y + $row, $pixel);
                }
            }
        }
        return $this;
    }

    public function line(int|float $x, int|float $y, int|float $endX, int|float $endY, mixed $style): self
    {
        [$x, $y, $endX, $endY] = array_map(__NAMESPACE__ . '\\integerValue', [$x, $y, $endX, $endY]);
        $pixel = self::resolvePixel($style, $this->base);
        Raster::line($x, $y, $endX, $endY, fn($column, $row) => $this->plot($column, $row, $pixel));
        return $this;
    }

    public function rect(int|float $x, int|float $y, int|float $width, int|float $height, mixed $style,
        array|\stdClass $options = []): self
    {
        [$x, $y, $width, $height] = array_map(__NAMESPACE__ . '\\integerValue', [$x, $y, $width, $height]);
        $pixel = self::resolvePixel($style, $this->base);
        $fill = objectFields($options)['fill'] ?? true;
        for ($row = $y; $row < $y + $height; $row++) {
            for ($column = $x; $column < $x + $width; $column++) {
                if ($fill || $row === $y || $row === $y + $height - 1
                    || $column === $x || $column === $x + $width - 1) {
                    $this->plot($column, $row, $pixel);
                }
            }
        }
        return $this;
    }

    private function mask(int|float $x, int|float $y, int|float $width, int|float $height, mixed $style,
        array|\stdClass $options, callable $contains): self
    {
        $pixel = self::resolvePixel($style, $this->base);
        Raster::mask($x, $y, $width, $height, $contains, (bool)(objectFields($options)['fill'] ?? true),
            $this->columns, $this->rows, fn($column, $row) => $this->plot($column, $row, $pixel));
        return $this;
    }

    public function ellipse(int|float $x, int|float $y, int|float $width, int|float $height, mixed $style,
        array|\stdClass $options = []): self
    {
        [$x, $y, $width, $height] = array_map(__NAMESPACE__ . '\\integerValue', [$x, $y, $width, $height]);
        return $this->mask($x, $y, $width, $height, $style, $options,
            static fn($column, $row) => (2 * $column + 1 - $width) ** 2 * $height * $height
                + (2 * $row + 1 - $height) ** 2 * $width * $width <= $width * $width * $height * $height);
    }

    public function roundRect(int|float $x, int|float $y, int|float $width, int|float $height,
        int|float $radius, mixed $style, array|\stdClass $options = []): self
    {
        $radius = integerValue($radius, 'Radius');
        if ($radius < 0) {
            throw new \InvalidArgumentException('Radius must not be negative');
        }
        [$x, $y, $width, $height] = array_map(__NAMESPACE__ . '\\integerValue', [$x, $y, $width, $height]);
        $radius = min($radius, intdiv(min($width, $height), 2));
        $contains = static function ($column, $row) use ($width, $height, $radius): bool {
            $nearX = min($column, $width - 1 - $column);
            $nearY = min($row, $height - 1 - $row);
            return $nearX >= $radius || $nearY >= $radius
                || (2 * $nearX + 1 - 2 * $radius) ** 2 + (2 * $nearY + 1 - 2 * $radius) ** 2 <= 4 * $radius ** 2;
        };
        return $this->mask($x, $y, $width, $height, $style, $options, $contains);
    }

    public function circle(int|float $centerX, int|float $centerY, int|float $radius, mixed $style,
        array|\stdClass $options = []): self
    {
        $radius = integerValue($radius, 'Radius');
        if ($radius < 0) {
            throw new \InvalidArgumentException('Radius must not be negative');
        }
        $centerX = integerValue($centerX, 'Center x');
        $centerY = integerValue($centerY, 'Center y');
        return $this->ellipse($centerX - $radius, $centerY - $radius, 2 * $radius + 1, 2 * $radius + 1,
            $style, $options);
    }

    public function arc(int|float $centerX, int|float $centerY, int|float $radius, float $start,
        float $end, mixed $style, array|\stdClass $options = []): self
    {
        $pixel = self::resolvePixel($style, $this->base);
        [$centerX, $centerY, $radius] = array_map(__NAMESPACE__ . '\\integerValue', [$centerX, $centerY, $radius]);
        $thickness = integerValue(objectFields($options)['thickness'] ?? 1, 'Thickness');
        Raster::arc($centerX, $centerY, $radius, $start, $end, $thickness,
            $this->columns, $this->rows, fn($column, $row) => $this->plot($column, $row, $pixel));
        return $this;
    }

    public function polygon(array $points, mixed $style, array|\stdClass $options = []): self
    {
        $pixel = self::resolvePixel($style, $this->base);
        Raster::polygon($points, objectFields($options)['fill'] ?? true, $this->columns, $this->rows,
            fn($column, $row) => $this->plot($column, $row, $pixel));
        return $this;
    }

    public function floodFill(int|float $x, int|float $y, mixed $style, array|\stdClass $options = []): self
    {
        $x = integerValue($x, 'Flood seed x');
        $y = integerValue($y, 'Flood seed y');
        if (!$this->contains($x, $y)) {
            return $this;
        }
        $target = $this->get($x, $y);
        $pixel = self::resolvePixel($style, $this->base);
        $offsets = [[1, 0], [-1, 0], [0, 1], [0, -1]];
        if (objectFields($options)['diagonal'] ?? false) {
            $offsets = array_merge($offsets, [[1, 1], [1, -1], [-1, 1], [-1, -1]]);
        }
        $pending = [[$x, $y]];
        $visited = [$this->index($x, $y) => true];
        while ($pending) {
            [$column, $row] = array_pop($pending);
            $this->plot($column, $row, $pixel);
            foreach ($offsets as [$deltaX, $deltaY]) {
                $nextX = $column + $deltaX;
                $nextY = $row + $deltaY;
                if (!$this->contains($nextX, $nextY)) {
                    continue;
                }
                $index = $this->index($nextX, $nextY);
                if (!isset($visited[$index]) && $this->get($nextX, $nextY) == $target) {
                    $pending[] = [$nextX, $nextY];
                    $visited[$index] = true;
                }
            }
        }
        return $this;
    }

    public function text(string $content, int|float $x, int|float $y, array|\stdClass $options = []): self
    {
        $x = integerValue($x, 'Text x');
        $y = integerValue($y, 'Text y');
        $options = objectFields($options);
        $style = array_key_exists('style', $options) ? $options['style'] : $this->base;
        $pixel = self::resolvePixel($style, $this->base);
        foreach (layoutText($content, $options)['glyphs'] as $placed) {
            foreach ($placed['glyph']['rows'] as $row => $line) {
                foreach (str_split($line) as $column => $character) {
                    if ($character === '#') {
                        $this->plot($x + $placed['x'] + $column, $y + $placed['y'] + $row, $pixel);
                    }
                }
            }
        }
        return $this;
    }

    private function blank(int|float $columns, int|float $rows): self
    {
        return new self($columns, $rows,
            ['default' => $this->defaultStyle, 'base' => $this->base, 'layout' => $this->layout]);
    }

    public function clone(): self
    {
        return clone $this;
    }

    public function flipX(): self
    {
        $result = $this->blank($this->columns, $this->rows);
        for ($row = 0; $row < $this->rows; $row++) {
            for ($column = 0; $column < $this->columns; $column++) {
                $result->cells[$row * $this->columns + $this->columns - 1 - $column]
                    = $this->cells[$row * $this->columns + $column];
            }
        }
        return $result;
    }

    public function flipY(): self
    {
        $result = $this->blank($this->columns, $this->rows);
        for ($row = 0; $row < $this->rows; $row++) {
            for ($column = 0; $column < $this->columns; $column++) {
                $result->cells[($this->rows - 1 - $row) * $this->columns + $column]
                    = $this->cells[$row * $this->columns + $column];
            }
        }
        return $result;
    }

    public function rotate90(int|float $turns = 1): self
    {
        $turns = integerValue($turns, 'Rotation turns');
        $result = $this->clone();
        for ($turn = 0; $turn < (($turns % 4 + 4) % 4); $turn++) {
            $previous = $result;
            $result = $previous->blank($previous->rows, $previous->columns);
            for ($row = 0; $row < $previous->rows; $row++) {
                for ($column = 0; $column < $previous->columns; $column++) {
                    $result->cells[$column * $previous->rows + $previous->rows - 1 - $row]
                        = $previous->cells[$row * $previous->columns + $column];
                }
            }
        }
        return $result;
    }

    public function crop(int|float $x, int|float $y, int|float $width, int|float $height): self
    {
        [$x, $y, $width, $height] = array_map(__NAMESPACE__ . '\\integerValue', [$x, $y, $width, $height]);
        $result = $this->blank($width, $height);
        for ($row = 0; $row < $height; $row++) {
            for ($column = 0; $column < $width; $column++) {
                if ($this->contains($x + $column, $y + $row)) {
                    $result->cells[$row * $width + $column] = $this->cells[$this->index($x + $column, $y + $row)];
                }
            }
        }
        return $result;
    }

    public function toRoot(): array
    {
        $groups = [];
        foreach ($this->cells as $index => $pixel) {
            if ($pixel === null || $pixel == $this->defaultStyle) {
                continue;
            }
            $value = $pixel === EMPTY_PIXEL ? ['empty' => true] : $pixel;
            $key = json_encode(canonicalNumbers($value), JSON_THROW_ON_ERROR);
            $groups[$key] ??= ['pixel' => $value, 'coordinates' => []];
            $groups[$key]['coordinates'][] = [intdiv($index, $this->columns), $index % $this->columns];
        }
        $pixels = array_fill(0, $this->rows, array_fill(0, $this->columns, null));
        $batch = [];
        foreach ($groups as $group) {
            if (count($group['coordinates']) >= BATCH_MIN_OCCURRENCES) {
                $batch[] = $group;
            } else {
                foreach ($group['coordinates'] as [$row, $column]) {
                    $pixels[$row][$column] = $group['pixel'];
                }
            }
        }
        $result = ['pixels' => $pixels, 'default' => $this->defaultStyle === EMPTY_PIXEL
            ? ['empty' => true] : $this->defaultStyle, 'layout' => $this->layout];
        if ($batch) {
            $result['batch'] = $batch;
        }
        return $result;
    }

    public function toPart(): array
    {
        return ['content' => $this->toRoot()];
    }
}
