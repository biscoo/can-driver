
#define FDCAN_ELEMENT_MASK_EXTID ((uint32_t)0x1FFFFFFFU) /* Extended Identifier*/
#define FDCAN_ELEMENT_MASK_XTD   ((uint32_t)0x40000000U) /* Extended Identifier*/
#define FDCAN_ELEMENT_MASK_STDID ((uint32_t)0x1FFC0000U) /* Standard Identifier         */
#define FDCAN_ELEMENT_MASK_DLC   ((uint32_t)0x000F0000U) /* Data Length Code */

#define FDCAN_RXF0S_F0FL_Pos      (0U)                                         
#define FDCAN_RXF0S_F0FL_Msk      (0x7FU << FDCAN_RXF0S_F0FL_Pos)              /*!< 0x0000007F */
#define FDCAN_RXF0S_F0FL          FDCAN_RXF0S_F0FL_Msk   

#pragma once
#include <tuple>

enum class CanSpeed
{
    SPEED_1Mbps,
    SPEED_500Kbps,
    SPEED_250Kbps,
    SPEED_125Kbps,
};


/**
 * Initializes the CAN driver
 * \param speed the CAN speed
 */
void canDriverInit(CanSpeed bitRate);

/**
 * Send a message through the CAN bus.
 * Blocking function, retuns after the message has been sent
 * Function can be called by multiple threads
 * \param address CAN address
 * \param message pointer to an array with the bytes of the message
 * \param size size of message
 * \return the number of bytes sent, or a negative number in case of errors
 */
int canDriverSend(unsigned int id, const void *message, int size);

/**
 * Receive a message from the CAN bus.
 * Blocking function, retuns after the message has been received
 * Function can be called by multiple threads
 * \param message pointer to an array with the bytes of the message
 * \param size size of message
 * \return the number of bytes received, or a negative number in case of errors
 */
std::tuple<int, unsigned int> canDriverReceive(void *message, int size);
