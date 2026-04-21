/*******************************************************************************
****FilePath: /Photonix/src/plc_module/include/plc_module/modbus_tcp.h
****Author: mwt 911608720@qq.com
****Date: 2025-04-15 09:00:54
****Description:
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#ifndef MODBUS_TCP_HPP
#define MODBUS_TCP_HPP

#include <modbus/modbus.h>

#include <string>

#include "comm_interface.h"

class ModbusTCP : public CommInterface {
 public:
  ModbusTCP(const std::string& ip, int port);
  ~ModbusTCP() override;
  void connect(int slave_id) override;
  void disconnect() override;
  bool writeRegisters(int address, const std::vector<uint16_t>& data) override;
  bool readHoldingRegisters(int address, int count,
                            std::vector<uint16_t>& out) override;
  bool readInputRegisters(int address, int count,
                          std::vector<uint16_t>& out) override;
  bool writeCoil(int address, bool value) override;

  void setSlaveId(int id) const override;

  std::string info() const override;

 private:
  std::string ip_;
  int port_;
  modbus_t* ctx_;
};

#endif  // MODBUS_TCP_HPP
