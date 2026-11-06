<?php
declare(strict_types=1);
namespace GlanceWidgets;

/** ECMAScript decimal formatting using the exact IEEE double and decimal integer arithmetic. */
final class Numbers
{
    public static function text(float|int $value): string
    {
        $value = (float)$value;
        if (is_nan($value)) {
            return 'NaN';
        }
        if (!is_finite($value)) {
            return $value < 0 ? '-Infinity' : 'Infinity';
        }
        if ($value == 0) {
            return '0';
        }
        $precision = ini_get('serialize_precision');
        try {
            ini_set('serialize_precision', '-1');
            $raw = strtolower(json_encode(abs($value), JSON_THROW_ON_ERROR));
        } finally {
            ini_set('serialize_precision', $precision);
        }
        $parts = explode('e', $raw);
        $mantissa = $parts[0];
        $decimal = strpos($mantissa, '.');
        $point = ($decimal === false ? strlen($mantissa) : $decimal) + (int)($parts[1] ?? 0);
        $digits = str_replace('.', '', $mantissa);
        while ($digits[0] === '0') {
            $digits = substr($digits, 1);
            $point--;
        }
        $digits = rtrim($digits, '0');
        $power = $point - 1;
        if ($power < -6 || $power >= 21) {
            $result = $digits[0] . (strlen($digits) > 1 ? '.' . substr($digits, 1) : '');
            $result .= 'e' . ($power >= 0 ? '+' : '') . $power;
        } elseif ($point <= 0) {
            $result = '0.' . str_repeat('0', -$point) . $digits;
        } elseif ($point >= strlen($digits)) {
            $result = $digits . str_repeat('0', $point - strlen($digits));
        } else {
            $result = substr($digits, 0, $point) . '.' . substr($digits, $point);
        }
        return ($value < 0 ? '-' : '') . $result;
    }

    public static function fixed(float|int $value, int|float $decimals): string
    {
        $decimals = integerValue($decimals, 'Decimals');
        if ($decimals < 0 || $decimals > 100) {
            throw new \InvalidArgumentException('decimals must be 0..100');
        }
        $value = (float)$value;
        if (!is_finite($value) || abs($value) >= 1e21) {
            return self::text($value);
        }
        $bits = unpack('Nhigh/Nlow', pack('E', abs($value)));
        $exponent = ($bits['high'] >> 20) & 2047;
        $mantissa = ($bits['high'] & 1048575) * 4294967296 + $bits['low'];
        $mantissa += $exponent ? 4503599627370496 : 0;
        $power = ($exponent ? $exponent - 1023 - 52 : -1074) + $decimals;
        $digits = (string)$mantissa;
        for ($index = 0; $index < $decimals; $index++) {
            $digits = self::multiply($digits, 5);
        }
        if ($power >= 0) {
            for ($index = 0; $index < $power; $index++) {
                $digits = self::multiply($digits, 2);
            }
        } else {
            $remainder = 0;
            for ($index = 0; $index < -$power; $index++) {
                [$digits, $remainder] = self::halve($digits);
            }
            if ($remainder) {
                $digits = self::increment($digits);
            }
        }
        $digits = str_pad($digits, $decimals + 1, '0', STR_PAD_LEFT);
        $result = $decimals ? substr($digits, 0, -$decimals) . '.' . substr($digits, -$decimals) : $digits;
        return ($value < 0 ? '-' : '') . $result;
    }

    public static function label(float|int $value, int|float $decimals): string
    {
        $result = self::fixed($value, $decimals);
        return (float)$result == 0 ? self::fixed(0, $decimals) : $result;
    }

    private static function multiply(string $digits, int $factor): string
    {
        $result = '';
        $carry = 0;
        for ($index = strlen($digits) - 1; $index >= 0; $index--) {
            $number = (int)$digits[$index] * $factor + $carry;
            $result = ($number % 10) . $result;
            $carry = intdiv($number, 10);
        }
        return ($carry ? (string)$carry : '') . $result;
    }

    private static function halve(string $digits): array
    {
        $result = '';
        $carry = 0;
        for ($index = 0; $index < strlen($digits); $index++) {
            $number = $carry * 10 + (int)$digits[$index];
            $result .= intdiv($number, 2);
            $carry = $number % 2;
        }
        return [ltrim($result, '0') ?: '0', $carry];
    }

    private static function increment(string $digits): string
    {
        $index = strlen($digits) - 1;
        while ($index >= 0 && $digits[$index] === '9') {
            $digits[$index--] = '0';
        }
        if ($index < 0) {
            return '1' . $digits;
        }
        $digits[$index] = (string)((int)$digits[$index] + 1);
        return $digits;
    }
}
