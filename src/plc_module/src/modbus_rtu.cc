/*******************************************************************************
****FilePath: /Photonix/src/plc_module/src/modbus_rtu.cc
****Author: mwt 911608720@qq.com
****Date: 2025-04-15 09:02:16
****Description:
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#include "plc_module/modbus_rtu.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void log_modbus_rtu_warn(const std::string& msg) {
  std::cerr << "[modbus_rtu][WARN] " << msg << std::endl;
}

void log_modbus_rtu_error(const std::string& msg) {
  std::cerr << "[modbus_rtu][ERROR] " << msg << std::endl;
}
}  // namespace

ModbusRTU::ModbusRTU(
    const std::string& device, int baud,
    char parity,   // 可以是 'N', 'E', 'O' 无校验，偶校验，奇校验
    int data_bit,  // 数据位，一般是 7 或 8
    int stop_bit)  // 停止位，一般是 1 或 2
    : device_(device),
      baud_(baud),
      data_bit_(data_bit),
      stop_bit_(stop_bit),
      parity_(parity),
      ctx_(nullptr) {}

ModbusRTU::~ModbusRTU() { disconnect(); }

void ModbusRTU::connect(int slave_id) {
  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }

  ctx_ = modbus_new_rtu(device_.c_str(), baud_, parity_, data_bit_, stop_bit_);
  if (!ctx_) {
    throw std::runtime_error("创建 Modbus RTU 失败");
  }

  if (modbus_set_slave(ctx_, slave_id) == -1) {
    std::string err = modbus_strerror(errno);
    throw std::runtime_error("设置从站 ID 失败: " + err);
  }

  if (modbus_connect(ctx_) == -1) {
    std::string err = modbus_strerror(errno);
    throw std::runtime_error("串口连接失败: " + err);
  }
  modbus_set_debug(ctx_, false);
}

void ModbusRTU::disconnect() {
  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }
}

bool ModbusRTU::writeRegisters(int address, const std::vector<uint16_t>& data) {
  if (!ctx_ || data.empty()) return false;

  int rc = -1;
  if (data.size() == 1) {
    rc = modbus_write_register(ctx_, address, data[0]); // 0x06
    if (rc != 1) {
      log_modbus_rtu_error("write_register fail addr=" +
                           std::to_string(address) + " val=" +
                           std::to_string(data[0]) + " err=" +
                           modbus_strerror(errno));
      return false;
    }
    return true;
  }

  rc = modbus_write_registers(ctx_, address, (int)data.size(), data.data()); // 0x10
  if (rc != (int)data.size()) {
    log_modbus_rtu_error("write_registers fail addr=" +
                         std::to_string(address) + " n=" +
                         std::to_string(data.size()) + " err=" +
                         modbus_strerror(errno));
    return false;
  }
  return true;
}


std::string ModbusRTU::info() const {
  return "ModbusRTU(" + device_ + ", " + std::to_string(baud_) + ", " +
         parity_ + ", " + std::to_string(data_bit_) + ", " +
         std::to_string(stop_bit_) + ")";
}
bool ModbusRTU::readHoldingRegisters(int address, int count,
                                     std::vector<uint16_t>& out) {
  if (!ctx_) return false;
  out.resize(count);
  int rc = modbus_read_registers(ctx_, address, count, out.data());
  if (rc == -1) {
    std::string err = modbus_strerror(errno);
    log_modbus_rtu_warn("readHoldingRegisters失败: " + err);
  }
  return rc == count;
}

bool ModbusRTU::readInputRegisters(int address, int count,
                                   std::vector<uint16_t>& out) {
  if (!ctx_) return false;
  out.resize(count);
  int rc = modbus_read_input_registers(ctx_, address, count, out.data());
  return rc == count;
}
bool ModbusRTU::writeCoil(int address, bool value) {
  if (!ctx_) return false;
  return modbus_write_bit(ctx_, address, value ? 1 : 0) == 1;
}
void ModbusRTU::setSlaveId(int id) const {
  if (!ctx_) return;
  modbus_set_slave(ctx_, id);
}
