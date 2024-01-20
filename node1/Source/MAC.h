#pragma once
#include<deque>
#include<assert.h>
#include<JuceHeader.h>
#include<chrono>
#include<cstdlib>
#include <float.h>
#include "receiver_transfer.h"
#include "macros.h"


// millisecond
#define ACK_TIME_OUT_THRESHOLD 1000
#define RESEND_THRESHOLD 100
double RTT = 30;

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
    };

    ~MAC_Layer() {
    }
    // update MAC states
    void refresh_MAC(const float *inBuffer, float *outBuffer, int num_samples);
    //bool test_crc();
    // prepare for next packet
    void Start() {
        macState = MAC_States_Set::Idle;
        receiver.Initialize();
        transmitter.Initialize();
        resend = 0;
        ackTimeOut_valid = false;
        wait = false;
        backoff_exp = 0;
        startTransmitting = START_TRANS_FIRST;
        start_time = std::chrono::steady_clock::now();
        TxPending = true;
    }
    
    //void reset_receiving_info();
    void STOP() {
        receiver.Write_symbols();

        //char t[50000 / 8];
        //int read_t = 0;
        //std::ofstream o("received_binary.bin", std::ios::binary | std::ios::out);
        //char byte = 0;
        //for (int i = 0; i < receiver.received_bits.size(); ++i) {
        //    // receiver.received_bits
        //    byte = (byte << 1) + receiver.received_bits[i];
        //    //std::cout << (int)byte << std::endl;
        //    if ((i + 1) % 8 == 0) {
        //        t[read_t++] = byte;
        //        byte = 0;
        //    }
        //}
        //o.write(t, 6250);
        //o.close();
    }

public:
    enum class MAC_States_Set {
        Idle,
        CarrierSense,
        RxFrame,
        TxFrame,
        TxACK,
        ACKTimeout,
        LinkError,
        ping2_detecting,
        ping2_reply
    };

    std::chrono::time_point<std::chrono::steady_clock> start_time;
    MAC_States_Set macState{MAC_States_Set::Idle};
    bool TxPending{ false };
    std::deque<int> received_data;    
    bool wait = false; // wait for ack
    int start_for_wait_sample=0;
    bool startTransmitting{ START_TRANS_FIRST };

private:
    int mac_address{ MY_MAC_ADDRESS };
    juce::Label *mes[5]{ nullptr }; // array of pointers to send message
    int resend{ 0 }; // the number of resending times
    // ack time out detect
    // std::chrono::steady_clock::now()
    std::chrono::time_point<std::chrono::steady_clock> beforeTime_ack;
    bool ackTimeOut_valid{ false };    
    int backoff_exp{ 0 }; // exponent of the backoff. 2^m - 1, millisecond
    std::chrono::time_point < std::chrono::steady_clock> beforeTime_backoff{ std::chrono::steady_clock::now() };
    std::chrono::time_point<std::chrono::steady_clock> beforeTime_carrier_sense{ std::chrono::steady_clock::now() };
public:
    Receiver receiver;
    Transfer transmitter;
};
void KeepSilence(const float* inBuffer, float* outBuffer, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        outBuffer[i] = 0;
    }
}

void MAC_Layer::refresh_MAC(const float *inBuffer, float *outBuffer, int num_samples) {
    if (macState == MAC_States_Set::ping2_detecting) {
        // catch packet with pcap


        return
    }
    else if (macState == MAC_States_Set::ping2_reply) {

    }




    /// Idle
    if (macState == MAC_States_Set::Idle) {
        //std::cout << "idle" << std::endl;
        do {
            /// Detect preamble, invoke detect_frame()
            bool tmp = receiver.detect_frame(inBuffer, outBuffer, num_samples);
            if (tmp) {
                mes[3]->setText("preamble detected " + std::to_string(receiver.received_packet) + ", " + std::to_string(transmitter.transmitted_packet),
                    juce::NotificationType::dontSendNotification);
                macState = MAC_States_Set::RxFrame;
                std::cout << "detect_frame" << std::endl;
                // The computer has received a packet. It can start to transmit.
                startTransmitting = true;
                
                // It must return due to the implementation of detect_frame().
                return;
            }

            ///  Ack time
            //auto currentTime = std::chrono::steady_clock::now();
            //if (ackTimeOut_valid) {
            //    // millisecond
            //    double duration_millisecond = std::chrono::duration<double, std::milli>(currentTime - beforeTime_ack).count();
            //    if (duration_millisecond > ACK_TIME_OUT_THRESHOLD) {
            //        macState = MAC_States_Set::ACKTimeout;//resend the package
            //        ackTimeOut_valid = false;
            //        break;
            //    }
            //}

            ///  Send data
            if (TxPending) {
                double duration_millisecond = DBL_MAX;
                //double duration_millisecond = std::chrono::duration<double, std::milli>(currentTime - beforeTime_backoff).count();
                // +, - are prior to <<
                double backoff = (1 << backoff_exp) - 1;
                if (!CSMA_ONLY_RECEIVE && (backoff == 0 || duration_millisecond >= backoff)) {
                    backoff_exp = 0;
                    macState = MAC_States_Set::CarrierSense;
                    beforeTime_carrier_sense = std::chrono::steady_clock::now();
                }
                break;
            }
        } while (0); // end of do while 0
    }
    /// RxFrame
    if (macState == MAC_States_Set::RxFrame) {
        Rx_Frame_Received_Type tmp = receiver.decode_one_packet(inBuffer, outBuffer, num_samples, transmitter.transmitted_packet);

        std::cout << "received packet type: " << (int)tmp << std::endl;
        switch (tmp) {
            case Rx_Frame_Received_Type::error:
                macState = MAC_States_Set::Idle;
                return;
            case Rx_Frame_Received_Type::still_receiving:
                return;
            case Rx_Frame_Received_Type::valid_ack:
                ackTimeOut_valid = false;
                wait = false;
                transmitter.transmitted_packet += 1; //the next status transmit the next packet
                macState = MAC_States_Set::Idle;
                mes[2]->setText("transmitted packet: " + std::to_string(transmitter.transmitted_packet), 
                    juce::NotificationType::dontSendNotification);
                backoff_exp = rand() % 3 + 5;
                resend = 0;
                return;
            case Rx_Frame_Received_Type::valid_data: {
                macState = MAC_States_Set::TxACK;
                receiver.received_packet += 1;
                bool feedback = transmitter.Add_one_packet(inBuffer, outBuffer, num_samples,
                    Tx_frame_status::Tx_ack, transmitter.url_bits, receiver.received_packet);
                mes[1]->setText("received packet: " + std::to_string(receiver.received_packet), juce::dontSendNotification);
                break; 
            }
            case Rx_Frame_Received_Type::repeated_data:
                macState = MAC_States_Set::TxACK;
                transmitter.Add_one_packet(inBuffer, outBuffer, num_samples,
                    Tx_frame_status::Tx_ack, transmitter.url_bits, receiver.received_packet, receiver.repeated_packet_num);
                mes[1]->setText("Packet received: " + std::to_string(receiver.received_packet), juce::dontSendNotification);
                break;
        }// end of switch
    }
    /// TxACK
    if (macState == MAC_States_Set::TxACK) {
        // Sending ack has no backoff, do immediately.

        //auto currentTime = std::chrono::steady_clock::now();
        //double duration_milisecond = std::chrono::duration<double, std::milli>(currentTime - beforeTime_backoff).count();
        // +, - first, then << so use ()
        //double backoff = (1 << backoff_exp) - 1;
        //if (duration_milisecond <= backoff) {
        //    return;
        //}
        std::cout << "sending ack" << std::endl;
        bool finish = transmitter.Trans(inBuffer, outBuffer, num_samples);
        if (finish) {
            backoff_exp = rand() % 2 + 5;
            macState = MAC_States_Set::Idle;
            std::cout << "finish sending ack" << std::endl;
        }
        return;
    }
    /// CarrierSense
    if (macState == MAC_States_Set::CarrierSense) {
        macState = MAC_States_Set::TxFrame;
        transmitter.Add_one_packet(inBuffer, outBuffer, num_samples, Tx_frame_status::Tx_data, transmitter.url_bits);
        //if (receiver.if_channel_quiet(inBuffer, num_samples)) {
        //    double duration = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - beforeTime_carrier_sense).count();
        //    // Sense a full rtt.
        //    if (duration > RTT) {
        //        macState = MAC_States_Set::TxFrame;
        //        transmitter.Add_one_packet(inBuffer, outBuffer, num_samples, Tx_frame_status::Tx_data);
        //    }
        //}
        //else {
        //    backoff_exp = rand() % 3 + 9;
        //    beforeTime_backoff = std::chrono::steady_clock::now();
        //    macState = MAC_States_Set::Idle;
        //    return;
        //}
    }
    /// TxFrame
    if (macState == MAC_States_Set::TxFrame) {
        bool finish= transmitter.Trans(inBuffer, outBuffer, num_samples);
        // transmit finishes
        if (finish) {
            beforeTime_ack = std::chrono::steady_clock::now();
            ackTimeOut_valid = true;
            macState = MAC_States_Set::Idle;
            wait = true;
            std::cout << "send num: " << transmitter.transmitted_packet << std::endl;
            TxPending = false;
        }
        return;
    }
    /// ACKTimeout
    if (macState == MAC_States_Set::ACKTimeout) {
        if (resend > RESEND_THRESHOLD) {
            macState = MAC_States_Set::LinkError;
        }
        else {
            ++resend;
            // set backoff after ack timeout
            // [3, 8]
            backoff_exp = rand() % 5 + 3;
            beforeTime_backoff = std::chrono::steady_clock::now();
            macState = MAC_States_Set::Idle;
            wait = false;
            return;
        }
    }
    /// LinkError
    if (macState == MAC_States_Set::LinkError) {
        std::cout << "link error" << std::endl;
        return;
    }
}