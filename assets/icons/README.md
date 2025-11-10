# Default Icons

PNG icons for map markers. Recommended size: 32×32 or 64×64 pixels.

## Included Icons

The app ships with 4 essential icons:

- **`bench.png`** - Save points / rest areas
- **`chest.png`** - Treasure / item pickups
- **`skull.png`** - Boss encounters / major enemies
- **`dot.png`** - Generic marker (auto-generated circle)

## Adding More Icons

### App-Wide Icons
Place PNG (or SVG if built with `-DUSE_SVG_ICONS=ON`) files in this directory:
```
assets/icons/your_icon.png
```

These will be available in all projects.

### Per-Project Icons
Projects can include custom icons in their `.cart` file under `/icons/`:
```
my_map.cart
  ├── /icons/
  │   ├── custom_item.png
  │   └── special_marker.png
  └── ...
```

These icons are bundled with the project and travel with the `.cart` file.

## Icon Requirements

- **Format**: PNG (always supported), SVG (requires compile flag)
- **Size**: Any size works, but 32×32 or 64×64 recommended for clarity
- **Transparency**: Alpha channel supported
- **Naming**: Use lowercase with underscores (e.g., `save_point.png`)

## Free Icon Resources

- **Kenney Game Icons**: https://kenney.nl/assets/game-icons (CC0 Public Domain)
- **Game-icons.net**: https://game-icons.net/ (CC BY 3.0)
- **OpenGameArt**: https://opengameart.org/

