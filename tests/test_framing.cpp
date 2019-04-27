/*
 * Copyright (c) 2016 UAVCAN Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Contributors: https://github.com/UAVCAN/libcanard/contributors
 */


#include <catch.hpp>
#include "canard.h"

static bool acceptAllTransfers(const CanardInstance*,
                              uint16_t,
                              CanardTransferType,
                              uint8_t)
{
    return true;
}

static void onTransferReceptionMock(CanardInstance*,
                                    CanardRxTransfer*)
{
}

TEST_CASE("Framing, SingleFrameBasicCan2")
{
    uint8_t node_id = 22;
    uint8_t toggle_bit = 5;
    uint8_t eof_bit = 6;
    uint8_t sof_bit = 7;

    std::uint8_t memory_arena[1024];
    ::CanardInstance ins;

    uint16_t subject_id = 12;
    uint8_t transfer_id = 2;
    uint8_t transfer_priority = CANARD_TRANSFER_PRIORITY_NOMINAL;
    uint8_t data[] = {1, 2, 3};
    
    canardInit(&ins,
               memory_arena,
               sizeof(memory_arena),
               onTransferReceptionMock,
               acceptAllTransfers,
               reinterpret_cast<void*>(12345));
    canardSetLocalNodeID(&ins, node_id);

    auto res = canardPublishMessage(&ins,
                         subject_id,
                         &transfer_id,
                         transfer_priority,
                         data,
                         sizeof(data));
    REQUIRE(res >= 0);

    // compenaste for internally incrementing transfer_id in canardPublishMessage
    transfer_id--;

    auto transfer_frame = canardPeekTxQueue(&ins);
    REQUIRE(transfer_frame != nullptr);
    canardPopTxQueue(&ins);

    // Only checks framing in this test
    REQUIRE(transfer_frame->data_len == 4);
    REQUIRE(transfer_frame->data[0] == 1);
    REQUIRE(transfer_frame->data[1] == 2);
    REQUIRE(transfer_frame->data[2] == 3);                 
    REQUIRE(transfer_frame->data[3] == ((1 << sof_bit) | (1 << eof_bit) | (1 << toggle_bit) | transfer_id));

    REQUIRE(canardPeekTxQueue(&ins) == nullptr);               
}

TEST_CASE("Deframing, SingleFrameBasicCan2")
{
    uint8_t node_id = 22;

    uint8_t toggle_bit = 5;
    uint8_t eof_bit = 6;
    uint8_t sof_bit = 7;

    uint8_t transfer_priority_bit = 24;
    uint8_t subject_id_bit = 8;
    uint8_t node_id_bit = 1;


    std::uint8_t memory_arena[1024];
    ::CanardInstance ins;

    uint16_t subject_id = 12;
    uint8_t transfer_id = 2;
    uint8_t transfer_priority = CANARD_TRANSFER_PRIORITY_NOMINAL;
    uint8_t data[] = {1, 2, 3};
    

    auto onTransferReception = [](CanardInstance*, CanardRxTransfer* transfer)
    {
        // Only check (de)framing in this test
        REQUIRE(transfer->payload_len == 3);

        uint8_t out_value = 0;
        canardDecodePrimitive(transfer, 0, 8, false, &out_value);
        REQUIRE(out_value == 1);

        canardDecodePrimitive(transfer, 8, 16, false, &out_value);
        REQUIRE(out_value == 2);

        canardDecodePrimitive(transfer, 16, 24, false, &out_value);
        REQUIRE(out_value == 3);
    };

    canardInit(&ins,
               memory_arena,
               sizeof(memory_arena),
               onTransferReception,
               acceptAllTransfers,
               reinterpret_cast<void*>(12345));

    CanardCANFrame frame = {
        .id = (static_cast<uint32_t>(CANARD_CAN_FRAME_EFF) 
            | static_cast<uint32_t>(transfer_priority << transfer_priority_bit) 
            | static_cast<uint32_t>(subject_id << subject_id_bit) 
            | static_cast<uint32_t>(node_id << node_id_bit)
        ),
        .data = {1, 2, 3, static_cast<uint8_t>((1 << sof_bit) | (1 << eof_bit) | (1 << toggle_bit) | transfer_id)},
        .data_len = 4,
    };

    auto res = canardHandleRxFrame(&ins,
                                   &frame,
                                   0);
    REQUIRE(res >= 0);

}

