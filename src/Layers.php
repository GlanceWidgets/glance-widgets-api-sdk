<?php
declare(strict_types=1);
namespace GlanceWidgets;

const TEXT_DEFAULTS = ['color' => '#FFFFFF', 'textOpacity' => 1, 'opacity' => 0, 'fontSizeRatio' => .12,
    'fontWeight' => '400', 'fontFamily' => 'system', 'letterSpacing' => 0, 'horizontalAlignment' => 'left',
    'verticalAlignment' => 'top', 'autoWrap' => false, 'paddingRatio' => 0];
const LINE_DEFAULTS = ['color' => '#FFFFFF', 'opacity' => 1, 'strokeWidth' => 2, 'pattern' => 'solid', 'cap' => 'butt'];
const SHAPE_DEFAULTS = ['color' => '#2A63D4', 'opacity' => 1, 'cornerRadiusRatio' => 0];
const ICON_DEFAULTS = ['color' => '#FFFFFF', 'textOpacity' => 1];
const IMAGE_DEFAULTS = ['opacity' => 1, 'cornerRadiusRatio' => 0];
const CURVE_DEFAULTS = ['color' => '#6BA7FF', 'opacity' => 1, 'strokeWidth' => 2, 'lineCap' => 'round',
    'lineJoin' => 'round', 'fillOpacity' => 0, 'plotPadding' => 0];

function normalizeStyleColors(array $style): array
{
    foreach ($style as $key => $value) {
        if (($key === 'color' || str_ends_with($key, 'Color')) && is_string($value)) {
            $style[$key] = normalizeColor($value);
        } elseif ($key === 'gradientColors' && is_array($value)) {
            $style[$key] = array_map(__NAMESPACE__ . '\\normalizeColor', $value);
        }
    }
    return $style;
}

function layerBase(string $type, array $options): array
{
    $frame = $options['frame'];
    $node = ['frame' => $frame instanceof Box ? $frame->toFrame() : copyJson($frame)];
    foreach (['zIndex', 'rotation', 'transform'] as $key) {
        if (($options[$key] ?? null) !== null) { $node[$key] = copyJson($options[$key]); }
    }
    $path = $options['dataPath'] ?? layerPath($options['id']);
    return ['layers' => [['id' => layerId($options['id']), 'type' => $type, 'dataPath' => $path]], 'data' => nestAtPath($path, $node)];
}

function finishLayer(array $base, array $fields): array
{
    $path = $base['layers'][0]['dataPath'];
    $node = array_merge(resolveDataPath($base['data'], $path), $fields);
    $base['data'] = nestAtPath($path, $node);
    return $base;
}

function text(array|\stdClass $options): array
{
    $options = objectFields($options);
    $content = ['text' => $options['text'] ?? ''];
    return finishLayer(layerBase('text', $options), ['content' => $content,
        'style' => normalizeStyleColors(array_replace(TEXT_DEFAULTS, objectFields($options['style'] ?? [])))]);
}

function line(array|\stdClass $options): array
{
    $options = objectFields($options);
    return finishLayer(layerBase('line', $options), ['content' => ['direction' => $options['direction']],
        'style' => normalizeStyleColors(array_replace(LINE_DEFAULTS, objectFields($options['style'] ?? [])))]);
}

function axisLine(array|\stdClass $options, bool $vertical): array
{
    $options = objectFields($options);
    $length = $options['to'] - $options['from'];
    if ($length < FRAME_SIZE_MIN) {
        throw new \InvalidArgumentException('Lines need at least .02 of the canvas');
    }
    $frame = $vertical ? new Box($options['at'], $options['from'], 0, $length)
        : new Box($options['from'], $options['at'], $length, 0);
    return line(array_replace($options, ['frame' => $frame, 'direction' => $vertical ? 'vertical' : 'horizontal']));
}

function horizontalLine(array|\stdClass $options): array
{
    return axisLine($options, false);
}

function verticalLine(array|\stdClass $options): array
{
    return axisLine($options, true);
}

function shape(array|\stdClass $options): array
{
    $options = objectFields($options);
    $frame = $options['frame'];
    if ($frame instanceof Box && $frame->canvasSize !== null) {
        $rect = $frame->toPt();
        if ($rect['width'] < SHAPE_MIN_SIZE_PT || $rect['height'] < SHAPE_MIN_SIZE_PT) {
            throw new \InvalidArgumentException('Shape smaller than App minimum 24pt');
        }
    }
    $style = array_replace(SHAPE_DEFAULTS, objectFields($options['style'] ?? []));
    $style['backgroundColor'] ??= $style['color'];
    return finishLayer(layerBase('shape', $options), ['content' => new \stdClass(), 'style' => normalizeStyleColors($style)]);
}

function icon(array|\stdClass $options): array
{
    $options = objectFields($options);
    if (count(array_intersect(['name', 'glyph', 'url'], array_keys($options))) !== 1) {
        throw new \InvalidArgumentException('Icon needs exactly one source');
    }
    if (array_key_exists('name', $options) || array_key_exists('glyph', $options)) {
        $glyph = isset($options['name']) ? iconGlyph($options['name']) : objectFields($options['glyph']);
        $content = array_intersect_key($glyph,
            array_flip(['icon', 'iconCodePoint', 'iconFont']));
    } elseif (array_key_exists('url', $options)) {
        $content = ['url' => $options['url']];
    } else {
        $content = new \stdClass();
    }
    return finishLayer(layerBase('icon', $options), ['content' => $content,
        'style' => normalizeStyleColors(array_replace(ICON_DEFAULTS, objectFields($options['style'] ?? [])))]);
}

function image(array|\stdClass $options): array
{
    $options = objectFields($options);
    if (!array_key_exists('url', $options)) {
        throw new \InvalidArgumentException('Image needs url');
    }
    return finishLayer(layerBase('image', $options), ['content' => ['url' => $options['url']],
        'style' => normalizeStyleColors(array_replace(IMAGE_DEFAULTS, objectFields($options['style'] ?? [])))]);
}

function curve(array|\stdClass $options): array
{
    $options = objectFields($options);
    $data = objectFields($options['data'] ?? []);
    $function = ($data['mode'] ?? null) === 'function'
        || (!array_key_exists('mode', $data) && array_key_exists('expression', $data));
    if ($function && (!isset($data['domain']) || !isset($data['yDomain']))) {
        throw new \InvalidArgumentException('Function curve needs domain and yDomain');
    }
    if (count($data['points'] ?? []) > 1024 || count($data['markers'] ?? []) > 32) {
        throw new \InvalidArgumentException('Curve exceeds point/marker limit');
    }
    foreach ($data['markers'] ?? [] as $marker) {
        $anchor = objectFields(objectFields($marker)['at'] ?? []);
        foreach ($anchor['offsetPt'] ?? [0, 0] as $offset) {
            if (abs($offset) > 64) {
                throw new \InvalidArgumentException('Marker offset must stay within ±64pt');
            }
        }
    }
    $content = copyJson($data);
    $content['mode'] = $data['mode'] ?? (isset($data['expression']) ? 'function' : 'samples');
    $content['interpolation'] = $data['interpolation'] ?? 'monotone';
    $style = array_replace(CURVE_DEFAULTS, objectFields($options['style'] ?? []));
    $style['fillColor'] ??= $style['color'];
    return finishLayer(layerBase('curve', $options), ['content' => $content, 'style' => normalizeStyleColors($style)]);
}
