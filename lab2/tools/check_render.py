"""Check actual Vulkan output, including the three Blinn–Phong components."""
from pathlib import Path
from PIL import Image, ImageChops

root = Path(__file__).resolve().parents[1]
checks = root / 'checks'
names = ['default', 'side', 'wide', 'ambient', 'diffuse', 'specular', 'red-light', 'matte']
scenes = {}
results = []
for name in names:
    with Image.open(checks / 'screenshots' / f'{name}.ppm') as source:
        image = source.convert('RGB')
    assert image.size == (1280, 900), (name, image.size)
    image.save(checks / 'screenshots' / f'{name}.png')
    scenes[name] = image.crop((400, 0, 1280, 900))
    log = (checks / f'{name}.log').read_text()
    assert 'VUID-' not in log and 'SYNC-HAZARD' not in log, f'{name}: Vulkan validation error'
    assert 'validation: enabled' in log and 'Completed successfully' in log, f'{name}: incomplete run'
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
results.append(f'PASS: torus, camera pose, FOV ({normal_area} -> {wide_area} pixels), window resize')

ambient_colors = scenes['ambient'].getcolors(maxcolors=100)
assert ambient_colors is not None and len(ambient_colors) == 2, 'Ambient-only surface is not uniform'
assert changed_pixels(scenes['diffuse'], scenes['ambient']) > 3000, 'Diffuse term has no effect'
highlights = changed_pixels(scenes['default'], scenes['matte'])
assert 100 < highlights < normal_area * .5, 'Specular term is missing or is not localized'
red = [p for p in scenes['red-light'].getdata() if p != background]
assert len(red) > 3000 and all(g == 0 and b == 0 for r, g, b in red), 'Light color is ignored'

# The swapchain is sRGB. Decode before verifying ambient + diffuse + specular.
def linear(byte):
    value = byte / 255
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4

samples = 0
for y in range(0, 900, 5):
    for x in range(0, 880, 5):
        point = (x, y)
        if scenes['ambient'].getpixel(point) == background:
            continue
        whole = scenes['default'].getpixel(point)
        parts = [scenes[name].getpixel(point) for name in ['ambient', 'diffuse', 'specular']]
        for channel in range(3):
            expected = min(1, sum(linear(p[channel]) for p in parts))
            assert abs(linear(whole[channel]) - expected) < .025, (point, channel, whole, parts)
        samples += 1
assert samples > 100
results.append(f'PASS: ambient + diffuse + specular ({samples} samples), highlight ({highlights} pixels), light color')
results.append('PASS: all 8 Vulkan runs completed with synchronization validation and one descriptor set')
output = '\n'.join(results) + '\n'
(checks / 'render.log').write_text(output)
print(output)
