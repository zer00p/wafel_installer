from PIL import Image

img = Image.open('Guide/assets/wafel_installer-icon.png').convert('RGBA')

stops = []
for i in range(15, 114, 10):
    r_sum, g_sum, b_sum, count = 0, 0, 0, 0
    for dx in [-2, -1, 0, 1, 2]:
        for dy in [-2, -1, 0, 1, 2]:
            x = i + dx
            y = i + dy
            if 0 <= x < img.width and 0 <= y < img.height:
                r, g, b, a = img.getpixel((x, y))
                if a > 50:
                    r_sum += r
                    g_sum += g
                    b_sum += b
                    count += 1
    if count > 0:
        avg_r = r_sum // count
        avg_g = g_sum // count
        avg_b = b_sum // count
        stops.append(f"{i},{i} #{avg_r:02X}{avg_g:02X}{avg_b:02X}")

print(" ".join(stops))
