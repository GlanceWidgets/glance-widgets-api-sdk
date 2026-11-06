<?php
declare(strict_types=1);
namespace GlanceWidgets;

/** Internal JSON utilities. Empty objects are stdClass; lists remain indexed arrays. */
function objectFields(mixed $value): array
{
    if ($value instanceof \stdClass) {
        return get_object_vars($value);
    }
    if (is_array($value) && ($value === [] || !array_is_list($value))) {
        return $value;
    }
    throw new \InvalidArgumentException('Expected a JSON object');
}

function isObject(mixed $value): bool
{
    return $value instanceof \stdClass || (is_array($value) && !array_is_list($value));
}

function copyJson(mixed $value): mixed
{
    if ($value instanceof \stdClass) {
        $copy = new \stdClass();
        foreach (get_object_vars($value) as $key => $child) {
            $copy->{$key} = copyJson($child);
        }
        return $copy;
    }
    if (is_array($value)) {
        return array_map(__NAMESPACE__ . '\\copyJson', $value);
    }
    return $value;
}

function codepoints(string $value): array
{
    $characters = preg_split('//u', $value, -1, PREG_SPLIT_NO_EMPTY);
    if ($characters === false) {
        throw new \InvalidArgumentException('Text must be valid UTF-8');
    }
    return $characters;
}

function dataFile(string $name): array
{
    return json_decode(file_get_contents(__DIR__ . '/data/' . $name), true, 512, JSON_THROW_ON_ERROR);
}

function finiteNumber(mixed $value, string $name = 'Value'): float|int
{
    if ((!is_int($value) && !is_float($value)) || !is_finite((float)$value)) {
        throw new \InvalidArgumentException($name . ' must be a finite number');
    }
    return $value;
}

function integerValue(mixed $value, string $name = 'Value'): int
{
    finiteNumber($value, $name);
    if (floor($value) != $value || abs($value) >= 9.2233720368547758E18) {
        throw new \InvalidArgumentException($name . ' must be an integer');
    }
    return (int)$value;
}

/** Float to int that never wraps: huge values saturate (callers then hit their size limits); NaN throws. */
function floatToInt(float $value): int
{
    if (is_nan($value)) {
        throw new \InvalidArgumentException('Value must be a finite number');
    }
    return (int)max(-1e15, min(1e15, $value));
}

/** Fill null-valued (unset) options with defaults, like JS `options.key ?? default`. */
function withDefaults(array $options, array $defaults): array
{
    foreach ($defaults as $key => $default) {
        $options[$key] ??= $default;
    }
    return $options;
}

/** Objects always keep object-ness: a list-shaped field array (numeric-string keys) becomes a stdClass. */
function objectValue(array $fields): array|\stdClass
{
    return array_is_list($fields) ? (object)$fields : $fields;
}

/** Numbers in canonical form so 90 and 90.0 (or 0 and -0.0) group together. */
function canonicalNumbers(mixed $value): mixed
{
    if (is_array($value)) {
        return array_map(__NAMESPACE__ . '\\canonicalNumbers', $value);
    }
    return is_float($value) && is_finite($value) && floor($value) == $value && abs($value) < 1e15
        ? (int)$value : $value;
}

function positiveInteger(mixed $value, string $name = 'Value'): int
{
    $value = integerValue($value, $name);
    if ($value < 1) {
        throw new \InvalidArgumentException($name . ' must be positive');
    }
    return $value;
}

function clamp(float|int $value, float|int $minimum, float|int $maximum): float|int
{
    return min($maximum, max($minimum, $value));
}

function jsRound(float|int $value): float|int
{
    $rounded = floor($value + .5);
    return abs($rounded) < PHP_INT_MAX ? (int)$rounded : $rounded;
}
