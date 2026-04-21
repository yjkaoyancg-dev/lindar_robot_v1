/*******************************************************************************
****FilePath: /Photonix/src/plc_module/src/modbus_tcp.cc
****Author: mwt 911608720@qq.com
****Date: 2025-04-15 09:02:12
****Description:
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#include "plc_module/modbus_tcp.h"

#include <cstring>
#include <stdexcept>

ModbusTCP::ModbusTCP(const std::string& ip, int port)
    : ip_(ip), port_(port), ctx_(nullptr) {}

ModbusTCP::~ModbusTCP() { disconnect(); }

void ModbusTCP::connect(int slave_id) {
  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }

  ctx_ = modbus_new_tcp(ip_.c_str(), port_);
  if (!ctx_) {
    throw std::runtime_error(
        "modbus tcp 连接失败: modbus_new_tcp 返回空指针");
  }

  if (modbus_connect(ctx_) == -1) {
    std::string err = modbus_strerror(errno);
    modbus_free(ctx_);
    ctx_ = nullptr;
    throw std::runtime_error("modbus tcp 连接失败: " + err);
  }

  if (modbus_set_slave(ctx_, slave_id) == -1) {
    std::string err = modbus_strerror(errno);
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
    throw std::runtime_error("设置从站 ID 失败: " + err);
  }
}

void ModbusTCP::disconnect() {
  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }
}

bool ModbusTCP::writeRegisters(int address, const std::vector<uint16_t>& data) {
  if (!ctx_) return false;
  int rc = modbus_write_registers(ctx_, address, data.size(), data.data());
  return rc != -1;
}

std::string ModbusTCP::info() const {
  return "ModbusTCP(" + ip_ + ":" + std::to_string(port_) + ")";
}
bool ModbusTCP::readHoldingRegisters(int address, int count,
                                     std::vector<uint16_t>& out) {
  if (!ctx_) return false;
  out.resize(count);
  int rc = modbus_read_registers(ctx_, address, count, out.data());
  return rc == count;
}

bool ModbusTCP::readInputRegisters(int address, int count,
                                   std::vector<uint16_t>& out) {
  if (!ctx_) return false;
  out.resize(count);
  int rc = modbus_read_input_registers(ctx_, address, count, out.data());
  return rc == count;
}
bool ModbusTCP::writeCoil(int address, bool value) {
  if (!ctx_) return false;
  return modbus_write_bit(ctx_, address, value ? 1 : 0) == 1;
}
void ModbusTCP::setSlaveId(int id) const {
  if (ctx_) modbus_set_slave(ctx_, id);
}
