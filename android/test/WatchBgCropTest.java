package io.grokwatch.app;

public final class WatchBgCropTest {
    private static int checks;
    private static void check(boolean condition, String message) {
        checks++;
        if (!condition) throw new AssertionError(message);
    }
    public static void main(String[] args) {
        WatchBgCrop landscape = new WatchBgCrop(4000, 3000, 1, 0.5, 0.5);
        check(landscape.left == 500 && landscape.top == 0 && landscape.side == 3000, "landscape center");
        WatchBgCrop portrait = new WatchBgCrop(3000, 4000, 1, 0.5, 0.5);
        check(portrait.left == 0 && portrait.top == 500 && portrait.side == 3000, "portrait center");
        int[][] sizes = {{466,466},{4000,3000},{3000,4000},{101,73},{1,1},{8000,1000}};
        for (int[] size : sizes) for (double zoom : new double[]{1,1.5,2,4})
            for (double x : new double[]{-2,0,0.5,1,3}) for (double y : new double[]{-2,0,0.5,1,3}) {
                WatchBgCrop crop = new WatchBgCrop(size[0],size[1],zoom,x,y);
                check(crop.left >= 0 && crop.top >= 0 && crop.left + crop.side <= size[0] + 1e-8
                        && crop.top + crop.side <= size[1] + 1e-8, "no blank edge");
                WatchBgCrop restored = new WatchBgCrop(size[0],size[1],zoom,crop.centerX,crop.centerY);
                check(Math.abs(restored.left - crop.left) < 1e-8 && Math.abs(restored.top - crop.top) < 1e-8, "selection round trip");
                WatchBgCrop preview = new WatchBgCrop(size[0]*2,size[1]*2,zoom,crop.centerX,crop.centerY);
                check(Math.abs(preview.left/2-crop.left)<1e-8 && Math.abs(preview.top/2-crop.top)<1e-8
                        && preview.side/2 == crop.side, "preview and source select same region");
            }
        for (double zoom : new double[]{0,0.9,4.1,Double.NaN,Double.POSITIVE_INFINITY}) {
            boolean rejected = false;
            try { new WatchBgCrop(466,466,zoom,0.5,0.5); } catch (IllegalArgumentException expected) { rejected = true; }
            check(rejected, "reject invalid zoom");
        }
        System.out.println("WatchBgCrop " + checks + " checks passed");
    }
}
