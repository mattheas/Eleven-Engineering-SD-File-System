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
        sd_card_command_response_t cmd0_response = send_cmd0();
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
    gpio_write(CS_ACTIVE_LOW, GPIO_D);

    sd_card_command_response_t cmd8_response = send_cmd8();

    if(cmd8_response == sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED)
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
    // assert CS to start communication
    gpio_write(CS_ACTIVE_LOW, GPIO_D);

    sd_card_command_response_t cmd58_response = send_cmd58(true); // card should be in idle

    if(cmd58_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND || 
            cmd58_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR)
    {
        // An illegal command and/ or CRC error on V1.x indicates this is not an SD card at all
        return initialization_result_t::INIT_FAILED_ON_CMD58;
    }
    else if (cmd58_response == sd_card_command_response_t::SD_CARD_UNSUPPORTED_VOLTAGE)
    {
        // voltage unsupported so refrain from using SD card
        return initialization_result_t::INIT_FAILED_ON_CMD58;
    }
    else if(cmd58_response == sd_card_command_response_t::SD_CARD_NO_RESPONSE)
    {
        // no response from SD card which is unlikely
        return initialization_result_t::INIT_FAILED_ON_CMD58;
    }

    // de-assert CS to end communication
    gpio_write(CS_INACTIVE_HIGH, GPIO_D);
    SPI_write(0xFF, SPI1);
    //================================================================================================================

    // CMD55 & ACMD41
    //================================================================================================================
    bool sd_card_in_idle_state = true;
    
    do{
        // assert CS to start communication
        gpio_write(CS_ACTIVE_LOW, GPIO_D);

        sd_card_command_response_t cmd55_response = send_cmd55();

        if(cmd55_response == sd_card_command_response_t::SD_CARD_NO_RESPONSE)
        {
            // no response from SD card which is unlikely
            return initialization_result_t::INIT_FAILED_ON_CMD55;
        }

        // de-assert CS to end communication
        gpio_write(CS_INACTIVE_HIGH, GPIO_D);
        SPI_write(0xFF, SPI1);

        // assert CS to start communication
        gpio_write(CS_ACTIVE_LOW, GPIO_D);

        sd_card_command_response_t acmd41_response = send_acmd41();

        if (acmd41_response == sd_card_command_response_t::SD_CARD_NOT_IN_IDLE_MODE_RESPONSE)
        {
            // initialization is complete
            sd_card_in_idle_state = false;
        }
        else if (acmd41_response == sd_card_command_response_t::SD_CARD_NO_RESPONSE)
        {
            return initialization_result_t::INIT_FAILED_ON_ACMD41;
        }

        // de-assert CS to end communication
        gpio_write(CS_INACTIVE_HIGH, GPIO_D);
        SPI_write(0xFF, SPI1);

    } while (sd_card_in_idle_state);
    //================================================================================================================

    // CMD58 (only for V2.00)
    //================================================================================================================
    if (sd_card_information.sd_card_version == sd_card_version_t::VER_1)
    {
        // Reaching this point, and its known its a V1.X guarantees SDSC standard
        sd_card_information.sd_card_standard = sd_card_standard_t::SDSC;
    }
    else if (sd_card_information.sd_card_version == sd_card_version_t::VER_2)
    {
        sd_card_information.sd_card_version = sd_card_version_t::VER_2;

        // assert CS to start communication
        gpio_write(CS_ACTIVE_LOW, GPIO_D);

        sd_card_command_response_t cmd58_response = send_cmd58(false); // card should no longer be in idle

        if(cmd58_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND || 
                cmd58_response == sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR)
        {
            // An illegal command and/ or CRC error on V1.x indicates this is not an SD card at all
            return initialization_result_t::INIT_FAILED_ON_CMD58;
        }
        else if (cmd58_response == sd_card_command_response_t::SD_CARD_UNSUPPORTED_VOLTAGE)
        {
            // voltage unsupported so refrain from using SD card
            return initialization_result_t::INIT_FAILED_ON_CMD58;
        }
        else if(cmd58_response == sd_card_command_response_t::SD_CARD_NO_RESPONSE)
        {
            // no response from SD card which is unlikely
            return initialization_result_t::INIT_FAILED_ON_CMD58;
        }

        // de-assert CS to end communication
        gpio_write(CS_INACTIVE_HIGH, GPIO_D);
        SPI_write(0xFF, SPI1);

        // check CCS bit of OCR register to determine sd card standard  of V2.00 or later card
        if (sd_card_information.ocr_register_contents[0] & (1 << 6))
        {
            sd_card_information.sd_card_standard = sd_card_standard_t::SDHC_OR_SDXC;
        } else 
        {
            sd_card_information.sd_card_standard = sd_card_standard_t::SDSC;
        }
    }
    //================================================================================================================

    return initialization_result_t::INIT_SUCCESS;
}

SDCard::sd_card_command_response_t SDCard::send_cmd0() const
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

    for (uint16_t i = 0; i < NUM_INVALID_RESPONSE_LIMIT_SPI_READ; i++)
    {
        const uint16_t spi_read_value = SPI_read(SPI1);

        if (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE))
        {
            return sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE;
        }
    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}

SDCard::sd_card_command_response_t SDCard::send_cmd8() const
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

    for (uint16_t i = 0; i < NUM_INVALID_RESPONSE_LIMIT_SPI_READ; i++)
    {
        const uint16_t spi_read_value = SPI_read(SPI1);

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

            return sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED;
            
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

SDCard::sd_card_command_response_t SDCard::send_cmd58(const bool &expected_in_idle)
{
    const uint16_t command_0 = 0x7A;
    const uint16_t crc_7 = 0xFD; // crc7 of bytes 1-5 of command

    // Send 6-byte CMD0 command “7A 00 00 00 00 FD” to read the OCR register
    SPI_write(command_0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(crc_7, SPI1);

    for (uint16_t i = 0; i < NUM_INVALID_RESPONSE_LIMIT_SPI_READ; i++)
    {
        const uint16_t spi_read_value = SPI_read(SPI1);

        if(spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND) 
                && sd_card_information.sd_card_version == sd_card_version_t::VER_1)
        {
            return sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND;
        }
        else if(spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR) 
                && sd_card_information.sd_card_version == sd_card_version_t::VER_1)
        {
            return sd_card_command_response_t::SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR;
        }
        else if ((spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE) && expected_in_idle) ||
                (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_NOT_IN_IDLE_MODE_RESPONSE) && !expected_in_idle))
        {
            // the 4 bytes after idle more response are the contents of the OCR register

            // bits 31-24, contains CCS and power up status bit, not important yet
            const uint16_t ocr_register_byte1 = SPI_read(SPI1);

            // bits 23-16, all bits must be set, indicates voltage 2.8-3.6V supported
            const uint16_t ocr_register_byte2 = SPI_read(SPI1);

            //bits 15-8, bit 15 must be set as that indicates voltage 2.7-2.8V supported
            const uint16_t ocr_register_byte3 = SPI_read(SPI1);

            // bits 7-0, mostly reserved, of no importance
            const uint16_t ocr_register_byte4 = SPI_read(SPI1);

            // save OCR register contents
            sd_card_information.ocr_register_contents[0] = ocr_register_byte1;
            sd_card_information.ocr_register_contents[1] = ocr_register_byte2;
            sd_card_information.ocr_register_contents[2] = ocr_register_byte3;
            sd_card_information.ocr_register_contents[3] = ocr_register_byte4;

            // suppress unused variable warnings
            (void)ocr_register_byte1;
            (void)ocr_register_byte4;

            if(ocr_register_byte2 == 0xFF && ocr_register_byte3 == 0x80)
            {
                // all voltage ranges are supported
                return sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED;
            }
            else
            {
                return sd_card_command_response_t::SD_CARD_UNSUPPORTED_VOLTAGE;
            }

        }

    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}

SDCard::sd_card_command_response_t SDCard::send_cmd55() const
{
    const uint16_t command_55 = 0x77;
    const uint16_t crc_7 = 0x65; // crc7 of bytes 1-5 of command

    SPI_write(command_55, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(crc_7, SPI1);

    for (uint16_t i = 0; i < NUM_INVALID_RESPONSE_LIMIT_SPI_READ; i++)
    {
        const uint16_t spi_read_value = SPI_read(SPI1);

        if (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE) || 
            spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_NOT_IN_IDLE_MODE_RESPONSE))
        {
            return sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED;
        }
    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}

SDCard::sd_card_command_response_t SDCard::send_acmd41() const
{
    const uint16_t application_specific_command_41 = 0x69;
    const uint16_t support_sdhc_sdxc_cards = 0x40;
    const uint16_t crc_7 = 0x77; // crc7 of bytes 1-5 of command

    // Send 6-byte ACMD41 command “0x69  40 00 00 00  77” to send host capacity support information and activate the card initialization process.
    SPI_write(application_specific_command_41, SPI1);
    SPI_write(support_sdhc_sdxc_cards, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(0x0, SPI1);
    SPI_write(crc_7, SPI1);

    for (uint16_t i = 0; i < NUM_INVALID_RESPONSE_LIMIT_SPI_READ; i++)
    {
        const uint16_t spi_read_value = SPI_read(SPI1);

        if (spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE))
        {
            return sd_card_command_response_t::SD_CARD_IN_IDLE_MODE_RESPONSE;
        }
        else if(spi_read_value == static_cast<uint16_t>(sd_card_command_response_t::SD_CARD_NOT_IN_IDLE_MODE_RESPONSE))
        {
            return sd_card_command_response_t::SD_CARD_NOT_IN_IDLE_MODE_RESPONSE;
        }
    }

    return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
}

SDCard::sd_card_command_response_t SDCard::send_cmd17(uint16_t (&block)[512], const uint16_t &block_addr_byte1, const uint16_t &block_addr_byte2, 
                            const uint16_t &block_addr_byte3, const uint16_t &block_addr_byte4) const
{
    const uint16_t block_size_bytes = 512U;
    const uint16_t command_17 = 0x51;
    const uint16_t crc_7 = 0x00; // crc7 of bytes 1-5 of command

    // Send 6-byte CMD17 command “0x51  XX XX XX XX 00” to read a block from sd card
    SPI_write(command_17, SPI1);
    SPI_write(block_addr_byte1, SPI1);
    SPI_write(block_addr_byte2, SPI1);
    SPI_write(block_addr_byte3, SPI1);
    SPI_write(block_addr_byte4, SPI1);
    SPI_write(crc_7, SPI1);

    bool waiting_for_data = true;
    uint16_t num_invalid_reads = 0U;
    do{
        const uint16_t spi_read_value = SPI_read(SPI1);

        if (spi_read_value == 0xFE)
        {
            // exit loop when 0xFE is read, this indicates next byte is start of block
            waiting_for_data = false;
        }
        else
        {
            num_invalid_reads++;
            if (num_invalid_reads > NUM_INVALID_RESPONSE_LIMIT_SPI_READ)
            {
                // return early if num invalid read threshold is reached
                return sd_card_command_response_t::SD_CARD_NO_RESPONSE;
            }
        }

    }while(waiting_for_data);

    for (uint16_t i = 0;  i < block_size_bytes; i++)
    {
        block[i] = SPI_read(SPI1);
    }

    return sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED;
}