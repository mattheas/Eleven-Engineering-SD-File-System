/**
 * @file SDCard.cpp
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Implementation of SD card driver
 * @version 0.1
 * @date 2024-03-01
 */

#include "../inc/SDCard.h"

using namespace sd_driver;

SDCard::SDCard(const bool &configure_spi1)
{
    if (configure_spi1 == true)
    {
        SPI_set_config_optimal(_49_152_MHz, SPI1);
    }

    // Set pin PD3 as an output, it is the Chip Select(CS) for the SD cards SPI
    // interface. Pins are set as an output by setting a 1 in the position N+8,
    // where N is the GPIO pin number
    gpio_set_config((0x03 << 8), GPIO_D);
}

SDCard::~SDCard()
{
}

SDCard::initialization_result_t SDCard::get_initialization_result() const
{
    return initialization_result;
}

SDCard::BootSectorInformation SDCard::get_boot_sector_information() const
{
    return boot_sector_information;
}

SDCard::initialization_result_t SDCard::initialize_sd_card()
{
    const uint16_t max_number_cmd0_commands_sent = 10U;

    // write dummy value to SPI while CS is inactive/HIGH for at least 74 clock cycles
    gpio_write(CS_INACTIVE_HIGH, GPIO_D);
    for (uint16_t i = 0; i < 20; i++)
    {
        SPI_write(0xFF, SPI1);
    }

    // assert CS to start communication
    gpio_write(CS_ACTIVE_LOW, GPIO_D);

    bool valid_cmd0_response = false;
    for (int i = 0; i < max_number_cmd0_commands_sent; i++)
    {
        if(send_cmd0(NUM_INVALID_RESPONSE_LIMIT_SPI_READ))
        {
            valid_cmd0_response = true;
            break;
        }
        
    }

    // de-assert CS to end communication
    gpio_write(CS_INACTIVE_HIGH, GPIO_D);
    SPI_write(0xFF, SPI1);

    // Stop initialization process if no or invalid response is received from CMD0
    if (valid_cmd0_response == false)
    {
        return initialization_result_t::INIT_FAILED_ON_CMD0;
    }

    return initialization_result_t::INIT_SUCCESS;
}

bool SDCard::read_boot_sector_information()
{
    return false;
}

bool SDCard::send_cmd0(const uint16_t &num_invalid_response_limit) const
{
    // Send 6-byte CMD0 command “40 00 00 00 00 95” to put the card in SPI mode
    SPI_write(0x40, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x95, SPI1); // CRC7

    for (uint16_t i = 0; i < num_invalid_response_limit; i++)
    {
        uint16_t spi_read_value = SPI_read(SPI1);
        if (spi_read_value == SD_CARD_IN_IDLE_MODE_RESPONSE)
        {
            return true; // valid response, return true
        }
    }

    return false; // no valid response, return false
}
