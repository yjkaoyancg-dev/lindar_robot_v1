#ifndef PLC_MODULE_MODBUS_RTU_SLAVE_H
#define PLC_MODULE_MODBUS_RTU_SLAVE_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <modbus/modbus.h>

#include "plc_module/modbus_slave_server.h"

class ModbusRTUSlave : public ModbusSlaveServer {
 public:
  ModbusRTUSlave(const std::string& device, int baud, char parity, int data_bit,
                 int stop_bit, int slave_id, int register_count,
                 int command_register);
  ~ModbusRTUSlave() override;

  void start() override;
  void stop() override;
  void syncHoldingRegisters(const std::vector<uint16_t>& registers) override;
  void setCommandWriteHandler(CommandWriteHandler handler) override;

 private:
  void workerLoop();

  std::string device_;
  int baud_;
  char parity_;
  int data_bit_;
  int stop_bit_;
  int slave_id_;
  int register_count_;
  int command_register_;
  modbus_t* ctx_{nullptr};
  modbus_mapping_t* mapping_{nullptr};
  std::atomic<bool> running_{false};
  std::thread worker_thread_;
  std::mutex mapping_mutex_;
  CommandWriteHandler command_handler_;
};

#endif
