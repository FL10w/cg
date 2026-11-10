"""Check captured Vulkan output: one albedo texture, camera and lighting."""
import argparse
from pathlib import Path
from PIL import Image, ImageChops

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--checks-dir', type=Path, default=root / 'checks')
parser.add_argument('--allow-missing-validation', action='store_true',
                    help='Allow a native smoke check without installed validation layers; report this limitation.')
args = parser.parse_args()
checks = args.checks_dir
names = ['default', 'side', 'wide', 'ambient', 'diffuse', 'red-light', 'matte']
scenes = {}
results = []
all_validated = True
for name in names:
    capture = checks / 'screenshots' / f'{name}.ppm'
    if not capture.exists():
        capture = capture.with_suffix('.png')
    with Image.open(capture) as source:
        image = source.convert('RGB')
    assert image.size == (1280, 900), (name, image.size)
    image.save(checks / 'screenshots' / f'{name}.png')
    scenes[name] = image.crop((400, 0, 1280, 900))
    log = (checks / f'{name}.log').read_text(encoding='utf-8', errors='replace')
    assert 'VUID-' not in log and 'SYNC-HAZARD' not in log, f'{name}: Vulkan validation error'
    validated = 'validation: enabled' in log
    all_validated = all_validated and validated
    assert validated or (args.allow_missing_validation and 'validation: unavailable' in log), f'{name}: validation layers were not enabled'
    assert 'Completed successfully' in log, f'{name}: incomplete run'
    assert 'one descriptor set' in log, f'{name}: unexpected resource layout'

def changed_pixels(a, b, threshold=4):
    return sum(max(pixel) > threshold for pixel in ImageChops.difference(a, b).getdata())

background = scenes['default'].getpixel((879, 899))
blank = Image.new('RGB', scenes['default'].size, background)
normal_area = changed_pixels(scenes['default'], blank)
assert normal_area > 3000, 'Torus was not rendered'
assert changed_pixels(scenes['side'], scenes['default']) > 3000, 'Camera pose has no effect'
wide_area = changed_pixels(scenes['wide'], blank)
assert 1000 < wide_area < normal_area * .5, 'Increasing FOV does not reduce projected object size'
results.append(f'PASS: textured torus, camera pose, FOV ({normal_area} -> {wide_area} pixels), window resize')

# With the directional light disabled, variation comes from the loaded image.
ambient_surface = [p for p in scenes['ambient'].getdata() if max(abs(p[c] - background[c]) for c in range(3)) > 4]
assert len(ambient_surface) > 3000, 'Ambient-only torus was not rendered'
assert len(set(ambient_surface)) > 8, 'Ambient-only torus has no visible texture variation'
assert len({tuple(c // 16 for c in p) for p in ambient_surface}) >= 3, 'Texture colors are indistinguishable'
assert changed_pixels(scenes['diffuse'], scenes['ambient']) > 3000, 'Diffuse term has no effect'
highlights = changed_pixels(scenes['default'], scenes['matte'])
assert 100 < highlights < normal_area * .5, 'Specular term is missing or is not localized'
red = [p for p in scenes['red-light'].getdata() if p != background]
assert len(red) > 3000 and all(g == 0 and b == 0 for r, g, b in red), 'Light color is ignored'

# Matte disables highlights: textured ambient + diffuse still add in linear space.
def linear(byte):
    value = byte / 255
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4

samples = 0
for y in range(0, 900, 5):
    for x in range(0, 880, 5):
        point = (x, y)
        if scenes['ambient'].getpixel(point) == background:
            continue
        whole = scenes['matte'].getpixel(point)
        parts = [scenes[name].getpixel(point) for name in ['ambient', 'diffuse']]
        for channel in range(3):
            expected = min(1, sum(linear(p[channel]) for p in parts))
            assert abs(linear(whole[channel]) - expected) < .025, (point, channel, whole, parts)
        samples += 1
assert samples > 100
results.append(f'PASS: image variation under ambient light, ambient + diffuse ({samples} samples), highlight ({highlights} pixels), light color')
validation = 'validation layers enabled' if all_validated else 'validation layers unavailable (native smoke check only)'
results.append(f'PASS: all 7 Vulkan runs completed with one descriptor set; {validation}')
output = '\n'.join(results) + '\n'
(checks / 'render.log').write_text(output, encoding='utf-8')
print(output)
