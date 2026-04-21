#include "plc_module/modbus_rtu_slave.h"

#include <algorithm>
#include <cerrno>
#include <stdexcept>

namespace {

bool request_touches_register(modbus_t* ctx, const uint8_t* query, int rc,
                              int target_register) {
  const int header_length = modbus_get_header_length(ctx);
  if (header_length < 0 || header_length + 4 >= rc) {
    return false;
  }

  const uint8_t function_code = query[header_length];
  if (function_code != 0x06 && function_code != 0x10) {
    return false;
  }

  const int address =
      (static_cast<int>(query[header_length + 1]) << 8) |
      static_cast<int>(query[header_length + 2]);
  int count = 1;
  if (function_code == 0x10) {
    count = (static_cast<int>(query[header_length + 3]) << 8) |
            static_cast<int>(query[header_length + 4]);
  }

  return target_register >= address && target_register < address + count;
}

}  // namespace

ModbusRTUSlave::ModbusRTUSlave(const std::string& device, int baud,
                               char parity, int data_bit, int stop_bit,
                               int slave_id, int register_count,
                               int command_register)
    : device_(device),
      baud_(baud),
      parity_(parity),
      data_bit_(data_bit),
      stop_bit_(stop_bit),
      slave_id_(slave_id),
      register_count_(register_count),
      command_register_(command_register) {}

ModbusRTUSlave::~ModbusRTUSlave() { stop(); }

void ModbusRTUSlave::start() {
  if (running_.load(std::memory_order_acquire)) {
    return;
  }

  mapping_ = modbus_mapping_new(0, 0, register_count_, 0);
  if (!mapping_) {
    throw std::runtime_error("modbus_mapping_new failed");
  }
  std::fill(mapping_->tab_registers, mapping_->tab_registers + register_count_,
            0);

  ctx_ = modbus_new_rtu(device_.c_str(), baud_, parity_, data_bit_, stop_bit_);
  if (!ctx_) {
    throw std::runtime_error("modbus_new_rtu failed");
  }
  if (modbus_set_slave(ctx_, slave_id_) == -1) {
    throw std::runtime_error("modbus_set_slave failed");
  }
  if (modbus_connect(ctx_) == -1) {
    throw std::runtime_error("modbus_connect failed");
  }
  modbus_set_debug(ctx_, 0);
  modbus_set_response_timeout(ctx_, 30, 0);
  modbus_set_byte_timeout(ctx_, 0, 300000);

  running_.store(true, std::memory_order_release);
  worker_thread_ = std::thread(&ModbusRTUSlave::workerLoop, this);
}

void ModbusRTUSlave::stop() {
  running_.store(false, std::memory_order_release);

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }

  if (mapping_) {
    modbus_mapping_free(mapping_);
    mapping_ = nullptr;
  }
}

void ModbusRTUSlave::syncHoldingRegisters(
    const std::vector<uint16_t>& registers) {
  std::lock_guard<std::mutex> lk(mapping_mutex_);
  if (!mapping_) {
    return;
  }

  const std::size_t count =
      std::min(registers.size(), static_cast<std::size_t>(register_count_));
  for (std::size_t index = 0; index < count; ++index) {
    mapping_->tab_registers[index] = registers[index];
  }
}

void ModbusRTUSlave::setCommandWriteHandler(CommandWriteHandler handler) {
  command_handler_ = std::move(handler);
}

void ModbusRTUSlave::workerLoop() {
  std::vector<uint8_t> query(MODBUS_RTU_MAX_ADU_LENGTH, 0U);
  while (running_.load(std::memory_order_acquire)) {
    errno = 0;
    const int rc = modbus_receive(ctx_, query.data());
    if (rc <= 0) {
      if (!running_.load(std::memory_order_acquire)) {
        break;
      }
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      continue;
    }

    const bool touches_command =
        request_touches_register(ctx_, query.data(), rc, command_register_);
    uint16_t command_value = 0U;

    {
      std::lock_guard<std::mutex> lk(mapping_mutex_);
      if (modbus_reply(ctx_, query.data(), rc, mapping_) == -1) {
        continue;
      }
      if (touches_command && mapping_) {
        command_value = mapping_->tab_registers[command_register_];
      }
    }

    if (touches_command && command_handler_) {
      command_handler_(command_value);
    }
  }
}
