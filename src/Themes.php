<?php
declare(strict_types=1);
namespace GlanceWidgets;

const OFF_PIXEL_OPACITY = .3;
const ROOT_RING_OPACITY_THRESHOLD = .4;
const DEFAULT_THEME_NAME = 'lcd';
const STATUS_COLORS = ['good' => '#34C759', 'warn' => '#FF9F0A', 'bad' => '#FF453A', 'neutral' => '#8E8E93'];

function makeTheme(string $name, array $colors, float $corner, string $gap,
    float $rotation = 0, float $halo = 0): array
{
    $palette = array_combine(['on', 'off', 'dim', 'accent'], $colors);
    $shape = ['cornerRadius' => $corner, 'rotation' => $rotation];
    return ['name' => $name, 'default' => ['color' => $palette['off'], 'opacity' => .3] + $shape + ['halo' => 0],
        'base' => ['color' => $palette['on'], 'opacity' => 1] + $shape + ['halo' => $halo],
        'layout' => ['gapMode' => $gap, 'edgeInsetRatio' => 0, 'cellAspectRatio' => 1], 'palette' => $palette];
}

define(__NAMESPACE__ . '\\THEMES', [
    'led' => makeTheme('led', ['#FF4D2E', '#3A1F1B', '#A33A26', '#FFD23F'], .5, 'far', 0, .6),
    'lcd' => makeTheme('lcd', ['#9BFF6B', '#27402B', '#5E9B4A', '#E6FF8A'], 0, 'near'),
    'dot' => makeTheme('dot', ['#F2F2F7', '#48484A', '#8E8E93', '#0A84FF'], .5, 'extraFar'),
    'diamond' => makeTheme('diamond', ['#5AC8FA', '#1C3A4A', '#3D7C99', '#FF9F0A'], .1, 'extraFar', 45),
    'soft' => makeTheme('soft', ['#FFE2B8', '#4A3F38', '#B59A82', '#FF7A59'], .3, 'default', 0, .25),
]);

function getTheme(string $name): array
{
    return THEMES[$name] ?? throw new \InvalidArgumentException('Unknown theme: ' . $name);
}

function themeOptions(array|\stdClass $theme, array|\stdClass $overrides = []): array
{
    $theme = objectFields($theme);
    $overrides = objectFields($overrides);
    return ['default' => copyJson($overrides['default'] ?? $theme['default']),
        'base' => array_replace(objectFields($theme['base']), objectFields($overrides['base'] ?? [])),
        'layout' => array_replace(objectFields($theme['layout']), objectFields($overrides['layout'] ?? []))];
}
