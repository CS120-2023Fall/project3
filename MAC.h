#pragma once
#include<deque>
#include<assert.h>
#include<JuceHeader.h>
#include<chrono>
#include<cstdlib>
#include "receiver_transfer.h"
//#include "transmitter.h"

/////////////////////////////////
// set these macros properly!///
/////////////////////////////////
#define NUM_HEADER_BITS 8
#define NUM_DEST_BITS 3
#define NUM_SRC_BITS 3
#define NUM_TYPE_BITS 2
#define MY_MAC_ADDRESS 0b001
#define RECEND_THRESHOLD 8
// milisecond
#define ACK_TIME_OUT_THRESHOLD 1e-2

//enum class Rx_Frame_Received_Type {
//    still_receiving = -1,
//    error = 0,
//    valid_ack = 0b01,
//    valid_data = 0b10
//};

class MAC_Layer {
public:
    MAC_Layer() = default;
    MAC_Layer(juce::Label *labels[], int num_labels) {
        if (num_labels > 5) {
            assert(0);
        }
        for (int i = 0; i < num_labels; ++i) {
            mes[i] = labels[i];
        }
    }

    ~MAC_Layer() {
        delete receiver;
        delete tranmitter;
    }

private:
    enum class MAC_States_Set {
        Idle,
        CarrierSense,
        RxFrame,
        TxFrame,
        TxACK,
        ACKTimeout,
        LinkError,
        debug_error
    };

public:
    MAC_States_Set macState{MAC_States_Set::Idle};
    bool TxPending{ true };
    //std::deque<int> received_data;
    void refresh_MAC(const float *inBuffer, float *outBuffer, int num_samples);

private:
    int mac_address{ MY_MAC_ADDRESS };
    // array of pointers to send message
    juce::Label *mes[5]{ nullptr };
    // the number of resending times
    int resend{ 0 };
    // ack time out detect
    // std::chrono::steady_clock::now() 
    std::chrono::time_point<std::chrono::steady_clock> beforeTime_ack;
    bool ackTimeOut_valid{ false };
    // exponent of the backoff. 2^m - 1, millisecond
    int backoff_exp{ 0 };
    std::chrono::time_point < std::chrono::steady_clock> beforeTime_backoff;
    Receiver *receiver = new Receiver();
    Transfer *tranmitter = new Transfer();
};

void MAC_Layer::refresh_MAC(const float *inBuffer, float *outBuffer, int num_samples) {
    // for debug
    if (macState == MAC_States_Set::debug_error) {
        assert(0);
    }

    // deal with every state

    /// Idle
    if (macState == MAC_States_Set::Idle) {
        bool tmp = receiver->detect_frame(inBuffer, outBuffer, num_samples);
        // 1. detect preamble, invoke detect_frame()
        if (tmp)  {
            macState = MAC_States_Set::RxFrame;
            return;
        }
        // 2. ack time out
        if (ackTimeOut_valid) {
            auto currentTime = std::chrono::steady_clock::now();
            // milisecond
            double duration_millsecond = std::chrono::duration<double, std::milli>(currentTime - beforeTime_ack).count();
            if (duration_millsecond > ACK_TIME_OUT_THRESHOLD) {
                macState = MAC_States_Set::ACKTimeout;
                ackTimeOut_valid = false;
                return;
            }
        }
        // 3. send data
        auto currentTime = std::chrono::steady_clock::now();
        double duration_milisecond = std::chrono::duration<double, std::milli>(currentTime - beforeTime_backoff).count();
        // +, - first, then <<
        double backoff = (1 << backoff_exp) - 1;
        if (TxPending && (backoff == 0 || duration_milisecond > backoff)) {
            backoff_exp = 0;
            macState = MAC_States_Set::CarrierSense;
            return;
        }
    }
    /// RxFrame
    else if (macState == MAC_States_Set::RxFrame) {
        Rx_Frame_Received_Type tmp = receiver->decode_one_packet(inBuffer, outBuffer, num_samples);
        switch (tmp) {
            case Rx_Frame_Received_Type::error:
                macState = MAC_States_Set::Idle;
                return;
            case Rx_Frame_Received_Type::still_receiving:
                return;
            case Rx_Frame_Received_Type::valid_ack:
                ackTimeOut_valid = false;
                macState = MAC_States_Set::Idle;
                return;
            case Rx_Frame_Received_Type::valid_data:
                macState = MAC_States_Set::TxACK;
                return;             
        }
    }
    /// TxACK
    else if (macState == MAC_States_Set::TxACK) {
        bool feedback =  tranmitter->Add_one_packet(inBuffer, outBuffer, num_samples, Tx_frame_status::Tx_ack);
        if (!feedback) {
            macState = MAC_States_Set::Idle;
        }
        return;
    }
    /// CarrierSense
    else if (macState == MAC_States_Set::CarrierSense) {
        if (receiver->if_channel_quiet(inBuffer, num_samples)) {
            macState = MAC_States_Set::TxFrame;
            backoff_exp = 0;
            return;
        }
        else {
            backoff_exp = backoff_exp + 1 <= 10 ? (backoff_exp + 1) : 10;
            beforeTime_backoff = std::chrono::steady_clock::now();
            macState = MAC_States_Set::Idle;
            return;
        }
    }
    /// TxFrame
    else if (macState == MAC_States_Set::TxFrame) {
         // header (dest | src | type), (num packet order)
         //std::vector<bool> header = {}

         bool feedback = tranmitter->Add_one_packet(inBuffer, outBuffer, num_samples, Tx_frame_status::Tx_data);
         // transmition finishes
         if (!feedback) {
             macState = MAC_States_Set::Idle;
             ackTimeOut_valid = true;
             beforeTime_ack = std::chrono::steady_clock::now();
         }
    }
    /// ACKTimeout
    else if (macState == MAC_States_Set::ACKTimeout) {
        if (resend > RECEND_THRESHOLD) {
            macState = MAC_States_Set::LinkError;
            return;
        }
        else {
            ++resend;
            // set backoff after ack timeout
            // [1, 8]
            backoff_exp = rand() % 8 + 1;
            beforeTime_backoff = std::chrono::steady_clock::now();
            macState = MAC_States_Set::Idle;
            return;
        }
    }
    /// LinkError
    else if (macState == MAC_States_Set::LinkError) {
        assert(false);
    }
}