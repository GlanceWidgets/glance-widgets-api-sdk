<?php
declare(strict_types=1);
require_once __DIR__ . '/../src/GlanceWidgets.php';

use GlanceWidgets\Box;
use GlanceWidgets\Matrix;

set_error_handler(static function ($severity, $message, $file, $line): never {
    throw new ErrorException($message, 0, $severity, $file, $line);
});

function buildMatrix(object $specification): Matrix
{
    $options = $specification->options ?? new stdClass();
    if (isset($specification->theme)) {
        $options = GlanceWidgets\themeOptions(GlanceWidgets\getTheme($specification->theme), $options);
    }
    $matrix = new Matrix($specification->columns, $specification->rows, $options);
    foreach ($specification->ops ?? [] as $operation) {
        $name = $operation[0];
        $arguments = array_slice($operation, 1);
        if ($name === 'blit') {
            $arguments[0] = buildMatrix($arguments[0]);
        }
        $result = $matrix->{$name}(...$arguments);
        if (in_array($name, ['flipX', 'flipY', 'rotate90', 'crop'], true)) {
            $matrix = $result;
        }
    }
    return $matrix;
}

function picture(Matrix $matrix): array
{
    $legend = $lines = [];
    $alphabet = '#ABCDEFGHIJKLMNOPQRSTUVWXYZ';
    for ($y = 0; $y < $matrix->rows; $y++) {
        $line = '';
        for ($x = 0; $x < $matrix->columns; $x++) {
            $style = $matrix->get($x, $y);
            if (!$matrix->isSet($x, $y)) {
                $line .= '.';
            } elseif ($style === 'empty') {
                $line .= '_';
            } else {
                $key = json_encode($style, JSON_THROW_ON_ERROR);
                if (!array_key_exists($key, $legend)) {
                    if (count($legend) >= strlen($alphabet)) {
                        throw new RuntimeException('Too many styles for ASCII');
                    }
                    $legend[$key] = $alphabet[count($legend)];
                }
                $line .= $legend[$key];
            }
        }
        $lines[] = $line;
    }
    return $lines;
}

function boxFrame(object $specification): array
{
    $box = Box::canvas($specification->canvas ?? null);
    foreach ($specification->ops ?? [] as $operation) {
        $name = $operation[0];
        $arguments = array_slice($operation, 1);
        $box = in_array($name, ['rows', 'columns'], true)
            ? $box->split($name, $arguments[0], $arguments[1])[$arguments[2]] : $box->{$name}(...$arguments);
    }
    return $box->toFrame();
}

function pixelatePicture(array $arguments): array
{
    $matrix = GlanceWidgets\pixelate(...$arguments);
    $colors = [];
    for ($y = 0; $y < $matrix->rows; $y++) {
        for ($x = 0; $x < $matrix->columns; $x++) {
            $style = $matrix->get($x, $y);
            if ($matrix->isSet($x, $y) && $style !== 'empty' && !in_array($style['color'], $colors, true)) {
                $colors[] = $style['color'];
            }
        }
    }
    return ['picture' => picture($matrix), 'colors' => $colors];
}

function runCall(object $call): mixed
{
    $arguments = $call->args;
    switch ($call->fn) {
        case 'matrix':
            $matrix = buildMatrix($arguments[0]);
            return match ($arguments[0]->export ?? 'root') {
                'ascii' => picture($matrix), 'part' => $matrix->toPart(), default => $matrix->toRoot()
            };
        case 'place':
            return GlanceWidgets\place(buildMatrix($arguments[0])->toPart(), $arguments[1]);
        case 'boxFrame':
            return boxFrame($arguments[0]);
        case 'theme':
            return GlanceWidgets\getTheme(...$arguments);
        case 'textMatrix':
            return picture(GlanceWidgets\textMatrix(...$arguments));
        case 'pixelate':
            return pixelatePicture($arguments);
        default:
            $function = 'GlanceWidgets\\' . $call->fn;
            if (!function_exists($function)) {
                throw new LogicException('Missing SDK API: ' . $function);
            }
            return $function(...$arguments);
    }
}

function difference(mixed $actual, mixed $expected, string $path = '$'): ?string
{
    if ((is_int($actual) || is_float($actual)) && (is_int($expected) || is_float($expected))) {
        return is_finite((float)$actual) && abs($actual - $expected) <= 1e-9 * max(1, abs($expected))
            ? null : "$path: $actual != $expected";
    }
    if (is_array($actual) && is_array($expected)) {
        if (count($actual) !== count($expected)) {
            return $path . ': length mismatch';
        }
        foreach ($actual as $index => $value) {
            $error = difference($value, $expected[$index], $path . '[' . $index . ']');
            if ($error !== null) {
                return $error;
            }
        }
        return null;
    }
    if ($actual instanceof stdClass && $expected instanceof stdClass) {
        $actualKeys = array_keys(get_object_vars($actual));
        $expectedKeys = array_keys(get_object_vars($expected));
        sort($actualKeys);
        sort($expectedKeys);
        if ($actualKeys !== $expectedKeys) {
            return $path . ': keys ' . json_encode($actualKeys) . ' != ' . json_encode($expectedKeys);
        }
        foreach ($actual as $key => $value) {
            $error = difference($value, $expected->{$key}, $path . '.' . $key);
            if ($error !== null) {
                return $error;
            }
        }
        return null;
    }
    return get_debug_type($actual) === get_debug_type($expected) && $actual === $expected ? null
        : $path . ': ' . json_encode($actual) . ' != ' . json_encode($expected);
}

function runGolden(): void
{
    $count = 0;
    $failures = [];
    $files = glob(__DIR__ . '/../../../spec/golden/*.json');
    if (count($files) !== 27) {
        throw new RuntimeException('Expected all 27 golden fixture files');
    }
    foreach ($files as $file) {
        $cases = json_decode(file_get_contents($file), false, 512, JSON_THROW_ON_ERROR);
        foreach ($cases as $case) {
            $count++;
            try {
                $value = runCall($case->call);
                $actual = json_decode(json_encode($value, JSON_THROW_ON_ERROR));
                $error = ($case->throws ?? false) ? 'Expected parameter exception'
                    : difference($actual, $case->expected);
            } catch (Throwable $exception) {
                $expectedError = $exception instanceof InvalidArgumentException
                    || $exception instanceof OutOfBoundsException || $exception instanceof TypeError;
                $error = ($case->throws ?? false) && $expectedError ? null
                    : get_class($exception) . ': ' . $exception->getMessage() . "\n" . $exception->getTraceAsString();
            }
            if ($error !== null) {
                $failures[] = basename($file) . ' / ' . $case->name . ': ' . $error;
            }
        }
    }
    echo implode("\n", $failures) . "\nPHP golden: " . ($count - count($failures)) . "/$count passed\n";
    if ($failures) {
        exit(1);
    }
}

if (realpath($_SERVER['SCRIPT_FILENAME']) === __FILE__) {
    runGolden();
}
