package io.grokwatch.app;

/** Square source region shared by the circular preview and final image decoder. */
public final class WatchBgCrop {
    public final double left, top, side, centerX, centerY;

    public WatchBgCrop(int width, int height, double zoom, double x, double y) {
        if (width <= 0 || height <= 0 || !Double.isFinite(zoom) || !Double.isFinite(x)
                || !Double.isFinite(y) || zoom < 1 || zoom > 4)
            throw new IllegalArgumentException("Invalid crop");
        side = Math.min(width, height) / zoom;
        left = Math.max(0, Math.min(width - side, x * width - side / 2));
        top = Math.max(0, Math.min(height - side, y * height - side / 2));
        centerX = (left + side / 2) / width;
        centerY = (top + side / 2) / height;
    }
}
