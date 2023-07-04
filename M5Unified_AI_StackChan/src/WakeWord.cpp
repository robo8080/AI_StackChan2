#include <M5Unified.h>

#include <memory>

#include <esp_ns.h>
#include <SPIFFS.h>

#include "simplevox.h"

#define base_path "/spiffs"
#define file_name "/wakeword.bin"
constexpr int kSampleRate = 16000;
constexpr int audioLength = kSampleRate * 3;  // 3 seconds
constexpr int kRxBufferNum = 3;
int16_t* rawAudio;
int16_t* rxBuffer;
int16_t* rawBuffer;
ns_handle_t nsInst;
simplevox::VadEngine vadEngine;
simplevox::MfccEngine mfccEngine;
simplevox::MfccFeature* mfcc = nullptr;

LGFX_Button regButton;
LGFX_Button cmpButton;

/**
 * @brief 1サンプリングごとにMFCCを算出するexample
 * @details
 * VAD -> Buffer -> MFCCの流れで逐次的にMFCCの算出を行う。
 * この例では参考のためにREGISTではraw dataを残しているが、
 * COMPAREの処理だけにすればraw dataは必要なくなるためメモリサイズの削減が可能となる。
 * 処理としてHangbeforeおよびPreDetection時間のデータ処理が複雑な点と
 * frame_lengthがVADとMFCCで異なり、１つの処理単位がhop_lengthである点など
 * 構造を理解していないと分からない処理が多いのが難点…。
 */

/**
 * @brief 除算を行い演算結果を切り上げます（正の整数）
 * @param[in] dividend  被除数
 * @param[in]  divisor   除数
 * @return 切り上げた整数(3/2 -> 2)
 */
constexpr int divCeil(int dividend, int divisor)
{
  return (dividend + divisor - 1) / divisor;
}

/**
 * @brief １フレームの録音を行い読み取り可能なバッファを返します
 * @return  読み取り可能な１フレーム分のバッファ
 */
int16_t* rxMic()
{
  static int rxIndex = 0;
  const int frameLength = vadEngine.config().frame_length();

  if (!M5.Mic.record(&rxBuffer[frameLength * rxIndex], frameLength, kSampleRate))
  {
    return nullptr;
  }
  rxIndex++;
  rxIndex = (rxIndex >= kRxBufferNum) ? 0 : rxIndex;
  return &rxBuffer[rxIndex * frameLength];
}

int rawBufferMaxSize;
int rawBufferSize;
void raw_init(int length)
{
  rawBufferMaxSize = length;
  rawBufferSize = 0;
  rawBuffer = (int16_t*)heap_caps_calloc(rawBufferMaxSize, sizeof(*rawBuffer), MALLOC_CAP_8BIT);
}

int raw_size() { return rawBufferSize; }
void raw_reset() { rawBufferSize = 0; }
int16_t* raw_front() { return rawBuffer; }

void raw_pushBack(const int16_t* src, int length)
{
  if (rawBufferSize + length <= rawBufferMaxSize)
  {
    std::copy_n(src, length, &rawBuffer[rawBufferSize]);
    rawBufferSize += length;
  }
}

void raw_popFront(int length)
{
  std::copy_n(&rawBuffer[length], rawBufferSize - length, rawBuffer);
  rawBufferSize -= length;
}

float* features;
int mfccFrameNum;
int mfccCoefNum;
int mfccBeforeFrameNum;
void feature_init(const simplevox::MfccConfig& mfccConfig, const simplevox::VadConfig& vadConfig, int maxTimeMs)
{
  const int maxLength = maxTimeMs * mfccConfig.sample_rate / 1000;
  mfccFrameNum = (maxLength - (mfccConfig.frame_length() - mfccConfig.hop_length())) / mfccConfig.hop_length();
  mfccCoefNum = mfccConfig.coef_num;
  features = (float*)heap_caps_malloc(sizeof(*features) * mfccFrameNum * mfccCoefNum, MALLOC_CAP_8BIT);

  // VADのHangbeforeおよびPreDetectionに相当するデータサイズを算出
  const int vadBeforeLength = 
            vadConfig.frame_length() *
            ( divCeil(vadConfig.before_length(), vadConfig.frame_length())
            + divCeil(vadConfig.decision_length(), vadConfig.frame_length()));
  // VADのBefore区間に相当するフレーム数を算出
  mfccBeforeFrameNum = (vadBeforeLength - (mfccConfig.frame_length() - mfccConfig.hop_length())) / mfccConfig.hop_length();
}

float* feature_get(int number) { return &features[number * mfccCoefNum]; }

constexpr uint32_t memCaps = (CONFIG_SPIRAM) ? (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM) : MALLOC_CAP_8BIT;

void wakeword_setup(){
  // auto vadConfig = vadEngine.config();
  // vadConfig.sample_rate = kSampleRate;
  // auto mfccConfig = mfccEngine.config();
  // mfccConfig.sample_rate = kSampleRate;

  // rawAudio = (int16_t*)heap_caps_malloc(audioLength * sizeof(*rawAudio), memCaps);
  // rxBuffer = (int16_t*)heap_caps_malloc(kRxBufferNum * vadConfig.frame_length() * sizeof(*rxBuffer), MALLOC_CAP_8BIT);
  
  // raw_init(mfccConfig.frame_length() + vadConfig.frame_length());
  // feature_init(mfccConfig, vadConfig, 3000);
}

int mode = 0;   // 0: none, <0: REGIST, >0: COMPARE

void clear_buff(){
  // memset(rawAudio, 0 ,audioLength * sizeof(*rawAudio));
  auto vadConfig = vadEngine.config();
  auto mfccConfig = mfccEngine.config();
  // memset(rxBuffer, 0 ,kRxBufferNum * vadConfig.frame_length() * sizeof(*rxBuffer));
  raw_init(mfccConfig.frame_length() + vadConfig.frame_length());
//  feature_init(mfccConfig, vadConfig, 3000);
}

void wakeword_init(){
  auto vadConfig = vadEngine.config();
  vadConfig.sample_rate = kSampleRate;
  auto mfccConfig = mfccEngine.config();
  mfccConfig.sample_rate = kSampleRate;

  rawAudio = (int16_t*)heap_caps_malloc(audioLength * sizeof(*rawAudio), memCaps);
  rxBuffer = (int16_t*)heap_caps_malloc(kRxBufferNum * vadConfig.frame_length() * sizeof(*rxBuffer), MALLOC_CAP_8BIT);
  
  raw_init(mfccConfig.frame_length() + vadConfig.frame_length());
  feature_init(mfccConfig, vadConfig, 3000);

  nsInst = ns_pro_create(vadConfig.frame_time_ms, 1, vadConfig.sample_rate);
  if (nsInst == NULL)
  {
    M5.Display.println("Failed to initialize ns.");
    while(true) delay(10);
  }
  if (!vadEngine.init(vadConfig))
  {
    M5.Display.println("Failed to initialize vad.");
    while(true) delay(10);
  }
  if (!mfccEngine.init(mfccConfig))
  {
    M5.Display.println("Failed to initialize mfcc.");
    while(true) delay(10);
  }
  
  SPIFFS.begin(true);
  if (SPIFFS.exists(file_name))
  {
    M5.Display.println("File exists !!");
    mfcc = mfccEngine.loadFile(base_path file_name);
    //mode = 1;
  }
}

bool wakeword_regist() // 検出した音声を再生しMFCCを登録及びファイルに保存します
{
  bool ret =false;
  // M5.Speaker.end();
  // M5.Mic.begin();
  auto* data = rxMic();
  // M5.Mic.end();
  // M5.Speaker.begin();
  if (data == nullptr) { return ret; }
  ns_process(nsInst, data, data);

  int length = vadEngine.detect(rawAudio, audioLength, data);
  if (length <= 0) { return ret; }
//  if (length <= 100) { return ret; }
  M5.Mic.end();
  if (M5.Speaker.begin())
  {
    M5.Speaker.playRaw(rawAudio, length, kSampleRate);
    while (M5.Speaker.isPlaying()) { delay(10); }
    M5.Speaker.end();
  }
  M5.Mic.begin();
  if (mfcc != nullptr){ delete mfcc; }
  mfcc = mfccEngine.create(rawAudio, length);
  if (mfcc)
  {
    mfccEngine.saveFile(base_path file_name, *mfcc);
    //mode = 1;
    ret = true;
  }
  vadEngine.reset();
//  mode = 0;
//  M5.Mic.end();
  return ret;
}

bool wakeword_compare()  
{
  // M5.Speaker.end();
  // M5.Mic.begin();
  bool ret = false;
  auto* data = rxMic();
  // M5.Speaker.begin();
  if (data == nullptr) {
    return false; 
  }
  ns_process(nsInst, data, data);

  if (mfcc == nullptr) {
     return false; 
  }
  static int mfccFrameCount = 0;
  const auto state = vadEngine.process(data);
  const int vadFrameLength = vadEngine.config().frame_length();
  const int mfccFrameLength = mfccEngine.config().frame_length();
  const int mfccHopLength = mfccEngine.config().hop_length();
  // Silence以上ならrawBufferにデータを追加
  if (state >= simplevox::VadState::Silence)
  {
    raw_pushBack(data, vadFrameLength);
  }
  // rawBufferにMFCCのframe_length()以上のデータがある場合はMFCCを算出
  while (raw_size() >= mfccFrameLength && mfccFrameCount < mfccFrameNum)
  {
    // feature_pushBack
    mfccEngine.calculate(raw_front(), feature_get(mfccFrameCount));
    mfccFrameCount++;
    raw_popFront(mfccHopLength);  // hop_length分シフト
  }
  // Speech以前(Silence, PreDetection)の場合はmfccBeforeFrameNumを超えた分は取り除く(シフトする)
  if (state < simplevox::VadState::Speech && mfccFrameCount > mfccBeforeFrameNum)
  {
    const int shiftCount = mfccFrameCount - mfccBeforeFrameNum;
    const int shiftLength = shiftCount * mfccCoefNum;
    std::copy_n(&features[shiftLength], mfccFrameCount * mfccCoefNum - shiftLength, features);
    mfccFrameCount -= shiftCount;
  }
  // 検出完了もしくは最大フレームに到達した場合は判定終了
  if (state == simplevox::VadState::Detected
      || (state >= simplevox::VadState::Speech && mfccFrameNum <= mfccFrameCount))
  {
    std::unique_ptr<simplevox::MfccFeature> feature(mfccEngine.create(features, mfccFrameCount, mfccCoefNum));
    const auto dist = simplevox::calcDTW(*mfcc, *feature);
    char cbuf[64];
    char pass = (dist < 180) ? '!': '?';  // 180未満で一致と判定, しきい値は要調整
    sprintf(cbuf, "Dist: %6lu, %c", dist, pass);
    //M5.Display.drawString(cbuf, 0, 50);
    Serial.println(String(cbuf));
    raw_reset();
    mfccFrameCount = 0;
    vadEngine.reset();
    //mode = 0;
//    M5.Mic.end();
    if(dist < 180) ret = true;
  }    
  return ret;
}
