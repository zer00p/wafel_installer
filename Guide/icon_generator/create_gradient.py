from PIL import Image

size = 128
img = Image.new('RGBA', (size, size), (0, 0, 0, 0))

stops = [
    (15, (251, 234, 5)),    # FBEA05
    (35, (250, 202, 2)),    # FACA02
    (45, (246, 168, 1)),    # F6A801
    (55, (241, 132, 3)),    # F18403
    (65, (236, 99, 2)),     # EC6302
    (75, (228, 77, 18)),    # E44D12
    (85, (221, 37, 0)),     # DD2500
    (95, (203, 7, 0)),      # CB0700
]

def get_color(x, y):
    diag_pos = (x + y) / 2
    if diag_pos <= stops[0][0]:
        return stops[0][1]
    if diag_pos >= stops[-1][0]:
        return stops[-1][1]
    for i in range(len(stops) - 1):
        p1, c1 = stops[i]
        p2, c2 = stops[i+1]
        if p1 <= diag_pos <= p2:
            ratio = (diag_pos - p1) / (p2 - p1)
            r = int(c1[0] + (c2[0] - c1[0]) * ratio)
            g = int(c1[1] + (c2[1] - c1[1]) * ratio)
            b = int(c1[2] + (c2[2] - c1[2]) * ratio)
            return (r, g, b, 255)
    return (0, 0, 0, 0)

for y in range(size):
    for x in range(size):
        img.putpixel((x, y), get_color(x, y))

img.save('Guide/assets/grad.png')
