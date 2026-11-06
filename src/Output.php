<?php
declare(strict_types=1);
namespace GlanceWidgets;

function layerId(string $id): string
{
    if ($id === '') {
        throw new \InvalidArgumentException('Layer id must not be empty');
    }
    return str_starts_with($id, LAYER_ID_PREFIX) ? $id : LAYER_ID_PREFIX . $id;
}

function mergeData(array $target, array $source, bool $strict, string $path): array
{
    foreach ($source as $key => $value) {
        $childPath = $path . '.' . $key;
        if (array_key_exists($key, $target) && isObject($target[$key]) && isObject($value)) {
            $children = mergeData(objectFields($target[$key]), objectFields($value), $strict, $childPath);
            $target[$key] = objectValue($children);
        } elseif (array_key_exists($key, $target) && $strict) {
            throw new \InvalidArgumentException('Merge conflict at ' . $childPath);
        } else {
            $target[$key] = copyJson($value);
        }
    }
    return $target;
}

function merge(array|\stdClass ...$parts): array
{
    $strict = false;
    if ($parts) {
        $last = objectFields($parts[count($parts) - 1]);
        if ($last !== [] && array_keys($last) === ['onConflict']) {
            $strict = $last['onConflict'] === 'error';
            array_pop($parts);
        }
    }
    $result = ['protocol' => PROTOCOL_VERSION];
    $rootKeys = ['pixels', 'default', 'layout', 'batch'];
    foreach ($parts as $part) {
        if (!isObject($part)) {
            throw new \InvalidArgumentException('Merge parts must be JSON objects');
        }
        $part = objectFields($part);
        if (array_key_exists('content', $part) || array_key_exists('style', $part)) {
            throw new \InvalidArgumentException('MatrixPart must be placed before merge');
        }
        if (array_key_exists('protocol', $part)
            && ((!is_int($part['protocol']) && !is_float($part['protocol'])) || $part['protocol'] != 1)) {
            throw new \InvalidArgumentException('Unsupported protocol');
        }
        if (array_intersect($rootKeys, array_keys($part))) {
            if (!array_key_exists('pixels', $part)) {
                throw new \InvalidArgumentException('Root fields require pixels');
            }
            if (array_key_exists('pixels', $result) && $strict) {
                throw new \InvalidArgumentException('Merge conflict: root matrix');
            }
            foreach ($rootKeys as $key) {
                unset($result[$key]);
                if (array_key_exists($key, $part)) {
                    $result[$key] = copyJson($part[$key]);
                }
            }
        }
        if (array_key_exists('data', $part)) {
            if (!isObject($part['data'])) {
                throw new \InvalidArgumentException('Data must be an object');
            }
            $data = mergeData(objectFields($result['data'] ?? []), objectFields($part['data']), $strict, 'data');
            $result['data'] = objectValue($data);
        }
        if (array_key_exists('layers', $part)) {
            if (!is_array($part['layers']) || !array_is_list($part['layers'])) {
                throw new \InvalidArgumentException('Layers must be an array');
            }
            $layers = $result['layers'] ?? [];
            foreach ($part['layers'] as $layer) {
                $fields = objectFields($layer);
                $position = null;
                foreach ($layers as $index => $existing) {
                    if ((objectFields($existing)['id'] ?? null) === ($fields['id'] ?? null)) {
                        $position = $index;
                        break;
                    }
                }
                if ($position === null) {
                    $layers[] = copyJson($layer);
                } elseif ($strict) {
                    throw new \InvalidArgumentException('Merge conflict: duplicate layer id');
                } else {
                    $layers[$position] = copyJson($layer);
                }
            }
            $result['layers'] = $layers;
        }
    }
    return $result;
}

function nestAtPath(string $path, mixed $value): array
{
    if (!preg_match('/\Adata(?:\.[A-Za-z_][A-Za-z0-9_]*)+\z/', $path)) {
        throw new \InvalidArgumentException('Expected a path of the form data.name(.name)*');
    }
    foreach (array_reverse(array_slice(explode('.', $path), 1)) as $key) {
        $value = [$key => $value];
    }
    return $value;
}

function layerPath(string $id): string
{
    $name = substr(layerId($id), 7);
    $out = 'data.layer_';
    foreach (preg_split('//u', $name, -1, PREG_SPLIT_NO_EMPTY) as $char) {
        if (preg_match('/^[A-Za-z0-9]$/', $char)) { $out .= $char; }
        else {
            $bytes = array_values(unpack('C*', $char));
            $code = $bytes[0];
            if ($code >= 240) { $code = (($code & 7) << 18) | (($bytes[1] & 63) << 12) | (($bytes[2] & 63) << 6) | ($bytes[3] & 63); }
            elseif ($code >= 224) { $code = (($code & 15) << 12) | (($bytes[1] & 63) << 6) | ($bytes[2] & 63); }
            elseif ($code >= 192) { $code = (($code & 31) << 6) | ($bytes[1] & 63); }
            $out .= '_' . dechex($code) . '_';
        }
    }
    return $out;
}

function place(array|\stdClass $part, array|\stdClass $options): array
{
    $part = objectFields($part);
    $options = objectFields($options);
    $frame = $options['frame'];
    $node = ['frame' => $frame instanceof Box ? $frame->toFrame() : copyJson($frame), 'content' => copyJson($part['content']), 'style' => new \stdClass()];
    foreach (['zIndex', 'rotation', 'transform'] as $key) {
        if (($options[$key] ?? null) !== null) { $node[$key] = copyJson($options[$key]); }
    }
    return ['layers' => [['id' => layerId($options['id']), 'type' => 'matrix', 'dataPath' => $options['path']]], 'data' => nestAtPath($options['path'], $node)];
}

function emptyRoot(): array
{
    return ['pixels' => [[['empty' => true]]], 'default' => ['empty' => true],
        'layout' => ['gapMode' => 'default', 'edgeInsetRatio' => 0, 'cellAspectRatio' => 1]];
}

function toResponse(array|\stdClass $part): array
{
    $response = merge($part);
    if (!array_key_exists('pixels', $response)) {
        $response = merge(emptyRoot(), $response);
    }
    validate($response);
    return $response;
}
