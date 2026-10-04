"""Verify actual Vulkan screenshots and logs of the basic torus application."""
from pathlib import Path
from PIL import Image, ImageChops

root = Path(__file__).resolve().parents[1]
shots = root / 'report/screenshots'
results = []
for name in ['default', 'side']:
    image = Image.open(shots / f'{name}.ppm').convert('RGB')
    image.save(shots / f'{name}.png')
    assert image.size == (1280, 900), (name, image.size)
    scene = image.crop((300, 130, 1280, 900))
    background = Image.new('RGB', scene.size, image.getpixel((1279, 899)))
    diff = ImageChops.difference(scene, background).convert('L')
    count = sum(1 for p in diff.getdata() if p > 15)
    assert count > 3000, f'{name}: torus was not drawn'
    results.append(f'{name}: 1280x900, {count} visible torus pixels')
assert ImageChops.difference(Image.open(shots/'default.png').crop((300,130,1280,900)),
                            Image.open(shots/'side.png').crop((300,130,1280,900))).getbbox(), 'Camera views are identical'
for name in ['math', 'default', 'side']:
    path = root/'report/checks'/f'{name}.log'
    text = path.read_text()
    assert 'VUID-' not in text and 'SYNC-HAZARD' not in text, f'Vulkan validation failed: {path}'
    if name != 'math':
        assert 'validation: enabled' in text, f'Validation layer missing: {path}'
        assert 'Completed successfully' in text, f'Runtime did not finish: {path}'
        assert 'one descriptor set' in text, f'Unexpected object resources: {path}'
results.append('PASS: basic torus, two camera views, window resize and Vulkan synchronization validation')
text = '\n'.join(results) + '\n'
(root/'report/checks/render.log').write_text(text)
print(text)
