# Acoustic Piano Model — JUCE Physical Modeling Piano Plugin

Plugin synth piano mô phỏng vật lý (physical/acoustic modeling), theo hướng
tiếp cận tương tự Pianoteq: thay vì phát lại sample thu âm, âm thanh được
**tổng hợp theo thời gian thực** từ mô hình toán học của dây đàn, búa, damper
và bàn cộng hưởng.

## Cấu trúc mã nguồn và ánh xạ với yêu cầu

| Yêu cầu | File triển khai |
|---|---|
| 1. Bộ xử lý dao động dây đàn (nghiệm phương trình sóng) | `Source/DSP/StringModel.h/.cpp` |
| 2. Cộng hưởng bàn đạp sustain (sympathetic resonance) | `Source/DSP/PedalResonanceEngine.h/.cpp` |
| 3a. Hammer hardness (độ cứng búa, phi tuyến) | `Source/DSP/HammerModel.h/.cpp` |
| 3b. Damper noise + Key-off sound | `Source/DSP/DamperModel.h/.cpp` |
| 4. Convolution Reverb chất lượng cao ở output bus | `Source/DSP/ConvolutionReverbUnit.h/.cpp` |
| Điều phối voice, MIDI, tham số, master bus | `Source/PluginProcessor.h/.cpp` |
| Một giọng đàn (dây + búa + damper) | `Source/PianoVoice.h/.cpp` |
| GUI tối giản | `Source/PluginEditor.h/.cpp` |

## Nguyên lý kỹ thuật cốt lõi

### 1. String Vibration DSP (`StringModel`)
Dùng **Digital Waveguide Synthesis** (Julius O. Smith) — nghiệm D'Alembert
của phương trình sóng 1 chiều có tổn hao và độ cứng:

```
y_tt = c^2 y_xx - 2 b1 y_t + b3 y_xxt - kappa^2 y_xxxx
y(x,t) = y+(t - x/c) + y-(t + x/c)
```

Hai delay-line vòng (`rightGoing`, `leftGoing`) biểu diễn hai sóng truyền
ngược chiều. Tổn hao mô phỏng bằng bộ lọc thông thấp một cực trong vòng lặp
phản xạ tại ngựa đàn/nut; độ cứng dây thép (gây inharmonicity — họa âm lệch
khỏi bội số nguyên) mô phỏng bằng chuỗi 6 bộ lọc allpass bậc nhất (dispersion
filter), hệ số điều khiển theo công thức Fletcher `f_n = n f0 sqrt(1+B n^2)`.

### 2. Sustain Pedal Resonance (`PedalResonanceEngine`)
Khi CC64 (sustain) được giữ, damper của **toàn bộ** dây (kể cả nốt không
đánh) được nhấc. Mỗi sample audio: tất cả `StringModel` có damper nhấc góp
đầu ra vào một bus cộng hưởng chung; bus này chạy qua 4 bộ lọc bandpass mô
phỏng các mode dao động chính của soundboard, rồi hồi tiếp một phần nhỏ trở
lại từng dây ở sample kế tiếp — tạo hiệu ứng cộng hưởng liên-dây thật.

**Lưu ý kiến trúc quan trọng**: vì cơ chế này cần thu năng lượng từ *mọi*
voice tại *cùng* một thời điểm sample, plugin **không dùng `juce::Synthesiser`
tiêu chuẩn** (vốn render trọn khối cho từng voice tuần tự). Thay vào đó,
`PluginProcessor::processBlock` tự vòng lặp từng sample và gọi
`PianoVoice::processOneSample()` cho mọi voice theo đúng thứ tự
`beginSample() → mọi voice → finishSample()`.

### 3. Cơ học búa/damper
- **HammerModel**: lực nén nỉ búa phi tuyến `F(x) = K x^p`, `p` (độ cứng)
  và thời gian tiếp xúc điều khiển bởi tham số Hammer Hardness + velocity.
- **DamperModel**: nhiễu trắng lọc bandpass tạo tiếng "sượt" khi damper nhấc
  (note-on) và tiếng "thụp" + noise khi damper rơi lại (note-off, key-off
  sound), độc lập với bao hình biên độ chính của dây.

### 4. Convolution Reverb (`ConvolutionReverbUnit`)
Bọc `juce::dsp::Convolution` (partitioned FFT, real-time an toàn). Vì plugin
cần tự chứa, impulse response được **tự sinh thuật toán** (early reflections
rời rạc + đuôi khuếch tán lọc thông thấp giảm dần theo thời gian) thay vì
phụ thuộc file `.wav` ngoài. Có sẵn `loadImpulseResponseFromFile()` nếu muốn
nạp IR thật đo từ phòng hòa nhạc.

## Build

Cần JUCE (khuyến nghị 7.x/8.x): tải từ https://github.com/juce-framework/JUCE
và đặt cạnh thư mục project dưới tên `JUCE/`, hoặc cài đặt rồi dùng
`find_package(JUCE CONFIG REQUIRED)`.

```bash
git clone https://github.com/juce-framework/JUCE.git   # nếu chưa có
cd AcousticPianoModel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Plugin build ra định dạng VST3, AU (macOS), và Standalone.

## Tham số điều khiển (APVTS, tự động hóa được từ DAW)

- **Hammer Hardness** (0..1): độ cứng búa.
- **Sympathetic Resonance** (0..1.5x): cường độ cộng hưởng liên dây khi đạp pedal.
- **Reverb Mix / Reverb Size**: điều khiển convolution reverb.
- **Output Gain** (dB).
- **Unison Detune (cents)**: độ lệch tần số giữa các dây unison cùng nốt.

## Giới hạn đã biết / hướng mở rộng

- Mô hình hiện dùng một `StringModel` đại diện cho cả bó dây unison với
  detune nhẹ, chưa mô phỏng riêng biệt tương tác cơ học giữa các dây trong
  cùng bó (string-to-string coupling tại chốt).
- IR reverb tự sinh mang tính xấp xỉ; có thể thay bằng IR đo thật qua
  `loadImpulseResponseFromFile()` để tăng độ chân thực.
- Chưa mô phỏng riêng "una corda" (soft pedal) và "sostenuto" (pedal giữa) —
  có thể mở rộng bằng cách thêm cờ điều khiển tương tự sustain trong
  `PedalResonanceEngine`/`PianoVoice`.
