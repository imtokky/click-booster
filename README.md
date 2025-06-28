# click-booster

設定の変更は以下の部分を編集してください

```cpp
static constexpr int LONG_PRESS_THRESHOLD = 150;
static constexpr int RAPID_CLICK_THRESHOLD = 5;
static constexpr int RAPID_CLICK_WINDOW = 1000;
static constexpr int AUTO_CLICK_INTERVAL = 100;
static constexpr int INACTIVITY_TIMEOUT = 500;
```

- 長押しと判断する秒数
- 何ミリ秒間に何回クリックしたら連打とみなすか *¹
- 連打とみなすクリックを計測する時間 *¹
- 自動クリックする間隔
- 連打をやめたと判断する秒数
