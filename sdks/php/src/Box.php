<?php
declare(strict_types=1);
namespace GlanceWidgets;

/** Immutable rectangular layout in canvas ratios; point operations require a canvas. */
final class Box
{
    public function __construct(
        public readonly float $left,
        public readonly float $top,
        public readonly float $width,
        public readonly float $height,
        public readonly ?array $canvasSize = null
    ) {
        foreach ([$left, $top, $width, $height] as $number) {
            finiteNumber($number, 'Box geometry');
        }
        if ($width < 0 || $height < 0) {
            throw new \InvalidArgumentException('Box dimensions must not be negative');
        }
    }

    public static function canvas(array|\stdClass|null $canvas = null): self
    {
        return new self(0, 0, 1, 1, $canvas === null ? null : objectFields($canvas));
    }

    public static function fromFrame(array|\stdClass $frame, array|\stdClass $canvas): self
    {
        return self::fromPt(resolveFrame($frame, $canvas), $canvas);
    }

    public static function fromPt(array|\stdClass $rect, array|\stdClass $canvas): self
    {
        $ratio = toRatioRect($rect, $canvas);
        return new self($ratio['x'], $ratio['y'], $ratio['width'], $ratio['height'], objectFields($canvas));
    }

    public function __get(string $name): float
    {
        return match ($name) {
            'right' => $this->left + $this->width,
            'bottom' => $this->top + $this->height,
            default => throw new \LogicException('Unknown Box property: ' . $name)
        };
    }

    public function sub(float $left, float $top, float $width, float $height): self
    {
        return new self($this->left + $left * $this->width, $this->top + $top * $this->height,
            $width * $this->width, $height * $this->height, $this->canvasSize);
    }

    private static function insets(float|int|array|\stdClass $value): array
    {
        $result = [];
        $values = is_numeric($value) ? null : objectFields($value);
        foreach (['left', 'top', 'right', 'bottom'] as $side) {
            $result[$side] = $values === null ? $value : ($values[$side] ?? 0);
        }
        return $result;
    }

    public function inset(float|int|array|\stdClass $value): self
    {
        $insets = self::insets($value);
        return $this->sub($insets['left'], $insets['top'],
            1 - $insets['left'] - $insets['right'], 1 - $insets['top'] - $insets['bottom']);
    }

    public function insetPt(float|int|array|\stdClass $value): self
    {
        $rect = $this->toPt();
        $insets = self::insets($value);
        return self::fromPt(['x' => $rect['x'] + $insets['left'], 'y' => $rect['y'] + $insets['top'],
            'width' => max(0, $rect['width'] - $insets['left'] - $insets['right']),
            'height' => max(0, $rect['height'] - $insets['top'] - $insets['bottom'])], $this->canvasSize);
    }

    public function split(string $direction, int|float|array $countOrWeights, array|\stdClass $options = []): array
    {
        $options = objectFields($options);
        $weights = is_array($countOrWeights) ? $countOrWeights
            : array_fill(0, positiveInteger($countOrWeights, 'Split count'), 1);
        $total = 0;
        foreach ($weights as $weight) {
            if (!($weight >= 0)) {
                throw new \InvalidArgumentException('Split needs nonnegative weights with a positive total');
            }
            $total += $weight;
        }
        if (!$weights || !($total > 0)) {
            throw new \InvalidArgumentException('Split needs nonnegative weights with a positive total');
        }
        $vertical = $direction === 'rows';
        $extent = $vertical ? $this->height : $this->width;
        $gap = ($options['gap'] ?? 0) * $extent;
        if (($options['gapPt'] ?? null) !== null) {
            $canvas = requireCanvas($this->canvasSize);
            $gap = $options['gapPt'] / $canvas[$vertical ? 'height' : 'width'];
        }
        $available = $extent - $gap * (count($weights) - 1);
        if ($available < 0) {
            throw new \InvalidArgumentException('Gaps are larger than the box');
        }
        $offset = $vertical ? $this->top : $this->left;
        $parts = [];
        foreach ($weights as $weight) {
            $size = $available * $weight / $total;
            $parts[] = $vertical ? new self($this->left, $offset, $this->width, $size, $this->canvasSize)
                : new self($offset, $this->top, $size, $this->height, $this->canvasSize);
            $offset += $size + $gap;
        }
        return $parts;
    }

    public function rows(int|float|array $countOrWeights, array|\stdClass $options = []): array
    {
        return $this->split('rows', $countOrWeights, $options);
    }

    public function columns(int|float|array $countOrWeights, array|\stdClass $options = []): array
    {
        return $this->split('columns', $countOrWeights, $options);
    }

    public function toPt(): array
    {
        $canvas = requireCanvas($this->canvasSize);
        return ['x' => $this->left * $canvas['width'], 'y' => $this->top * $canvas['height'],
            'width' => $this->width * $canvas['width'], 'height' => $this->height * $canvas['height']];
    }

    public function toFrame(): array
    {
        $axis = static function ($start, $size): array {
            if ($size > 1.5) {
                throw new \InvalidArgumentException('Frame size exceeds 1.5');
            }
            if ($size < .02) {
                return [jsRound(($start + $size / 2 - .01) * 1e6) / 1e6, .02];
            }
            return [jsRound($start * 1e6) / 1e6, jsRound($size * 1e6) / 1e6];
        };
        [$left, $width] = $axis($this->left, $this->width);
        [$top, $height] = $axis($this->top, $this->height);
        return compact('left', 'top', 'width', 'height');
    }
}
