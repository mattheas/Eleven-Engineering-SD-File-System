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

SDCard::SDCardInformation SDCard::get_sd_card_information() const
{
    return sd_card_information;
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

    // CMD0
    //================================================================================================================
    // assert CS to start communication
    gpio_write(CS_ACTIVE_LOW, GPIO_D);

    bool valid_cmd0_response = false;
    for (int i = 0; i < max_number_cmd0_commands_sent; i++)
    {
        sd_card_command_response_t cmd0_response = send_cmd0(NUM_INVALID_RESPONSE_LIMIT_SPI_READ);
        if(cmd0_response == sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE)
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
        // early return on failed CMD0
        return initialization_result_t::INIT_FAILED_ON_CMD0;
    }
    //================================================================================================================

    // CMD8
    //================================================================================================================
    // assert CS to start communication
    SPI_write(0xFF, SPI1);
    SPI_write(0xFF, SPI1);
    gpio_write(CS_ACTIVE_LOW, GPIO_D);

    sd_card_command_response_t cmd8_response = send_cmd8(NUM_INVALID_RESPONSE_LIMIT_SPI_READ);

    if(cmd8_response == sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE)
    {
        sd_card_information.sd_card_version = sd_card_version_t::VER_2;
    }
    else if(cmd8_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND || 
            cmd8_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR)
    {
        // An illegal command and/ or CRC error indicates the SD card is an older V1.X card
        sd_card_information.sd_card_version = sd_card_version_t::VER_1;
    }
    else
    {
        // early return on failed CMD8
        return initialization_result_t::INIT_FAILED_ON_CMD8;
    }

    // de-assert CS to end communication
    gpio_write(CS_INACTIVE_HIGH, GPIO_D);
    SPI_write(0xFF, SPI1);
    //================================================================================================================

    // CMD58
    //================================================================================================================

    //================================================================================================================





    return initialization_result_t::INIT_SUCCESS;
}

bool SDCard::read_boot_sector_information()
{
    return false;
}

SDCard::sd_card_command_response_t SDCard::send_cmd0(const uint16_t &num_invalid_response_limit) const
{
    const uint16_t command_0 = 0x40;
    const uint16_t crc_7 = 0x95; // crc7 of bytes 1-5 of command

    // Send 6-byte CMD0 command “40 00 00 00 00 95” to put the card in SPI mode
    SPI_write(command_0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(crc_7, SPI1); // CRC7

    for (uint16_t i = 0; i < num_invalid_response_limit; i++)
    {
        uint16_t spi_read_value = SPI_read(SPI1);
        if (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE))
        {
            return sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE;
        }
    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}

SDCard::sd_card_command_response_t SDCard::send_cmd8(const uint16_t &num_invalid_response_limit) const
{
    const uint16_t command_8 = 0x48;
    const uint16_t supported_voltage_range = 0x1; // Indicates range 2.7V - 3.6V supported
    const uint16_t check_pattern = 0xAA; // Should be repeated back by SD card
    const uint16_t crc_7 = 0x87; // crc7 of bytes 1-5 of command

    // Send 6-byte CMD8 command “48 00 00 01 AA 95” to tell the SD card which voltages it must accept
    SPI_write(command_8, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(supported_voltage_range, SPI1);
    SPI_write(check_pattern, SPI1);
    SPI_write(crc_7, SPI1);

    for (uint16_t i = 0; i < num_invalid_response_limit; i++)
    {
        uint16_t spi_read_value = SPI_read(SPI1);
        if (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE))
        {
            // discard two unused bytes of response which should both be 0
            SPI_read(SPI1);
            SPI_read(SPI1);

            const uint16_t spi_read_voltage_range_supported = SPI_read(SPI1);
            const uint16_t spi_read_repeated_check_pattern = SPI_read(SPI1);

            if (spi_read_voltage_range_supported != supported_voltage_range)
            {
                return sd_card_command_response_t::SD_CARD_UNSUPPORTED_VOLTAGE;
            }
            else if (spi_read_repeated_check_pattern != check_pattern)
            {
                return sd_card_command_response_t::SD_CARD_CHECK_PATTERN_ERROR;
            }

            return sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE;
            
        }
        else if(spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND))
        {
            return sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND;
        }
        else if(spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR))
        {
            return sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR;
        }
    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}
