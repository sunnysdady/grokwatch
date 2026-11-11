package io.grokwatch.app;

/** Loss-limited sRGB ARGB to RGB565 LE conversion, independent of Android APIs. */
public final class WatchBgPixels {
    public static final int SIZE = 466;
    private static final int[] BAYER = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

    private WatchBgPixels() {}

    public static byte[] encode(int[] pixels) {
        if (pixels.length != SIZE * SIZE) throw new IllegalArgumentException("Expected 466x466 pixels");
        byte[] out = new byte[pixels.length * 2];
        for (int i = 0; i < pixels.length; i++) {
            int c = pixels[i], alpha = c >>> 24;
            // Match the black watch-face background for transparent source pixels.
            int r = (((c >>> 16) & 255) * alpha + 127) / 255;
            int g = (((c >>> 8) & 255) * alpha + 127) / 255;
            int b = ((c & 255) * alpha + 127) / 255;
            int threshold = BAYER[((i / SIZE) & 3) * 4 + ((i % SIZE) & 3)];
            int v = (quantize(r, 31, threshold) << 11)
                    | (quantize(g, 63, threshold) << 5) | quantize(b, 31, threshold);
            out[2 * i] = (byte) v;
            out[2 * i + 1] = (byte) (v >>> 8);
        }
        return out;
    }

    // Ordered dithering preserves average tone between adjacent RGB565 levels.
    // In particular, black and white stay exact; no brightness loss is baked in.
    private static int quantize(int value, int levels, int threshold) {
        return (value * levels * 32 + (threshold * 2 + 1) * 255) / (255 * 32);
    }
}
