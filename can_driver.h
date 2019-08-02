
#define FDCAN_ELEMENT_MASK_EXTID ((uint32_t)0x1FFFFFFFU) /* Extended Identifier*/
#define FDCAN_ELEMENT_MASK_DLC   ((uint32_t)0x000F0000U) /* Data Length Code */

#pragma once
#include <tuple>


/**
 * Initializes the CAN driver
 * \param speed the CAN speed
 */
void canDriverInit(unsigned int speed);

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
