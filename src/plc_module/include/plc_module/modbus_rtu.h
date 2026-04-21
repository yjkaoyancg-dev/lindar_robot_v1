/*******************************************************************************
****FilePath: /Photonix/src/plc_module/include/plc_module/modbus_rtu.h
****Author: mwt 911608720@qq.com
****Date: 2025-04-15 09:00:58
****Description:
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#ifndef MODBUS_RTU_HPP
#define MODBUS_RTU_HPP

#include <modbus/modbus.h>

#include <string>

#include "comm_interface.h"

class ModbusRTU : public CommInterface {
 public:
  ModbusRTU(const std::string& device, int baud, char parity, int data_bit,
            int stop_bit);
  ~ModbusRTU() override;
  void connect(int slave) override;
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
  std::string device_;
  int baud_, data_bit_, stop_bit_, slave_id_;
  char parity_;
  modbus_t* ctx_;
};

#endif  // MODBUS_RTU_HPP
