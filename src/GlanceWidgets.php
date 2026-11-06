<?php
declare(strict_types=1);
namespace GlanceWidgets;

foreach (['Support', 'Numbers', 'Protocol', 'Color', 'Geometry', 'Box', 'Raster', 'Matrix', 'Font',
    'TextMatrix', 'Themes', 'Icons', 'Output', 'Layers', 'Signature', 'Pixelate', 'Components/Shared',
    'Components/Modern', 'Components/PixelProgress', 'Components/Status', 'Components/PixelText', 'Components/Heatmap',
    'Components/PixelBarChart', 'Components/LineChart', 'Components/Stat'] as $module) {
    require_once __DIR__ . '/' . $module . '.php';
}
