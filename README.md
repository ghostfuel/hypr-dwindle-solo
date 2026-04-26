# hypr-dwindle-solo

A Hyprland plugin that resizes and aligns solo (single tiled) windows on a workspace instead of letting them fill the entire monitor.

## Features

- Configurable width and height as a fraction of the work area
- Horizontal alignment: left, center, or right
- Resizing works naturally — anchored edges stay fixed, free edges move
- Per-workspace enable/disable via workspace ID list

## Configuration

```ini
plugin {
    dwindle-solo {
        solo_width = 0.65           # 0.1–1.0, fraction of work area width
        solo_height = 1.0           # 0.1–1.0, fraction of work area height
        solo_align = 0              # 0 = center, 1 = left, 2 = right
        enabled_workspaces =        # comma-separated workspace IDs, empty = all
    }
}
```

## Building

Requires Hyprland headers (`hyprland-headers` or equivalent for your distro).

```sh
make
```

## Loading

```sh
hyprctl plugin load /path/to/hypr-dwindle-solo.so
```

## License

MIT
