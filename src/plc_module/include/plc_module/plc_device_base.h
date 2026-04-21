#ifndef PLC_DEVICE_BASE_HPP
#define PLC_DEVICE_BASE_HPP

#include <string>
#include <variant>
#include <vector>
#include <cstdint>

enum class ByteOrder {
    BigEndian,
    LittleEndian
};
using PlcValue = std::variant<bool, int16_t, int32_t, float, double>;
class PlcDevice {
public:
    explicit PlcDevice(ByteOrder order = ByteOrder::BigEndian)
        : byte_order_(order) {}
    virtual ~PlcDevice() = default;

    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void sendRequest() = 0;
    virtual void handleResponse() = 0;
    virtual std::string getDeviceInfo() const = 0;

protected:
    ByteOrder byte_order_;

    // 工具函数：编码任意 PLC 数据类型为 modbus 16-bit 寄存器数组
    std::vector<uint16_t> encodeValue(const PlcValue& value) const;

    //解码为 PlcValue，可用于控制类/轮询类设备
    PlcValue decodeValue(const std::vector<uint16_t>& regs, const std::string& type_hint) const;
};

#endif  // PLC_DEVICE_BASE_HPP
