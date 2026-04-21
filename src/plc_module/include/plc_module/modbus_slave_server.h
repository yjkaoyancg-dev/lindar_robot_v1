#ifndef PLC_MODULE_MODBUS_SLAVE_SERVER_H
#define PLC_MODULE_MODBUS_SLAVE_SERVER_H

#include <cstdint>
#include <functional>
#include <vector>

class ModbusSlaveServer {
 public:
  using CommandWriteHandler = std::function<void(uint16_t)>;

  virtual ~ModbusSlaveServer() = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void syncHoldingRegisters(const std::vector<uint16_t>& registers) = 0;
  virtual void setCommandWriteHandler(CommandWriteHandler handler) = 0;
};

#endif
