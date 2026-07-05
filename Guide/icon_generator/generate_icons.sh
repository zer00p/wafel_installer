#!/bin/bash
cd "$(dirname "$0")"

mkdir -p source_icons

# Download SVGs if missing
if [ ! -f source_icons/rocket.svg ]; then
  wget -q https://raw.githubusercontent.com/twbs/icons/main/icons/rocket-fill.svg -O source_icons/rocket.svg
  wget -q https://raw.githubusercontent.com/twbs/icons/main/icons/floppy-fill.svg -O source_icons/floppy.svg
  wget -q https://raw.githubusercontent.com/twbs/icons/main/icons/disc-fill.svg -O source_icons/disc.svg
  wget -q https://raw.githubusercontent.com/twbs/icons/main/icons/trash3-fill.svg -O source_icons/trash.svg
fi

# Generate 128x128 exact Wafel gradient
python3 create_gradient.py

# Process icons
for icon in rocket floppy disc trash; do
  if [ "$icon" = "disc" ]; then
    SIZE="110x110\!"
  else
    SIZE="96x96\!"
  fi

  # Render SVG, trim, resize, center in 128x128 mask
  magick -background none -density 600 "source_icons/$icon.svg" -trim +repage -resize "$SIZE" \
    -alpha extract -morphology Dilate Disk:1 -background black -gravity center -extent 128x128 mask.png
  
  # Composite gradient over mask
  magick grad.png mask.png -compose CopyOpacity -composite "${icon}_color.png"
  
  # Add drop shadow and lock to 128x128 canvas
  magick -background none "${icon}_color.png" \
    \( +clone -background black -shadow 60x2+0+4 \) +swap -background none -layers merge -gravity center -extent 128x128 "${icon}-icon.png"
done

# Move to assets
mv rocket-icon.png ../assets/getting_started-icon.png
mv floppy-icon.png ../assets/backups-icon.png
mv disc-icon.png ../assets/games-icon.png
mv trash-icon.png ../assets/uninstall-icon.png

# Cleanup temps
rm mask.png grad.png *_color.png
