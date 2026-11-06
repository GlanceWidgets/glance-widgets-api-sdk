<?php
declare(strict_types=1);
namespace GlanceWidgets;

const PROTOCOL_VERSION = 1;
const MAX_RESPONSE_BYTES = 524288;
const MAX_MATRIX_CELLS = 4096;
const LAYER_ID_PREFIX = 'remote:';
const MAX_SAFE_NATIVE_INTEGER = 9.2e18;
const CELL_ASPECT_RATIO_MIN = .1;
const CELL_ASPECT_RATIO_MAX = 10;
const EDGE_INSET_RATIO_LIMIT = .45;
const GAP_RATIO_MAX = 10;
const FRAME_SIZE_MIN = .02;
const FRAME_SIZE_MAX = 1.5;
const SHAPE_MIN_SIZE_PT = 24;
const DATA_PATH_PATTERN = '/\Adata(?:\.[A-Za-z_][A-Za-z0-9_]*|\[[0-9]+\])*\z/';

final class GlanceValidationError extends \InvalidArgumentException
{
    public function __construct(public readonly string $path, string $message)
    {
        parent::__construct($path . ': ' . $message);
    }
}

/** Strict protocol validation against the App's published schemas and SDK invariants. */
final class Validator
{
    private array $schemas;

    public function __construct()
    {
        $this->schemas = dataFile('schemas.json');
    }

    private function fail(string $path, string $message): never
    {
        throw new GlanceValidationError($path, $message);
    }

    private function json(mixed $value, string $path): void
    {
        if ($value === null || is_string($value) || is_bool($value)) {
            return;
        }
        if (is_int($value) || is_float($value)) {
            if (!is_finite((float)$value) || abs($value) >= MAX_SAFE_NATIVE_INTEGER) {
                $this->fail($path, 'Number is not safe for the native renderer');
            }
            return;
        }
        if (is_array($value) || $value instanceof \stdClass) {
            foreach ($value as $key => $child) {
                $this->json($child, $path . (is_array($value) && array_is_list($value)
                    ? '[' . $key . ']' : '.' . $key));
            }
            return;
        }
        $this->fail($path, 'Value is not JSON');
    }

    private function object(mixed $value, array $schema, string $path, bool $strict = false): array
    {
        if (!isObject($value)) {
            $this->fail($path, 'Expected object');
        }
        $fields = objectFields($value);
        if ($strict) { foreach ($fields as $key => $_) { if (!array_key_exists($key, $schema)) { $this->fail($path . '.' . $key, 'Unsupported field'); } } }
        foreach ($schema as $key => $rule) {
            if (array_key_exists($key, $fields) && $fields[$key] !== null) {
                $this->rule($fields[$key], $rule, $path . '.' . $key);
            }
        }
        return $fields;
    }

    private function rule(mixed $value, mixed $rule, string $path): void
    {
        if (is_string($rule)) {
            $number = is_int($value) || is_float($value);
            $valid = match ($rule) {
                'number' => $number,
                'textValue' => $number || is_string($value),
                'integer' => $number && floor($value) == $value,
                'boolean' => is_bool($value), 'string' => is_string($value), 'color' => isAppColor($value)
            };
            if (!$valid) {
                $this->fail($path, 'Expected ' . $rule);
            }
            return;
        }
        if (isset($rule['enum'])) {
            if (!is_string($value) || !in_array($value, $rule['enum'], true)) {
                $this->fail($path, 'Invalid enum value');
            }
        } elseif (isset($rule['arrayOf'])) {
            if (!is_array($value) || !array_is_list($value)
                || (isset($rule['length']) && count($value) !== $rule['length'])) {
                $this->fail($path, 'Invalid array');
            }
            foreach ($value as $index => $child) {
                $this->rule($child, $rule['arrayOf'], $path . '[' . $index . ']');
            }
        } elseif (isset($rule['object'])) {
            $this->object($value, $rule['object'], $path);
        } else {
            $fields = $this->object($value, [], $path);
            foreach ($fields as $key => $child) {
                $this->rule($child, $rule['record'], $path . '.' . $key);
            }
        }
    }

    private function matrix(array $matrix, string $path): void
    {
        $pixels = $matrix['pixels'] ?? null;
        if (!is_array($pixels) || !array_is_list($pixels) || !$pixels
            || !is_array($pixels[0]) || !array_is_list($pixels[0]) || !$pixels[0]) {
            $this->fail($path . '.pixels', 'Expected a nonempty 2D array');
        }
        $rows = count($pixels);
        $columns = count($pixels[0]);
        if ($rows * $columns > MAX_MATRIX_CELLS) {
            $this->fail($path . '.pixels', 'Matrix exceeds 4096 cells');
        }
        foreach ($pixels as $rowIndex => $row) {
            $rowPath = $path . '.pixels[' . $rowIndex . ']';
            if (!is_array($row) || !array_is_list($row) || count($row) !== $columns) {
                $this->fail($rowPath, 'Inconsistent row width');
            }
            foreach ($row as $column => $pixel) {
                if ($pixel !== null) {
                    $this->object($pixel, $this->schemas['pixel'], $rowPath . '[' . $column . ']');
                }
            }
        }
        foreach (['default' => 'pixel', 'layout' => 'layout'] as $key => $schema) {
            if (array_key_exists($key, $matrix)) {
                $this->object($matrix[$key], $this->schemas[$schema], $path . '.' . $key);
            }
        }
        if (array_key_exists('batch', $matrix)) {
            $batch = $matrix['batch'];
            if (!is_array($batch) || !array_is_list($batch)) {
                $this->fail($path . '.batch', 'Expected array');
            }
            foreach ($batch as $index => $entry) {
                $entryPath = $path . '.batch[' . $index . ']';
                $entry = $this->object($entry, ['pixel' => ['object' => $this->schemas['pixel']]], $entryPath);
                $coordinates = $entry['coordinates'] ?? null;
                if (!is_array($coordinates) || !array_is_list($coordinates)) {
                    $this->fail($entryPath . '.coordinates', 'Expected coordinate array');
                }
                foreach ($coordinates as $coordinateIndex => $pair) {
                    $pairPath = $entryPath . '.coordinates[' . $coordinateIndex . ']';
                    $this->rule($pair, ['arrayOf' => 'integer', 'length' => 2], $pairPath);
                    if ($pair[0] < 0 || $pair[0] >= $rows || $pair[1] < 0 || $pair[1] >= $columns) {
                        $this->fail($pairPath, 'Coordinates outside matrix');
                    }
                }
            }
        }
    }

    public function validate(mixed $response): void
    {
        $response = $this->object($response, [], '$');
        $this->json($response, '$');
        $encoded = json_encode($response, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES
            | JSON_UNESCAPED_LINE_TERMINATORS | JSON_THROW_ON_ERROR);
        if (strlen($encoded) > MAX_RESPONSE_BYTES) {
            $this->fail('$', 'Response exceeds 512 KB');
        }
        if (array_key_exists('protocol', $response)
            && ((!is_int($response['protocol']) && !is_float($response['protocol'])) || $response['protocol'] != 1)) {
            $this->fail('$.protocol', 'Must be the number 1');
        }
        $this->matrix($response, '$');
        $data = $response['data'] ?? new \stdClass();
        if (!isObject($data)) {
            $this->fail('$.data', 'Expected object (a PHP array with sequential integer-like keys is a list; pass such data as stdClass)');
        }
        if (!array_key_exists('layers', $response)) {
            return;
        }
        $layers = $response['layers'];
        if (!is_array($layers) || !array_is_list($layers)) {
            $this->fail('$.layers', 'Expected array');
        }
        $seen = [];
        foreach ($layers as $index => $value) {
            $path = '$.layers[' . $index . ']';
            $layer = $this->object($value, ['id' => 'string', 'type' => 'string', 'dataPath' => 'string'], $path, true);
            if (!is_string($layer['id'] ?? null) || !str_starts_with($layer['id'], LAYER_ID_PREFIX) || strlen($layer['id']) <= 7 || isset($seen[$layer['id']])) {
                $this->fail($path . '.id', 'Must be unique remote: id');
            }
            $seen[$layer['id']] = true;
            $kind = $layer['type'] ?? null;
            $schema = is_string($kind) ? ($this->schemas['layers'][$kind] ?? null) : null;
            if ($schema === null) { $this->fail($path . '.type', 'Unknown layer type'); }
            $binding = $layer['dataPath'] ?? null;
            if (!is_string($binding) || !preg_match(DATA_PATH_PATTERN, $binding)) { $this->fail($path . '.dataPath', 'Invalid dataPath'); }
            $this->node(resolveDataPath($data, $binding), $schema, $kind, $binding, true);
        }
    }
    public function validateLayerNode(mixed $node, string $type, string $path = '$', bool $requiresFrame = false): void
    {
        $this->json($node, $path);
        $schema = $this->schemas['layers'][$type] ?? null;
        if ($schema === null) { $this->fail($path, 'Unknown layer type'); }
        $this->node($node, $schema, $type, $path, $requiresFrame);
    }

    private function node(mixed $value, array $schema, string $kind, string $path, bool $requiresFrame): void
    {
        $node = $this->object($value, [], $path);
        foreach ($node as $key => $_) {
            if (!in_array($key, ['frame','zIndex','rotation','transform','content','style'], true)) { $this->fail($path . '.' . $key, 'Unsupported field'); }
        }
        $frame = $node['frame'] ?? null;
        if (($frame !== null || $requiresFrame) && (!isObject($frame) || !isset(objectFields($frame)['width'], objectFields($frame)['height']))) { $this->fail($path . '.frame', 'Remote frame needs width and height'); }
        if ($frame !== null) { $this->object($frame, $this->schemas['frame'], $path . '.frame', true); }
        foreach (['zIndex','rotation'] as $key) { if (isset($node[$key])) { $this->rule($node[$key], $key === 'zIndex' ? 'integer' : 'number', $path . '.' . $key); } }
        if (isset($node['transform'])) { $this->object($node['transform'], $this->schemas['transform'], $path . '.transform', true); }
        $content = $node['content'] ?? new \stdClass();
        if ($kind === 'matrix') {
            $fields = $this->object($content, [], $path . '.content');
            foreach ($fields as $key => $_) { if (!in_array($key, ['pixels','default','batch','layout'], true)) { $this->fail($path . '.content.' . $key, 'Unsupported field'); } }
            $fields = array_filter($fields, static fn($value) => $value !== null);
            if (isset($fields['pixels']) || $requiresFrame) { $this->matrix($fields, $path . '.content'); }
            elseif (isset($fields['batch'])) { $this->fail($path . '.content.batch', 'Batch requires pixels'); }
            elseif (isset($fields['default'])) { $this->matrix(['pixels'=>[[null]], 'default'=>$fields['default']], $path . '.content'); }
            if (isset($fields['layout'])) { $this->object($fields['layout'], $this->schemas['layout'], $path . '.content.layout', true); }
        } else { $this->object($content, $schema['content'], $path . '.content', true); }
        if (isset($node['style'])) { $this->object($node['style'], $schema['style'], $path . '.style', true);
            if ((objectFields($node['style'])['gradientColors'] ?? null) === []) { $this->fail($path . '.style.gradientColors', 'Must contain at least one color'); }
        }
        $content = objectFields($content);
        if (in_array($kind, ['image','icon'], true)) {
            $glyph = isset($content['icon']);
            if (($glyph && isset($content['url'])) || (!$glyph && (isset($content['iconCodePoint']) || isset($content['iconFont'])))) { $this->fail($path . '.content.icon', 'Choose one artwork source'); }
        }
    }

}

function resolveDataPath(mixed $data, string $path): mixed
{
    if (!preg_match(DATA_PATH_PATTERN, $path)) {
        return null;
    }
    preg_match_all('/\.([A-Za-z_][A-Za-z0-9_]*)|\[([0-9]+)\]/', substr($path, 4), $segments,
        PREG_SET_ORDER | PREG_UNMATCHED_AS_NULL);
    foreach ($segments as $segment) {
        if ($segment[1] !== null) {
            if (!isObject($data)) {
                return null;
            }
            $data = objectFields($data)[$segment[1]] ?? null;
        } else {
            $index = (int)$segment[2];
            $data = is_array($data) && array_is_list($data) ? ($data[$index] ?? null) : null;
        }
        if ($data === null) {
            return null;
        }
    }
    return $data;
}

function validate(mixed $response): void
{
    (new Validator())->validate($response);
}

function validateLayerNode(mixed $node, string $type, string $path = '$', bool $requiresFrame = false): void
{
    (new Validator())->validateLayerNode($node, $type, $path, $requiresFrame);
}
