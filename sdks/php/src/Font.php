<?php
declare(strict_types=1);
namespace GlanceWidgets;

const DEFAULT_FONT_NAME = '3x5';
const DEFAULT_LETTER_SPACING = 1;
const DEFAULT_LINE_SPACING = 1;
const ANCHOR_BOX_WIDTH = 1;

final class PixelFont
{
    public readonly string $name;
    public readonly int $height;
    public readonly int $descent;
    public readonly int $spacing;
    public readonly int $lineHeight;
    private array $glyphs = [];
    private ?string $fallback;

    public function __construct(array|\stdClass $definition)
    {
        $definition = objectFields($definition);
        $this->name = $definition['name'];
        $this->height = positiveInteger($definition['height'], 'Font height');
        $this->descent = integerValue($definition['descent'] ?? 0, 'Font descent');
        $this->spacing = integerValue($definition['spacing'] ?? 1, 'Font spacing');
        if ($this->descent < 0) {
            throw new \InvalidArgumentException('Font descent must not be negative');
        }
        $this->lineHeight = $this->height + $this->descent;
        $this->fallback = $definition['fallback'] ?? null;
        $fixed = array_key_exists('advance', $definition)
            ? integerValue($definition['advance'], 'Font advance') : null;
        foreach (objectFields($definition['glyphs']) as $character => $glyph) {
            $character = (string)$character;
            if (count(codepoints($character)) !== 1) {
                throw new \InvalidArgumentException('Glyph key must be one Unicode code point');
            }
            $isRows = is_array($glyph) && array_is_list($glyph);
            $properties = $isRows ? [] : objectFields($glyph);
            $rows = $isRows ? $glyph : $properties['rows'];
            $width = $rows ? strlen($rows[0]) : 0;
            if (count($rows) > $this->lineHeight) {
                throw new \InvalidArgumentException('Too many glyph rows');
            }
            foreach ($rows as $row) {
                if (strlen($row) !== $width || !preg_match('/\A[#.]*\z/', $row)) {
                    throw new \InvalidArgumentException('Invalid glyph rows');
                }
            }
            if (!$rows && $fixed === null && !array_key_exists('advance', $properties)) {
                throw new \InvalidArgumentException('Glyph without rows requires advance');
            }
            $advance = integerValue($fixed ?? ($properties['advance'] ?? $width), 'Glyph advance');
            if ($advance < 0 || ($fixed !== null && $width > $fixed)) {
                throw new \InvalidArgumentException('Invalid glyph advance');
            }
            while (count($rows) < $this->height) {
                $rows[] = str_repeat('.', $width);
            }
            $inkBottom = 0;
            foreach ($rows as $index => $row) {
                if (str_contains($row, '#')) {
                    $inkBottom = $index + 1;
                }
            }
            $this->glyphs[$character] = compact('rows', 'width', 'advance', 'inkBottom');
        }
        foreach (objectFields($definition['aliases'] ?? []) as $alias => $target) {
            if (count(codepoints((string)$alias)) !== 1 || !isset($this->glyphs[$target])) {
                throw new \InvalidArgumentException('Invalid font alias');
            }
            $this->glyphs[$alias] ??= $this->glyphs[$target];
        }
        if ($this->fallback !== null && !isset($this->glyphs[$this->fallback])) {
            throw new \InvalidArgumentException('Font fallback has no glyph');
        }
    }

    public function monospaceAdvance(): int
    {
        return max([0, ...array_map(static fn($g) => max($g['width'], $g['advance']), $this->glyphs)]);
    }

    public function hasGlyph(string $character): bool
    {
        return isset($this->glyphs[$character]);
    }

    public function glyphFor(string $character): array
    {
        return $this->glyphs[$character] ?? ($this->fallback !== null ? $this->glyphs[$this->fallback]
            : throw new \InvalidArgumentException('Font ' . $this->name . ' has no glyph for ' . $character));
    }
}

final class FontRegistry
{
    private static array $fonts = [];

    public static function define(array|\stdClass $definition): PixelFont
    {
        $font = new PixelFont($definition);
        if (isset(self::$fonts[$font->name])) {
            throw new \InvalidArgumentException('Font already defined');
        }
        self::$fonts[$font->name] = $font;
        return $font;
    }

    public static function get(string $name): PixelFont
    {
        return self::$fonts[$name] ?? throw new \InvalidArgumentException('Unknown font: ' . $name);
    }
}

function defineFont(array|\stdClass $definition): PixelFont
{
    return FontRegistry::define($definition);
}

function getFont(string $name): PixelFont
{
    return FontRegistry::get($name);
}

function resolveFont(string|array|\stdClass|PixelFont|null $font = null): PixelFont
{
    return is_string($font) || $font === null ? getFont($font ?? DEFAULT_FONT_NAME)
        : ($font instanceof PixelFont ? $font : new PixelFont($font));
}

foreach (glob(__DIR__ . '/data/fonts/*.json') as $fontPath) {
    defineFont(json_decode(file_get_contents($fontPath), false, 512, JSON_THROW_ON_ERROR));
}
define(__NAMESPACE__ . '\\FONT_3X5', getFont('3x5'));
define(__NAMESPACE__ . '\\FONT_5X7', getFont('5x7'));
