package io.grokwatch.app;

import java.util.Arrays;

public final class WatchBgPixelsTest {
    private static int checks;
    private static void check(boolean condition, String message) {
        checks++;
        if (!condition) throw new AssertionError(message);
    }
    private static int pixel(byte[] bytes, int x, int y) {
        int i = 2 * (y * WatchBgPixels.SIZE + x);
        return (bytes[i] & 255) | ((bytes[i + 1] & 255) << 8);
    }
    public static void main(String[] args) {
        int[] src = new int[WatchBgPixels.SIZE * WatchBgPixels.SIZE];
        int[] colors = {0xff000000, 0xffffffff, 0xffff0000, 0xff00ff00, 0xff0000ff, 0x00ffffff};
        int[] expected = {0, 0xffff, 0xf800, 0x07e0, 0x001f, 0};
        for (int c = 0; c < colors.length; c++) {
            Arrays.fill(src, colors[c]);
            byte[] out = WatchBgPixels.encode(src);
            check(out.length == 466 * 466 * 2, "wire size");
            for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
                check(pixel(out, x, y) == expected[c], "primary/endpoint/alpha/byte order");
        }
        // Every 8-bit grey: block averages must retain source brightness, including shadows.
        for (int v = 0; v <= 255; v++) {
            Arrays.fill(src, 0xff000000 | (v << 16) | (v << 8) | v);
            byte[] out = WatchBgPixels.encode(src);
            double red = 0, green = 0, blue = 0;
            for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                int p = pixel(out, x, y);
                red += (p >>> 11) * 255.0 / 31;
                green += ((p >>> 5) & 63) * 255.0 / 63;
                blue += (p & 31) * 255.0 / 31;
            }
            check(Math.abs(red / 16 - v) < 0.6 && Math.abs(green / 16 - v) < 0.6
                    && Math.abs(blue / 16 - v) < 0.6, "grey average: " + v);
        }
        // Native-size high-frequency detail must not be blurred or resampled by packing.
        for (int y = 0; y < 466; y++) for (int x = 0; x < 466; x++)
            src[y * 466 + x] = ((x + y) & 1) == 0 ? 0xff000000 : 0xffffffff;
        byte[] out = WatchBgPixels.encode(src);
        boolean exact = true;
        for (int y = 0; y < 466; y++) for (int x = 0; x < 466; x++)
            exact &= pixel(out, x, y) == (((x + y) & 1) == 0 ? 0 : 0xffff);
        check(exact, "one-pixel checkerboard");
        Arrays.fill(src, 0x80ffffff);
        byte[] alpha = WatchBgPixels.encode(src);
        Arrays.fill(src, 0xff808080);
        check(Arrays.equals(alpha, WatchBgPixels.encode(src)), "alpha over black");
        boolean rejected = false;
        try { WatchBgPixels.encode(new int[1]); } catch (IllegalArgumentException expectedError) { rejected = true; }
        check(rejected, "invalid dimensions");
        System.out.println("WatchBgPixels " + checks + " checks passed");
    }
}
