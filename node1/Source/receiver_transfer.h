#pragma once
#include<deque>
#include <vector>
#include<iomanip>
#include<string>
#include "transmitter.h"
#include "macros.h"



/// //////////////////////////
///  set the macros appropriately!!!
#define QUIET_THRESHOLD 1.0

enum class  Frame_Type {
    ack = 0b01,
    data = 0b10
};
enum  Rx_Frame_Received_Type {
    still_receiving = -1,
    error = 0,
    valid_ack = 1,
    valid_data = 2,
    repeated_data = 3
};


class Receiver {
private:
    int matched_preamble_len{ 0 };
    bool header_processed{ false };
    bool repeated_data_flag{ false };
public:
    //std::vector<double> receive_buffer = empty;
    std::deque<double> sync_buffer;
    std::deque<double> decode_buffer;
    std::deque<int>received_bits; // Decoded bits. Received message.
    std::vector<int> preamble;
    std::vector<int> check_crc_bits; // 0 or 1 int
    int start_index = -1;
    unsigned int received_packet = 0;
    int repeated_packet_num{ -1 };
    int data_part_len{ 0 };

    Receiver() :preamble(default_trans_wire.preamble), check_crc_bits(NUM_CRC_BITS_PER_PACKET, 0) {
        Initialize();
    }

    void Initialize() {
        sync_buffer = std::deque<double>(280, 0.0);
        received_bits.clear();
        received_packet = 0;
        decode_buffer.clear();
        matched_preamble_len = 0;
        header_processed = false;
        repeated_data_flag = false;
        repeated_packet_num = -1;
    }
    void Write_symbols() {
        Write("project2_bits_receiver.txt", received_bits);
    }

    // decode a NUM_SAMPLES_PER_BIT samples
    // return a bit, 0 or 1.
    /// sample_buffer: The function gets samples from this buffer
    /// start_index: start at which position to decode
    /// after_end_index: position after the end position
    int decode_a_bit(const std::vector<double> &sample_buffer, int start_index) {
        // 0
        if (sample_buffer[start_index] + sample_buffer[start_index + 1] < 0 &&
            sample_buffer[start_index + 2] + sample_buffer[start_index + 3] > 0) {
            return 0;
        }
        // 1
        else {
            return 1;
        }
    }
    int decode_a_bit(const std::deque<double> &sample_buffer, int start_index) {
        // 0
        if (sample_buffer[start_index] + sample_buffer[start_index + 1] < 0 &&
            sample_buffer[start_index + 2] + sample_buffer[start_index + 3] > 0) {
            return 0;
        }
        // 1
        else {
            return 1;
        }
    }
    int decode_a_bit(const float *sample_buffer, int start_index) {
        // 0
        if (sample_buffer[start_index] + sample_buffer[start_index + 1] < 0 &&
            sample_buffer[start_index + 2] + sample_buffer[start_index + 3] > 0) {
            return 0;
        }
        // 1
        else {
            return 1;
        }
    }
        // inBuffer: input ,outBuffer: not used ,num_samples: sample from input
        // try to read the num_samples sample from inBuffer,if the accumulated buffer still cannot achieve a packet,return still_receiving,if it satisfies a packet samples
        // return whether it is valid_ack,valid_data,or error,if valid_data,push the translated data symbols into symbol_code
        //-1  CRC_ERROR, 
        // 0 still RX_frame--still decoding,
        // 1 VALID_ACK,
        // 2 VALID_DATA
    Rx_Frame_Received_Type decode_one_packet(const float *inBuffer, float *outBuffer, int num_samples, int transmitted_packet = 0) {
        for (int i = 0; i < num_samples; i++) {
            decode_buffer.push_back(inBuffer[i]);

            // process header.
            if (!header_processed && decode_buffer.size() >= NUM_MAC_HEADER_BITS * NUM_SAMPLES_PER_BIT) {
                std::vector<int> header_vec;
                // j is the bit index
                for (int j = 0; j < NUM_MAC_HEADER_BITS; ++j) {
                    header_vec.emplace_back(decode_a_bit(decode_buffer, j * NUM_SAMPLES_PER_BIT));
                }
                int dest = (header_vec[0] << 2) + (header_vec[1] << 1) + header_vec[2];
                int src = (header_vec[3] << 2) + (header_vec[4] << 1) + header_vec[5];
                int type = (header_vec[6] << 1) + header_vec[7];
                std::cout << "dest, src, type: " << dest << src << type << std::endl;
                int packet_num = 0;
                for (int j = NUM_DEST_BITS + NUM_SRC_BITS + NUM_TYPE_BITS, offset = PACKET_NUM_BITS - 1;
                    j < NUM_DEST_BITS + NUM_SRC_BITS + NUM_TYPE_BITS + PACKET_NUM_BITS; ++j, --offset) {
                    packet_num += header_vec[j] << offset;
                }
                std::cout << "packet num: " << packet_num << std::endl;
                std::cout << "received packet: " << received_packet << std::endl;
                std::cout << "== data? " << (Frame_Type(type) == Frame_Type::data) << std::endl;
                // packet error
                if (dest != MY_MAC_ADDRESS || (Frame_Type(type) != Frame_Type::ack && Frame_Type(type) != Frame_Type::data)) {
                    std::cout << "error packet" << std::endl;
                    std::cout << decode_buffer.size() << std::endl;
                    //Write("decode_log.txt", decode_buffer);
                    decode_buffer.clear();
                    return error;
                }
                // data len
                unsigned len = 0;
                for (int i = NUM_MAC_HEADER_BITS - NUM_DATE_LEN_BITS ; i < NUM_MAC_HEADER_BITS; ++i) {
                    len = (len << 1) + (unsigned)header_vec[i];
                }
                data_part_len = (int)len;

                // ack
                if (Frame_Type(type) == Frame_Type::ack) {
                    if (packet_num != transmitted_packet) {
                        return error;
                    }
                    std::cout << "exit after receiving ack" << std::endl;
                    //Write("decode_log.txt", decode_buffer);
                    decode_buffer.clear();
                    return valid_ack;
                }
                // data
                else if (Frame_Type(type) == Frame_Type::data) {
                    if ((unsigned)packet_num < received_packet) {
                        //decode_buffer.clear();
                        repeated_data_flag = true;
                        repeated_packet_num = packet_num;
                        std::cout << "repeated" << std::endl;
                    }
                    else if ((unsigned)packet_num > received_packet) {
                        decode_buffer.clear();
                        return error;
                    }
                    header_processed = true;
                    
                }
            }
            // decode data and crc
            if (header_processed && decode_buffer.size() >= (NUM_MAC_HEADER_BITS + data_part_len) * NUM_SAMPLES_PER_BIT) {
                if (repeated_data_flag) {
                    decode_buffer.clear();
                    header_processed = false;
                    repeated_data_flag = false;
                    return repeated_data;
                }
                // start_position: Data part start position. bit index, remember x4
                int start_position = NUM_MAC_HEADER_BITS;
                //std::deque<int> received_bits_tmp;
                for (int bit_index = start_position; bit_index < start_position + data_part_len; ++bit_index) {
                    //received_bits_tmp.emplace_back(decode_a_bit(decode_buffer, bit_index * 4));
                    received_bits.emplace_back(decode_a_bit(decode_buffer, bit_index * 4));
                }
                // decode crc
                //for (int bit_index = start_position + NUM_PACKET_DATA_BITS, i = 0; 
                //    bit_index < start_position + NUM_PACKET_DATA_BITS + NUM_CRC_BITS_PER_PACKET; ++bit_index, ++i) {
                //    check_crc_bits[i] = decode_a_bit(decode_buffer, bit_index * 4);
                //}

                //Write("received_tmp.txt", received_bits_tmp);
                //FILE *file = fopen("crc_tmp.txt", "w");
                //for (int i = 0; i < 320; ++i) {
                //    fprintf(file, "%d ", check_crc_bits[i]);
                //    if ((i + 1) % 32 == 0) {
                //        fprintf(file, "\n");
                //    }
                //}
                //fclose(file);

                //exit(1);

                // calculate and check crc
                bool crc_correct = true;

                //int received_tmp_read_count = 0;  // count for bits
                //char bytes_for_calculation[63] = { 0 };  // for 500 bits
                //int received_crc_read_count = 0;  // a reader for check_crc_bits[], the received crc part.
                //// check first 504* 9 = 4536 bits
                //char tmp_byte = 0;
                //for (int tmp_bits_group_start = 0; tmp_bits_group_start + 504 < NUM_PACKET_DATA_BITS && crc_correct; 
                //    tmp_bits_group_start += 504) {
                //    for (int i = 0; i < 504; ++i) {
                //        tmp_byte = (tmp_byte << 1) + (char)received_bits_tmp[i + tmp_bits_group_start];
                //        if ((i + 1) % 8 == 0) {
                //            bytes_for_calculation[i / 8] = tmp_byte;
                //            tmp_byte = 0;
                //        }
                //    }  // a group of bytes is ready to calculate crc
                //    std::uint32_t crc = CRC::CalculateBits(bytes_for_calculation, sizeof(bytes_for_calculation) * 8, CRC::CRC_32());
                //    //printf("crc: %x\n", crc);
                //    //std::cout <<"crc: " << std::hex << crc << std::endl;
                //    // Compare with received crc bits.
                //    for (int i = 31; i >= 0; --i) {
                //        if ((crc >> i & 1) != (std::uint32_t)check_crc_bits[received_crc_read_count++]) {
                //            crc_correct = false;
                //            std::cout << "crc error1 at: " << (received_crc_read_count - 1) << std::endl;
                //            break;
                //        }
                //    }
                //} // end of first 504 * 9 bits
                //// calculate the last 464 bits
                //tmp_byte = 0;
                //for (int i = 504 * 9; i < 5000 && crc_correct; ++i) {
                //    tmp_byte = (tmp_byte << 1) + (char)received_bits_tmp[i];
                //    if ((i + 1) % 8 == 0) {
                //        bytes_for_calculation[(i - 504 * 9) / 8] = tmp_byte;
                //        tmp_byte = 0;
                //    }
                //}
                //std::uint32_t crc = CRC::CalculateBits(bytes_for_calculation, 464, CRC::CRC_32());
                //for (int i = 31; i >= 0 && crc_correct; --i) {
                //    if ((crc >> i & 1) != (std::uint32_t)check_crc_bits[received_crc_read_count++]) {
                //        crc_correct = false;
                //        std::cout << "crc error2 at: " << (received_crc_read_count - 1) << std::endl;
                //        break;
                //    }
                //}  // end of processing last 464 bits

/////////////////////////////  delete me ////////////////////
                //crc_correct = true;
//////////////////////////////////////////////////////////

                //if (crc_correct) {
                //    for (auto &i : received_bits_tmp) {
                //        received_bits.emplace_back(i);
                //    }
                //}

                decode_buffer.clear();
                header_processed = false;
                if (!crc_correct) {
                    std::cout << "crc not passed" << std::endl;
                    return error;
                }

                std::cout << "exit after receiving data" << std::endl;
                std::string t(received_bits.size() / 8, (char)0);
                char tmp = 0;
                for (int i = 0; i < (int)received_bits.size(); ++i) {
                    tmp = (tmp << 1) + received_bits[i];
                    if ((i + 1) % 8 == 0) {
                        t[i / 8] = tmp;
                    }
                }
                received_bits.clear();
                std::cout << "received url: " << t << std::endl;                
                
                return valid_data;
            }  // end of processing data/ack
        }
        return still_receiving;

    }
    
    /// outBuffer is not used,inBuffer is the input ,read num_samples sample from input
    bool detect_frame(const float *inBuffer, float *outBuffer, int num_samples) {
        bool detected = false;
    
        int sync_buffer_size = sync_buffer.size();
        for (int i = 0; i < num_samples; ++i) {
            sync_buffer.pop_front();
            sync_buffer.emplace_back(inBuffer[i]);
            // compare with preamble
            for (int j = 0; j <= sync_buffer_size - 4; j += 4) {
                if (decode_a_bit(sync_buffer, j) == preamble[matched_preamble_len]) {
                    ++matched_preamble_len;
                }
                else {
                    matched_preamble_len = 0;
                    break;
                }
                if (matched_preamble_len == 64) {
                    detected = true;
                }
                if (detected && j + 4 <= sync_buffer_size - 1) {
                    for (int k = j + 4; k < sync_buffer_size; ++k) {
                        decode_buffer.emplace_back(sync_buffer[k]);
                    }
                    break;
                }
            }
            if (detected) {
                for (int j = i + 1; j < num_samples; ++j) {
                    decode_buffer.emplace_back(inBuffer[j]);
                }
                for (auto element : sync_buffer) {
                    element = 0.0;
                }
                break;
            }
        }
        
        return detected;
    }

    // detect if the channel dose not has either jamming noise or data.
    bool if_channel_quiet(const float *inBuffer, int num_samples) {
        // compare the sum of power with the threshold
        float absolutePowerSum = 0;

        for (int i = 0; i < num_samples; ++i) {
            absolutePowerSum += abs(inBuffer[i]);
        }
        if (absolutePowerSum < QUIET_THRESHOLD)
            return true;
        else
            return false;
    }
};

enum Tx_frame_status {
    Tx_ack = 0,
    Tx_data = 1
};


class Transfer {
public:
    Transfer() : preamble(default_trans_wire.preamble) {
        Initialize();
        std::string url_string(URL);
        for (char & c : url_string) {
            for (int i = 7; i >= 0; --i) {
                url_bits.emplace_back(c >> i & 1);
            }
        }
    }
    void Initialize() {
        transfer_num = 0;
        transmitting_buffer.clear();
        transmitted_packet = 0;
    }
    std::deque<double> transmitting_buffer;
    std::vector<bool>bits = default_trans_wire.bits;
    std::vector<int> preamble;
    std::deque<unsigned> url_bits;  // binary url
    int transfer_num = 0;
    int transmitted_packet = 0;


    // convert a bit to 4 samples and add the samples to the end fo dest.
    void add_samples_from_a_bit(std::deque<double> &dest, int bit) {
        if (bit == 0) {
            dest.emplace_back(-0.95);
            dest.emplace_back(-0.95);
            dest.emplace_back(0.95);
            dest.emplace_back(0.95);
        }
        else if (bit == 1) {
            dest.emplace_back(0.95);
            dest.emplace_back(0.95);
            dest.emplace_back(-0.95);
            dest.emplace_back(-0.95);
        }
    }

    /// inBuffer ,outBuffer and num_samples is not used,status indicate ack or data you want to add,it will add to the transmittion_buffer 
    /// The return value is not used.
    bool Add_one_packet(const float *inBuffer, float *outBuffer, int num_samples, Tx_frame_status status,const std::deque<unsigned> &bits_to_transmit,
        unsigned int received_packet = 1, int repeated_packet_num = -1) {
        // set these constants properly
        constexpr int data_bits_in_a_packet = NUM_PACKET_DATA_BITS;

            // add preamble
            for (int i = 0; i < preamble.size(); ++i) {
                add_samples_from_a_bit(transmitting_buffer, preamble[i]);
            }

            // Add destination, source, type, packet number(start from 0).
            int concatenate = (OTHER_MAC_ADDRESS << 5) + (MY_MAC_ADDRESS << 2)
                + (status == Tx_data ? (int)Frame_Type::data : (int)Frame_Type::ack);
            concatenate <<= PACKET_NUM_BITS;
            if (status == Tx_data) {
                concatenate += transmitted_packet;
            }
            else {
                if (repeated_packet_num != -1) {
                    concatenate += repeated_packet_num;
                }
                else {
                    concatenate += received_packet - 1;
                }
            }
            for (int i = NUM_DEST_BITS + NUM_SRC_BITS + NUM_TYPE_BITS + PACKET_NUM_BITS - 1; i >= 0; --i) {
                int bit = concatenate >> i & 1;
                if (bit == 1) {
                    add_samples_from_a_bit(transmitting_buffer, bit);
                }
                // 0
                else {
                    add_samples_from_a_bit(transmitting_buffer, bit);
                }                
            }

            // data length, 16 bits
            int len = static_cast<unsigned> (bits_to_transmit.size());
            for (int i = 0; i < NUM_DATE_LEN_BITS; ++i) {
                add_samples_from_a_bit(transmitting_buffer, (len >> (NUM_DATE_LEN_BITS - 1 - i) & 1));
            }
            ////////////////////////////////////////////////////
            // data
            if (status == Tx_data) {
                for (int i = 0; i < (int)bits_to_transmit.size(); ++i) {
                    add_samples_from_a_bit(transmitting_buffer, (int)bits_to_transmit[i]);
                }
                //// add crc
                //for (int i = 0; i < 10; ++i) {
                //    unsigned crc_t = default_trans_wire.crc_32_t[transmitted_packet * 10 + i];
                //    for (int j = 31; j >= 0; --j) {
                //        int bit = (crc_t >> j & 1);
                //        add_samples_from_a_bit(transmitting_buffer, bit);
                //    }
                //}
            }
            else if (status == Tx_ack) {
                unsigned tmp = 0b1110;
                for (int i = 0; i < 500; ++i) {
                    // random content
                    add_samples_from_a_bit(transmitting_buffer, (tmp >> (i % 4) & 1) );
                }
            }        
        return true;
    }

    //inBuffer is not used,outBuffer is used to read from tranmittion_buffer,and read the num_samples sample
    //trans finished then return true
    bool Trans(const float *inBuffer, float *outBuffer, int num_samples)
    {
        bool silence = false;

            for (int i = 0; i < num_samples; i++) {
                if (transfer_num >= transmitting_buffer.size()) {
                    silence = true;
                    outBuffer[i] = 0;
                }
                else {
                    outBuffer[i] = (float)transmitting_buffer[transfer_num];
                    transfer_num++;
                }
            }
            if (transfer_num >= transmitting_buffer.size()) {
                transfer_num = 0;
                transmitting_buffer.clear();
            }
            Write("transmitting_buffer.txt", transmitting_buffer);
            return silence;
    }
};