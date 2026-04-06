// MiioDevice.h - 纯C++ miIO协议实现（AES-128-CBC + UDP）
// 参考 Python 原版 miio_proto.py
#pragma once
#include "pch.h"

// ─── MD5 简易实现 ───
namespace MiioMD5 {
    struct MD5Context {
        unsigned int state[4];
        unsigned int count[2];
        unsigned char buffer[64];
    };
    void Init(MD5Context* ctx);
    void Update(MD5Context* ctx, const unsigned char* data, unsigned int len);
    void Final(unsigned char digest[16], MD5Context* ctx);
    void Compute(const unsigned char* data, size_t len, unsigned char out[16]);
}

// ─── AES-128-CBC 简易实现 ───
namespace MiioAES {
    // 加密（PKCS7 Padding，输入明文，输出密文，长度必须是16的倍数+padding后）
    bool Encrypt(const unsigned char key[16], const unsigned char iv[16],
                 const unsigned char* plaintext, size_t plainLen,
                 std::vector<unsigned char>& ciphertext);
    bool Decrypt(const unsigned char key[16], const unsigned char iv[16],
                 const unsigned char* ciphertext, size_t cipherLen,
                 std::vector<unsigned char>& plaintext);
}

// ─── miIO 设备类 ───
struct MiioDeviceConfig {
    std::wstring ip;
    std::wstring token;
    std::wstring name;
    std::wstring model;
    bool autoConnect = false;
};

struct MiioProperty {
    int siid;
    int piid;
};

struct MiioModelProfile {
    const char* model;
    MiioProperty powerProperty;
    double powerScale = 1.0;
};

class MiioDevice {
public:
    static const int PORT = 54321;

    explicit MiioDevice(const std::string& ip, const std::string& token, int timeoutMs = 5000);
    ~MiioDevice() = default;

    bool Handshake();
    bool IsHandshaked() const { return m_handshaked; }

    // 发送命令，返回 result 字段 JSON 字符串
    bool Send(const std::string& method, const std::string& paramsJson, std::string& outResult);

    // 获取功率 (W)
    bool GetPower(double& outWatts);

private:
    std::string  m_ip;
    std::string  m_model;
    unsigned char m_token[16];
    int          m_timeoutMs;
    unsigned char m_key[16];
    unsigned char m_iv[16];
    unsigned int  m_deviceId  = 0;
    unsigned int  m_serverStamp = 0;
    long long     m_stampDelta  = 0;
    int           m_msgId      = 1;
    bool          m_handshaked = false;
    bool          m_modelQueried = false;

    std::vector<unsigned char> Encrypt(const std::string& plaintext);
    std::string Decrypt(const std::vector<unsigned char>& ciphertext);
    std::vector<unsigned char> BuildPacket(const std::string& payloadJson);
    std::string ParsePacket(const std::vector<unsigned char>& data);
    bool UdpSendRecv(const std::vector<unsigned char>& sendBuf,
                     std::vector<unsigned char>& recvBuf, int recvMax = 4096);
    unsigned int CurrentStamp() const;
    bool QueryDeviceModel();
    bool ReadNumericProperty(const MiioProperty& prop, double scale, double& outValue);
};
