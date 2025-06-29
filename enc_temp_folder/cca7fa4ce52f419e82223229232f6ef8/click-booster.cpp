#include <iostream>
#include <windows.h>
#include <chrono>
#include <array>
#include <random>
#include <atomic>
#include <mmsystem.h>
using namespace std;
using namespace std::chrono;

#pragma comment(lib, "winmm.lib")

// ブランチ予測ヒント（C++20未満対応）
#ifdef __has_cpp_attribute
#if __has_cpp_attribute(likely)
#define LIKELY [[likely]]
#define UNLIKELY [[unlikely]]
#else
#define LIKELY
#define UNLIKELY
#endif
#else
#define LIKELY
#define UNLIKELY
#endif

class HighPerformanceClickAssist {
private:
    // アトミック変数でロックフリー実装
    atomic<bool> isPressed{ false };
    atomic<bool> wasPressed{ false };
    atomic<bool> longPressDetected{ false };
    atomic<bool> rapidClickMode{ false };
    atomic<bool> shouldExit{ false };

    steady_clock::time_point pressStartTime;
    steady_clock::time_point lastAutoClick;

    // 円形バッファでメモリ効率最適化
    static constexpr size_t BUFFER_SIZE = 8;  // 2の累乗で最適化
    static constexpr size_t BUFFER_MASK = BUFFER_SIZE - 1;
    array<steady_clock::time_point, BUFFER_SIZE> clickBuffer;
    atomic<size_t> bufferHead{ 0 };
    atomic<size_t> bufferTail{ 0 };

    // コンパイル時定数でブランチ予測最適化
    static constexpr int LONG_PRESS_THRESHOLD = 150;
    static constexpr int RAPID_CLICK_THRESHOLD = 5;
    static constexpr int RAPID_CLICK_WINDOW = 1000;
    static constexpr int AUTO_CLICK_INTERVAL = 100;
    static constexpr int INACTIVITY_TIMEOUT = 500;
    static constexpr int MAIN_LOOP_INTERVAL = 1;

    inline void fastClick() noexcept {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));

        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));

        lastAutoClick = steady_clock::now();
    }

    inline void addClickRecord(const steady_clock::time_point& clickTime) noexcept {
        size_t head = bufferHead.load(memory_order_relaxed);
        clickBuffer[head & BUFFER_MASK] = clickTime;
        bufferHead.store(head + 1, memory_order_release);

        size_t tail = bufferTail.load(memory_order_relaxed);
        if ((head - tail) >= BUFFER_SIZE) {
            bufferTail.store(tail + 1, memory_order_release);
        }
    }

    inline bool isRapidClicking() noexcept {
        const auto now = steady_clock::now();
        const auto cutoffTime = now - milliseconds(RAPID_CLICK_WINDOW);

        size_t head = bufferHead.load(memory_order_acquire);
        size_t tail = bufferTail.load(memory_order_acquire);

        int validClicks = 0;

        // 最新のクリックから逆順にチェック（最適化）
        for (size_t i = head; i > tail && validClicks < RAPID_CLICK_THRESHOLD; --i) {
            const auto& clickTime = clickBuffer[(i - 1) & BUFFER_MASK];
            if (clickTime >= cutoffTime) {
                ++validClicks;
            }
            else {
                break;
            }
        }

        return validClicks >= RAPID_CLICK_THRESHOLD;
    }

    inline bool shouldStopRapidMode() noexcept {
        size_t head = bufferHead.load(memory_order_relaxed);
        if (head == 0) return true;

        const auto& lastClickTime = clickBuffer[(head - 1) & BUFFER_MASK];
        const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - lastClickTime).count();

        return elapsed > INACTIVITY_TIMEOUT;
    }

    static uint64_t get_rand_range(uint64_t min_val, uint64_t max_val) {
        static std::mt19937_64 mt64(0);

        std::uniform_int_distribution<uint64_t> get_rand_uni_int(min_val, max_val);

        return get_rand_uni_int(mt64);
    }

public:
    HighPerformanceClickAssist() {
        lastAutoClick = steady_clock::now();
        cout << "- 長押し判定: " << LONG_PRESS_THRESHOLD << "ms" << endl;
        cout << "- 連打判定: " << RAPID_CLICK_WINDOW << "ms以内に" << RAPID_CLICK_THRESHOLD << "回" << endl;
        cout << "- 自動クリック間隔: " << AUTO_CLICK_INTERVAL << "ms" << endl;
        cout << "- レスポンス間隔: " << MAIN_LOOP_INTERVAL << "ms" << endl;
        cout << "- 終了: Ctrl+C\n" << endl;
    }

    void run() {
        timeBeginPeriod(1);

        int sleepCounter = 0;
        constexpr int SLEEP_THRESHOLD = 10;

        while (!shouldExit.load(memory_order_relaxed)) {
            const bool currentPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool prevPressed = wasPressed.load(memory_order_relaxed);

            if (currentPressed == prevPressed) LIKELY{
                if (currentPressed && !longPressDetected.load(memory_order_relaxed)) {
                    const auto elapsed = duration_cast<milliseconds>(
                        steady_clock::now() - pressStartTime).count();

                    if (elapsed >= LONG_PRESS_THRESHOLD) {
                        longPressDetected.store(true, memory_order_relaxed);
                        if (rapidClickMode.load(memory_order_relaxed)) {
                            rapidClickMode.store(false, memory_order_relaxed);
                            cout << "*** 連打モード終了（長押し検知） ***" << endl;
                        }
                    }
                }

            if (rapidClickMode.load(memory_order_relaxed) && !currentPressed) {
                const auto now = steady_clock::now();
                int randomizedDelay = get_rand_range(static_cast<uint64_t>(AUTO_CLICK_INTERVAL) - 50, static_cast<uint64_t>(AUTO_CLICK_INTERVAL) + 50);
                if (duration_cast<milliseconds>(now - lastAutoClick).count() >= randomizedDelay) {
                    fastClick();
                }
            }

            if (rapidClickMode.load(memory_order_relaxed) && shouldStopRapidMode()) {
                rapidClickMode.store(false, memory_order_relaxed);
                cout << "*** 連打モード終了（非アクティブ） ***" << endl;
            }
            }
            else {
                isPressed.store(currentPressed, memory_order_relaxed);

                if (currentPressed && !prevPressed) {
                    pressStartTime = steady_clock::now();
                    longPressDetected.store(false, memory_order_relaxed);
                    addClickRecord(pressStartTime);

                }
                else if (!currentPressed && prevPressed) {
                    if (!longPressDetected.load(memory_order_relaxed)) {
                        if (isRapidClicking()) {
                            if (!rapidClickMode.load(memory_order_relaxed)) {
                                rapidClickMode.store(true, memory_order_relaxed);
                                cout << "*** 連打モード開始 ***" << endl;
                            }
                        }
                        else {
                            if (rapidClickMode.load(memory_order_relaxed)) {
                                rapidClickMode.store(false, memory_order_relaxed);
                                cout << "*** 連打モード終了 ***" << endl;
                            }
                        }
                    }
                }

                wasPressed.store(currentPressed, memory_order_relaxed);
            }

            if (++sleepCounter >= SLEEP_THRESHOLD) {
                Sleep(MAIN_LOOP_INTERVAL * 2);
                sleepCounter = 0;
            }
            else {
                Sleep(MAIN_LOOP_INTERVAL);
            }
        }

        timeEndPeriod(1);
    }

    void stop() {
        shouldExit.store(true, memory_order_relaxed);
    }
};

HighPerformanceClickAssist* g_assistant = nullptr;

static BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT && g_assistant) {
        cout << "\n終了処理中..." << endl;
        g_assistant->stop();
        return TRUE;
    }
    return FALSE;
}

int main() {
    HighPerformanceClickAssist assistant;
    g_assistant = &assistant;

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    try {
        assistant.run();
    }
    catch (const exception& e) {
        cerr << "エラー: " << e.what() << endl;
        return 1;
    }

    return 0;
}