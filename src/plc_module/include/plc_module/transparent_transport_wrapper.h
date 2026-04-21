/*******************************************************************************
****FilePath: /Photonix/src/plc_module/include/plc_module/transparent_transport_wrapper.h
****Author: mwt 911608720@qq.com
****Date: 2025-07-31 12:57:59
****Description: 
****Copyright: 2025 by mwt, All Rights Reserved.
********************************************************************************/
#ifndef TRANSPARENT_TRANSPORT_WRAPPER_H
#define TRANSPARENT_TRANSPORT_WRAPPER_H

#include <string>
#include <vector>
#include <mutex>
#include <boost/asio.hpp>
#include "plc_module/comm_interface.h"

class TransparentTransportWrapper : public CommInterface {
public:
    TransparentTransportWrapper(const std::string& ip, int port);
    ~TransparentTransportWrapper();

    void connect(int slave_id) override;
    void disconnect() override;

    bool readHoldingRegisters(int address, int count, std::vector<uint16_t>& out) override;
    bool readInputRegisters(int address, int count, std::vector<uint16_t>& out) override;

    bool writeRegisters(int address, const std::vector<uint16_t>& data) override;
    bool writeCoil(int address, bool value) override;

    std::string info() const override;
    void setSlaveId(int id) const override;

private:
    std::string ip_;
    int port_;
    mutable int slave_id_;
    mutable std::mutex mutex_;

    boost::asio::io_service io_;
    boost::asio::ip::tcp::socket socket_;

    bool readRegisters(uint8_t function_code, int address, int count, std::vector<uint16_t>& out);
};

#endif  // TRANSPARENT_TRANSPORT_WRAPPER_HPP
