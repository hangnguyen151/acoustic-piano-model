#pragma once
#include <juce_dsp/juce_dsp.h>

/**
    HammerModel
    -----------
    Mô phỏng tương tác phi tuyến giữa búa nỉ (felt hammer) và dây đàn.

    Theo mô hình vật lý cổ điển (Hall & Askenfelt, Chaigne & Askenfelt),
    lực nén của nỉ búa lên dây tuân theo quan hệ phi tuyến:

        F(x) = K * x^p          (x = độ nén nỉ búa, p = hệ số phi tuyến ~2..3.5)

    - "hardness" (độ cứng búa) điều khiển p và tần số cắt của bộ lọc định
      hình xung: búa cứng -> p lớn -> xung lực hẹp và nhiều năng lượng cao
      tần (âm sắc "sáng", có "click" đanh). Búa mềm -> xung rộng, mượt,
      âm ấm hơn, ít họa âm cao.
    - "velocity" (vận tốc phím) quyết định biên độ lực và cũng ảnh hưởng
      gián tiếp tới độ nén (chơi mạnh -> nỉ nén sâu hơn -> hơi cứng hơn),
      đúng như piano thật ("velocity-dependent timbre").

    Lớp này sinh ra một chuỗi sample lực kích thích (excitation impulse)
    có độ dài vài mili-giây, được StringModel::injectExcitation() bơm vào
    dây từng sample một trong quá trình búa tiếp xúc với dây.
*/
class HammerModel
{
public:
    void prepare (double sampleRate);

    /** Bắt đầu một cú gõ mới: velocity, hardness đều trong khoảng 0..1. */
    void trigger (float velocity, float hardness);

    /** Gọi mỗi sample trong khi búa đang tiếp xúc dây (contactActive() == true).
        Trả về sample lực tiếp theo trong xung kích thích. */
    float getNextSample() noexcept;

    bool contactActive() const noexcept { return sampleIndex < contactLengthSamples; }

private:
    double sampleRate = 44100.0;
    int sampleIndex = 0;
    int contactLengthSamples = 200;
    float amplitude = 0.0f;
    float shapeExponent = 2.0f;

    // Bộ lọc thông thấp một cực để "làm mềm" xung theo độ cứng búa
    // (búa mềm hơn -> cắt bớt tần số cao của xung lực).
    float lpState = 0.0f;
    float lpCoeff = 0.3f;
};
