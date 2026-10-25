# 0x11 NAV_DATA 向后兼容扩展（App 0.6.2 起）

现状（固件 gw_model.c `case 0x11`）：payload = `dist\0 road\0 dur\0 eta\0`，按 4 个 NUL 结尾字符串读取，之后的字节不读。转向箭头走 0x12（48x48 1bpp，3 段）。

扩展：在 eta 之后可选追加第 5 个字符串 `ext\0`，格式 `k=v;k=v`（ASCII，≤40 字节）。App 仅在有值时追加，并把 road 截到 30 字以保证总长 <230。

| key | 含义 | 例 |
|---|---|---|
| `L` | 红绿灯倒计时，秒（1–300），由无障碍读取高德界面得到 | `L=23` |
| `C` | 红绿灯颜色（可选）：`R` 红 / `G` 绿 / `Y` 黄；缺省按红色显示 | `C=G` |
| `R` | 剩余总距离（预留） | `R=5.1公里` |

兼容性：旧固件只调用 4 次 `cs()`，第 5 段被忽略；`fnv(p,n)` 对整帧求哈希，倒计时变化只会让“内容变化”时间更新，不影响 300 s 静止过期逻辑（反而更准）。

## 固件解析草稿（未合入、未构建；页面视觉需先出效果图）

```c
// gw_model.h: 在 gw_model_t 里加
//   int light_s; int64_t light_t;   // 红绿灯倒计时秒数及收到时间，0=无
// gw_model.c case 0x11 末尾（eta 之后）:
      gw_m.light_s = 0;
      if (i < n) { s = cs(p,n,&i,&l);
        for (int k = 0; k + 1 < l; ) {            // 解析 "k=v;k=v"
          int e = k; while (e < l && s[e] != ';') e++;
          if (s[k] == 'L' && s[k+1] == '=') { int v = atoi(s + k + 2); if (v > 0 && v <= 300) { gw_m.light_s = v; gw_m.light_t = now; } }
          k = e + 1; }
      }
// UI 侧显示剩余 = light_s - (now - light_t)/1e6，<=0 隐藏；超过 3 s 没新帧也按本地递减。
```


## 固件 2.6.11 已合入
解析见 `gw_model.c case 0x11`，`gw_model_light_left()` 本地每秒递减；导航页箭头右侧小胶囊显示（systemRed/Green/Yellow 圆点 + 秒数），无数据隐藏，其他元素位置不变。导航结束清零。
