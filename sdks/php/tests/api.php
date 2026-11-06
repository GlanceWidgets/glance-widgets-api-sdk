<?php
declare(strict_types=1);
require_once __DIR__ . '/golden.php';

use GlanceWidgets\Box;
use GlanceWidgets\GlanceValidationError;
use GlanceWidgets\Matrix;
use GlanceWidgets\Numbers;

$assertions = 0;

function check(bool $condition, string $message): void
{
    global $assertions;
    $assertions++;
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

function raises(callable $operation, string $class = GlanceValidationError::class): Throwable
{
    global $assertions;
    $assertions++;
    try {
        $operation();
    } catch (Throwable $exception) {
        if ($exception instanceof $class) {
            return $exception;
        }
        throw $exception;
    }
    throw new RuntimeException('Expected ' . $class);
}

$canvas = ['width' => 338, 'height' => 158];
$frame = Box::canvas($canvas)->sub(.1, .75, .8, .15)->toFrame();
$root = GlanceWidgets\pixelTextRoot('12:30', ['canvas' => $canvas, 'theme' => 'led']);
$bar = GlanceWidgets\pixelProgressBar(65, ['canvas' => $canvas, 'frame' => $frame]);
$response = GlanceWidgets\toResponse(GlanceWidgets\merge($root,
    GlanceWidgets\place($bar, ['id' => 'progress', 'frame' => $frame, 'path' => 'data.progress'])));
check(GlanceWidgets\validate(json_decode(json_encode($response))) === null, 'Composed response validates');

$roundingCases = [[2.5, 0, '3'], [-2.5, 0, '-3'], [.125, 2, '0.13'], [2.55, 1, '2.5'],
    [1.005, 2, '1.00'], [-.01, 1, '0.0']];
foreach ($roundingCases as [$value, $decimals, $expected]) {
    check(Numbers::label($value, $decimals) === $expected, 'Exact-double decimal rounding');
}
$response = GlanceWidgets\toResponse(new stdClass());
$response['pixels'][0][0] = ['opacity' => true];
$error = raises(static fn() => GlanceWidgets\validate($response));
check($error->path === '$.pixels[0][0].opacity', 'Validation error path');
foreach ([NAN, INF, 9.2e18] as $value) {
    raises(static fn() => GlanceWidgets\validate(GlanceWidgets\emptyRoot() + ['data' => ['value' => $value]]));
}
raises(static fn() => GlanceWidgets\validate(GlanceWidgets\emptyRoot()
    + ['data' => ['large' => str_repeat('字', 180000)]]));

$first = ['data' => ['x' => ['one' => 1]]];
$second = ['data' => ['x' => ['two' => [2]]]];
$third = ['data' => ['x' => ['one' => 3]]];
check(GlanceWidgets\merge(GlanceWidgets\merge($first, $second), $third)
    === GlanceWidgets\merge($first, GlanceWidgets\merge($second, $third)), 'Merge associativity');
$merged = GlanceWidgets\merge($first, $second, $third);
$merged['data']['x']['two'][] = 4;
check($second['data']['x']['two'] === [2], 'Merge purity');
$error = raises(static fn() => GlanceWidgets\merge($first, $third, ['onConflict' => 'error']),
    InvalidArgumentException::class);
check(str_contains($error->getMessage(), 'data.x.one'), 'Merge conflict path');

$font = GlanceWidgets\defineFont(['name' => 'test-unicode', 'height' => 1, 'glyphs' => ['😀' => ['#']]]);
check($font->hasGlyph('😀'), 'Non-BMP glyph');
check(GlanceWidgets\measureText('😀😀', $font)['width'] === 3, 'Codepoint iteration');
check(GlanceWidgets\textMatrix('😀', ['font' => $font])->get(0, 0)['color'] === '#FFFFFF', 'Registered font');
$timestamp = '1700000000000';
$secret = '密钥🔑';
$request = ['timestamp' => $timestamp, 'signature' => GlanceWidgets\sign($timestamp, $secret)];
check(GlanceWidgets\verifySignature($request, $secret, ['now' => 1700000000000]), 'Unicode HMAC');
check(!GlanceWidgets\verifySignature($request, 'wrong', ['now' => 1700000000000]), 'Wrong secret');
check(!GlanceWidgets\verifySignature(['timestamp' => '١٧', 'signature' => 'x'], $secret), 'ASCII timestamp');

$matrix = (new Matrix(2, 1))->set(0, 0, '#FFF');
$flipped = $matrix->flipX();
$flipped->set(1, 0, '#000');
check($matrix->get(0, 0)['color'] === '#FFFFFF', 'Transform independence');
check((new Matrix(1, 1, ['default' => new stdClass()]))->toRoot()['default'] === GlanceWidgets\DEFAULT_PIXEL_BASE,
    'Empty default object is visible');
$numericCases = [[1e21, '1e+21'], [1.2345678901234567e21, '1.2345678901234568e+21'],
    [1e20, '100000000000000000000'], [1e-7, '1e-7'], [1e-6, '0.000001'], [-0.0, '0']];
foreach ($numericCases as [$value, $expected]) {
    check(GlanceWidgets\sign($value, 'secret') === GlanceWidgets\sign($expected, 'secret'), 'Numeric timestamp');
    if (abs($value) >= 1e21) {
        $card = GlanceWidgets\stat($value, ['canvas' => $canvas, 'decimals' => 3]);
        check(GlanceWidgets\resolveDataPath($card['data'], $card['layers'][0]['dataPath'])['content']['text'] === $expected, 'Large numeric label');
    }
}
$previousPrecision = ini_get('serialize_precision');
try {
    ini_set('serialize_precision', '4');
    check(GlanceWidgets\sign(1.2345678901234567e21, 'secret')
        === GlanceWidgets\sign('1.2345678901234568e+21', 'secret'), 'Shortest digits ignore php.ini precision');
    check(ini_get('serialize_precision') === '4', 'php.ini precision restored');
} finally {
    ini_set('serialize_precision', $previousPrecision);
}
check(json_encode(GlanceWidgets\shape(['id' => 'shape', 'frame' => new stdClass()])) !== false, 'JSON objects');
check(count(GlanceWidgets\iconNames()) === 5287, 'All icons bundled');
check(GlanceWidgets\FONT_3X5 instanceof GlanceWidgets\PixelFont, 'Font constant');
check(isset(GlanceWidgets\THEMES['led']), 'Theme constant');
$part = (new Matrix(2, 2))->toPart();
$bound = GlanceWidgets\toResponse(GlanceWidgets\place($part,
    ['id' => 'grid', 'frame' => ['width'=>1,'height'=>1], 'path' => 'data.grid']));
$bound['data']['grid']['content']['batch'] = [['pixel' => new stdClass(), 'coordinates' => [[2, 0]]]];
raises(static fn() => GlanceWidgets\validate($bound));

// A declaration has no nested content binding.
$unresolved = GlanceWidgets\emptyRoot() + ['data' => new stdClass(), 'layers' => [
    ['id' => 'remote:grid', 'type' => 'matrix', 'dataPath' => null,
        'content' => ['dataPath' => 'data.absent']]]];
$error = raises(static fn() => GlanceWidgets\validate($unresolved));
check($error->path === '$.layers[0].content', 'Declaration rejects nested binding');
$invalidKind = GlanceWidgets\emptyRoot() + ['layers' => [['id' => 'remote:bad', 'type' => []]]];
$error = raises(static fn() => GlanceWidgets\validate($invalidKind));
check($error->path === '$.layers[0].type', 'Invalid kind remains a path-aware validation error');
check((new Matrix(1, 1, ['layout' => ['unknown' => 'ignored']]))->toRoot()['layout']
    === ['gapMode' => 'default', 'edgeInsetRatio' => 0, 'cellAspectRatio' => 1], 'Unknown layout keys ignored');
$floatGap = GlanceWidgets\pixelProgressBar(50, ['segmentGap' => 1.0]);
check(count($floatGap['content']['pixels'][0]) === 39, 'Integer-valued floating gap accepted');
raises(static fn() => GlanceWidgets\pixelProgressBar(.5, ['min' => false, 'max' => true]), TypeError::class);
check(GlanceWidgets\percentLabel(1e21) === '1e+23%', 'Large percent uses JS numeric notation');

$nodePart = GlanceWidgets\text(['id'=>'title:😀', 'frame'=>['width'=>1, 'height'=>1], 'text'=>'hello', 'transform'=>['translateX'=>.1]]);
check($nodePart['layers'][0]['dataPath'] === 'data.layer_title_3a__1f600_', 'Deterministic node path');
$nodeResponse = GlanceWidgets\toResponse($nodePart);
check(count($nodeResponse['layers'][0]) === 3, 'Declaration contains exactly three fields');
$node = GlanceWidgets\resolveDataPath($nodeResponse['data'], $nodePart['layers'][0]['dataPath']);
check($node['content'] === ['text'=>'hello'], 'One node contains layer content');
GlanceWidgets\validateLayerNode(['content'=>['text'=>42], 'style'=>['textOpacity'=>0]], 'text');
GlanceWidgets\validateLayerNode(['content'=>['default'=>['color'=>'#FFFFFF']]], 'matrix');
raises(static fn()=>GlanceWidgets\validateLayerNode(['content'=>['batch'=>[]]], 'matrix'));
raises(static fn()=>GlanceWidgets\validateLayerNode(['frame'=>['width'=>1]], 'text'));
raises(static fn()=>GlanceWidgets\validateLayerNode(['content'=>['sourceMode'=>'api']], 'text'));
$nodeResponse['layers'][] = $nodeResponse['layers'][0];
raises(static fn()=>GlanceWidgets\validate($nodeResponse));

$firstLayer = GlanceWidgets\text(['id'=>'label','frame'=>['width'=>1,'height'=>1],'text'=>'first']);
$changedLayer = GlanceWidgets\text(['id'=>'label','frame'=>['width'=>1,'height'=>1],'text'=>'changed']);
$lastLayer = GlanceWidgets\text(['id'=>'last','frame'=>['width'=>1,'height'=>1]]);
$composedLayers = GlanceWidgets\merge($firstLayer,$lastLayer,$changedLayer);
check(array_column($composedLayers['layers'], 'id') === ['remote:label','remote:last'], 'Merge replaces id in place');
check(GlanceWidgets\resolveDataPath($composedLayers['data'],$firstLayer['layers'][0]['dataPath'])['content']['text'] === 'changed', 'Merge installs latest node');
$composedLayers['data']['layer_label']['content']['text'] = 'mutated';
check(GlanceWidgets\resolveDataPath($changedLayer['data'], 'data.layer_label')['content']['text'] === 'changed', 'Merge copies nodes');
raises(static fn()=>GlanceWidgets\validateLayerNode(['style'=>['gradientColors'=>[]]], 'shape'));
echo "PHP native API: $assertions assertions passed\n";
