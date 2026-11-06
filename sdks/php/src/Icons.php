<?php
declare(strict_types=1);
namespace GlanceWidgets;

const DEFAULT_ICON_NAME = 'ph_activity';

final class IconCatalog
{
    private static ?array $entries = null;

    public static function all(): array
    {
        return self::$entries ??= dataFile('icons.json')['icons'];
    }
}

function hasIcon(string $name): bool
{
    return isset(IconCatalog::all()[$name]);
}

function iconNames(): array
{
    return array_keys(IconCatalog::all());
}

function iconGlyph(string $name): array
{
    $entry = IconCatalog::all()[$name] ?? throw new \InvalidArgumentException('Unknown icon: ' . $name);
    return ['icon' => $name] + $entry;
}
