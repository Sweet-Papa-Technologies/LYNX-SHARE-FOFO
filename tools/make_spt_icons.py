# Fork only (LYNX-SHARE-FOFO): writes the fork's Sweet Papa Studio badge into the icon files that upstream's logo
# tooling generates from its own artwork, so the app, the macOS bundle and the Linux packages show one logo.
# Run after an upstream merge that changed the logo files, then run tools/make_logo_data.py:
#   python3 tools/make_spt_icons.py && python3 tools/make_logo_data.py
# Source: resources/lynx_badge.png (the badge, 1024 px). Not touched: resources/lynx_icon_256.png, LYNX.ico, the MSIX
# assets (already the fork's), resources/brand/*.svg and the website / Store pictures (upstream's, not shipped by the fork).
from PIL import Image

BADGE = "resources/lynx_badge.png"
badge = Image.open(BADGE).convert("RGBA")


def at(size):
    return badge.resize((size, size), Image.LANCZOS)


# the small-size forms tools/make_logo_data.py embeds (upstream draws thicker lines for these; the badge is one picture)
for name in ("lynx-share-logo-small", "lynx-share-logo-tiny", "lynx-share-mark-small"):
    at(1024).save("resources/brand/%s.png" % name, "PNG", optimize=True)

# macOS bundle icon sources, on Apple's icon grid: the tile at 824 of 1024 px
for size in (1024, 256):
    margin = round(size * 100 / 1024)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(at(size - 2 * margin), (margin, margin))
    canvas.save("resources/macos/lynx-icon-%d.png" % size, "PNG", optimize=True)

# Linux hicolor icons
for size in (16, 24, 32, 48, 64, 128, 256, 512):
    at(size).save("resources/linux/icons/hicolor/%dx%d/apps/lynx.png" % (size, size), "PNG", optimize=True)

print("SPT badge written to resources/brand, resources/macos and resources/linux/icons")
