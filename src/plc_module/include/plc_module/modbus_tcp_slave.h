#ifndef PLC_MODULE_MODBUS_TCP_SLAVE_H
#define PLC_MODULE_MODBUS_TCP_SLAVE_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <modbus/modbus.h>

#include "plc_module/modbus_slave_server.h"

class ModbusTCPSlave : public ModbusSlaveServer {
 public:
  ModbusTCPSlave(const std::string& ip, int port, int register_count,
                 int command_register);
  ~ModbusTCPSlave() override;

  void start() override;
  void stop() override;
  void syncHoldingRegisters(const std::vector<uint16_t>& registers) override;
  void setCommandWriteHandler(CommandWriteHandler handler) override;

 private:
  void acceptLoop();
  void clientWorker(int client_socket);

  std::string ip_;
  int port_;
  int register_count_;
  int command_register_;
  modbus_mapping_t* mapping_{nullptr};
  int server_socket_{-1};
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  std::mutex mapping_mutex_;
  std::mutex clients_mutex_;
  std::unordered_set<int> client_fds_;
  std::vector<std::thread> client_threads_;
  CommandWriteHandler command_handler_;
};

#endif
