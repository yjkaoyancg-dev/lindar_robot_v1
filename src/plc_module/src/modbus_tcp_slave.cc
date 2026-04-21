#include "plc_module/modbus_tcp_slave.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace {

void safe_close(int& fd) {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

void enable_keepalive(int fd) {
  int yes = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
  int idle = 10;
  ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
  int interval = 3;
  ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
#endif
#ifdef TCP_KEEPCNT
  int count = 3;
  ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

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

ModbusTCPSlave::ModbusTCPSlave(const std::string& ip, int port,
                               int register_count, int command_register)
    : ip_(ip),
      port_(port),
      register_count_(register_count),
      command_register_(command_register) {}

ModbusTCPSlave::~ModbusTCPSlave() { stop(); }

void ModbusTCPSlave::start() {
  if (running_.load(std::memory_order_acquire)) {
    return;
  }

  mapping_ = modbus_mapping_new(0, 0, register_count_, 0);
  if (!mapping_) {
    throw std::runtime_error("modbus_mapping_new failed");
  }
  std::fill(mapping_->tab_registers, mapping_->tab_registers + register_count_,
            0);

  modbus_t* listen_ctx = modbus_new_tcp(ip_.c_str(), port_);
  if (!listen_ctx) {
    throw std::runtime_error("modbus_new_tcp failed");
  }

  server_socket_ = modbus_tcp_listen(listen_ctx, 8);
  modbus_free(listen_ctx);
  if (server_socket_ < 0) {
    throw std::runtime_error("modbus_tcp_listen failed");
  }

  running_.store(true, std::memory_order_release);
  accept_thread_ = std::thread(&ModbusTCPSlave::acceptLoop, this);
}

void ModbusTCPSlave::stop() {
  running_.store(false, std::memory_order_release);
  safe_close(server_socket_);

  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  std::vector<int> fds;
  {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    fds.reserve(client_fds_.size());
    for (int fd : client_fds_) {
      fds.push_back(fd);
    }
  }

  for (int fd : fds) {
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
  }

  {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    for (auto& thread : client_threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    client_threads_.clear();
    client_fds_.clear();
  }

  if (mapping_) {
    modbus_mapping_free(mapping_);
    mapping_ = nullptr;
  }
}

void ModbusTCPSlave::syncHoldingRegisters(
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

void ModbusTCPSlave::setCommandWriteHandler(CommandWriteHandler handler) {
  command_handler_ = std::move(handler);
}

void ModbusTCPSlave::acceptLoop() {
  while (running_.load(std::memory_order_acquire)) {
    pollfd pfd{};
    pfd.fd = server_socket_;
    pfd.events = POLLIN;

    const int prc = ::poll(&pfd, 1, 250);
    if (!running_.load(std::memory_order_acquire)) {
      break;
    }
    if (prc <= 0 || !(pfd.revents & POLLIN)) {
      continue;
    }

    int client_socket = ::accept(server_socket_, nullptr, nullptr);
    if (client_socket < 0) {
      continue;
    }

    enable_keepalive(client_socket);
    std::lock_guard<std::mutex> lk(clients_mutex_);
    client_fds_.insert(client_socket);
    client_threads_.emplace_back([this, client_socket]() {
      clientWorker(client_socket);
      std::lock_guard<std::mutex> lk2(clients_mutex_);
      client_fds_.erase(client_socket);
    });
  }
}

void ModbusTCPSlave::clientWorker(int client_socket) {
  modbus_t* ctx = modbus_new_tcp(ip_.c_str(), port_);
  if (!ctx) {
    ::close(client_socket);
    return;
  }

  modbus_set_response_timeout(ctx, 30, 0);
  modbus_set_byte_timeout(ctx, 0, 300000);
  modbus_set_socket(ctx, client_socket);
  modbus_set_debug(ctx, 0);

  std::vector<uint8_t> query(260);
  while (running_.load(std::memory_order_acquire)) {
    const int rc = modbus_receive(ctx, query.data());
    if (rc <= 0) {
      break;
    }

    bool touches_command =
        request_touches_register(ctx, query.data(), rc, command_register_);
    uint16_t command_value = 0U;

    {
      std::lock_guard<std::mutex> lk(mapping_mutex_);
      if (modbus_reply(ctx, query.data(), rc, mapping_) == -1) {
        break;
      }
      if (touches_command && mapping_) {
        command_value = mapping_->tab_registers[command_register_];
      }
    }

    if (touches_command && command_handler_) {
      command_handler_(command_value);
    }
  }

  ::shutdown(client_socket, SHUT_RDWR);
  ::close(client_socket);
  modbus_close(ctx);
  modbus_free(ctx);
}
