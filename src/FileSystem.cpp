/**
 * @file FileSystem.cpp
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Implementation of File System class
 * @version 0.1
 * @date 2024-03-03
 */

#include "../inc/FileSystem.h"
#include <XPD.h>

using namespace file_system;

FileSystem::FileSystem(sd_driver::SDCard &_sd_card, const file_system_t &_file_system_type) : sd_card(_sd_card), file_system_type(_file_system_type)
{
    // Initialize SD card if its not already initalized

    // read MBR
    read_fat32_master_boot_record(); 

    // read VolumeID
    read_fat_32_volume_id(fat_32_master_boot_record.primary_partition_1.lba_begin);

    // xpd_echo_int(fat_32_volume_id.sectors_per_cluster, XPD_Flag_UnsignedDecimal); // 0x1 GOOD
    // xpd_putc('\n');
}

FileSystem::~FileSystem()
{
}

FileSystem::FAT32MasterBootRecord FileSystem::get_fat_32_master_boot_record() const
{
    return fat_32_master_boot_record;
}

FileSystem::FAT32VolumeID FileSystem::get_fat_32_volume_id() const
{
    return fat_32_volume_id;
}

bool FileSystem::read_fat32_master_boot_record()
{
    uint16_t mbr_512_byte_sector[512];

    sd_driver::SDCard::sd_card_command_response_t cmd17_response;

    uint16_t mbr_sector_address[4] = {0x00, 0x00, 0x00, 0x00};

    cmd17_response = sd_card.send_cmd17(mbr_512_byte_sector, mbr_sector_address);

    if(cmd17_response == sd_driver::SDCard::sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED)
    {
        // const uint16_t partition_1_first_byte_index = 446U;
        fat_32_master_boot_record.primary_partition_1.boot_flag = mbr_512_byte_sector[446];

        fat_32_master_boot_record.primary_partition_1.chs_begin[2] = mbr_512_byte_sector[447];
        fat_32_master_boot_record.primary_partition_1.chs_begin[1] = mbr_512_byte_sector[448];
        fat_32_master_boot_record.primary_partition_1.chs_begin[0] = mbr_512_byte_sector[449];

        fat_32_master_boot_record.primary_partition_1.type_code = mbr_512_byte_sector[450];

        fat_32_master_boot_record.primary_partition_1.chs_end[2] = mbr_512_byte_sector[451];
        fat_32_master_boot_record.primary_partition_1.chs_end[1] = mbr_512_byte_sector[452];
        fat_32_master_boot_record.primary_partition_1.chs_end[0] = mbr_512_byte_sector[453];

        fat_32_master_boot_record.primary_partition_1.lba_begin[3] = mbr_512_byte_sector[454];
        fat_32_master_boot_record.primary_partition_1.lba_begin[2] = mbr_512_byte_sector[455];
        fat_32_master_boot_record.primary_partition_1.lba_begin[1] = mbr_512_byte_sector[456];
        fat_32_master_boot_record.primary_partition_1.lba_begin[0] = mbr_512_byte_sector[457];

        fat_32_master_boot_record.primary_partition_1.number_of_sectors[3] = mbr_512_byte_sector[458];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[2] = mbr_512_byte_sector[459];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[1] = mbr_512_byte_sector[460];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[0] = mbr_512_byte_sector[461];

        fat_32_master_boot_record.mbr_signature[1] = mbr_512_byte_sector[510];
        fat_32_master_boot_record.mbr_signature[0] = mbr_512_byte_sector[511];

        // verify signature, check both ordering since documentation is often mixed
        const bool valid_signature = (fat_32_master_boot_record.mbr_signature[0] == 0x55 && fat_32_master_boot_record.mbr_signature[1] == 0xAA) ||
                                    (fat_32_master_boot_record.mbr_signature[0] == 0xAA && fat_32_master_boot_record.mbr_signature[1] == 0x55);

        // TODO read other 3 partitions

        // TODO check if its a guarantee that partition 1 is guarnteed to have FAT32
        // Check that type code and signature are valid
        if ((fat_32_master_boot_record.primary_partition_1.type_code == 0xB ||
            fat_32_master_boot_record.primary_partition_1.type_code == 0xC) && 
            valid_signature)
        {
            return true;    
        }

    }

    return false;
}

bool FileSystem::read_fat_32_volume_id(const uint16_t (&block_address)[4])
{
    uint16_t volume_id_sector[512];
    sd_driver::SDCard::sd_card_command_response_t cmd17_response;

    cmd17_response = sd_card.send_cmd17(volume_id_sector, block_address);

    for (uint16_t i = 0; i<3; i++)
    {
        fat_32_volume_id.jmp_to_boot_code[i] = volume_id_sector[i];
    }

    for (uint16_t i = 3; i<11; i++)
    {
        fat_32_volume_id.oem_name_ascii[i] = volume_id_sector[i];
    }

    // Together should be Byte0 + Byte1 == 512 bytes per sector
    //be careful b/c bytes are 02 and 00 in decimal which needs to be converted to hex 0x200 which is then 512
    fat_32_volume_id.bytes_per_sector[1] = volume_id_sector[11];
    fat_32_volume_id.bytes_per_sector[0] = volume_id_sector[12];

    fat_32_volume_id.sectors_per_cluster = volume_id_sector[13];

    fat_32_volume_id.size_of_reserved_area_sectors[1] = volume_id_sector[14];
    fat_32_volume_id.size_of_reserved_area_sectors[0] = volume_id_sector[15];

    // usually 2 fats
    fat_32_volume_id.number_of_fats = volume_id_sector[16];

    // should be zero for FAT 32
    fat_32_volume_id.max_num_files_in_root_dir[1] = volume_id_sector[17];
    fat_32_volume_id.max_num_files_in_root_dir[0] = volume_id_sector[18];

    // if ZERO check the extended 4 byte field
    fat_32_volume_id.number_of_sectors_in_file_system[1] = volume_id_sector[19];
    fat_32_volume_id.number_of_sectors_in_file_system[0] = volume_id_sector[20];

    if (volume_id_sector[21] == static_cast<uint16_t>(media_type_t::REMOVABLE_DISK))
    {
        fat_32_volume_id.media_type = media_type_t::REMOVABLE_DISK;
    }
    else if (volume_id_sector[21] == static_cast<uint16_t>(media_type_t::FIXED_DISK))
    {
        fat_32_volume_id.media_type = media_type_t::FIXED_DISK;
    }

    // Should be 0 for FAT32
    fat_32_volume_id.size_of_each_fat_in_sectors[1] = volume_id_sector[22];
    fat_32_volume_id.size_of_each_fat_in_sectors[0] = volume_id_sector[23];
        
    fat_32_volume_id.sectors_per_track_in_storage_device[1] = volume_id_sector[24];
    fat_32_volume_id.sectors_per_track_in_storage_device[0] = volume_id_sector[25];        

    fat_32_volume_id.num_heads_in_storage_device[1] = volume_id_sector[26];
    fat_32_volume_id.num_heads_in_storage_device[0] = volume_id_sector[27];

    fat_32_volume_id.num_of_sectors_before_start_partition[3] = volume_id_sector[28];
    fat_32_volume_id.num_of_sectors_before_start_partition[2] = volume_id_sector[29];
    fat_32_volume_id.num_of_sectors_before_start_partition[1] = volume_id_sector[30];
    fat_32_volume_id.num_of_sectors_before_start_partition[0] = volume_id_sector[31];
        
    // Will be 0 if the 2 byte field above is non-zero (bytes 19-20)
    fat_32_volume_id.num_of_sectors_in_file_system_extended[3] = volume_id_sector[32];
    fat_32_volume_id.num_of_sectors_in_file_system_extended[2] = volume_id_sector[33];
    fat_32_volume_id.num_of_sectors_in_file_system_extended[1] = volume_id_sector[34];
    fat_32_volume_id.num_of_sectors_in_file_system_extended[0] = volume_id_sector[35];

    fat_32_volume_id.sectors_per_fat[3] = volume_id_sector[36];
    fat_32_volume_id.sectors_per_fat[2] = volume_id_sector[37];
    fat_32_volume_id.sectors_per_fat[1] = volume_id_sector[38];
    fat_32_volume_id.sectors_per_fat[0] = volume_id_sector[39];

    // usually 2 
    fat_32_volume_id.root_directory_first_cluster[3] = volume_id_sector[44];
    fat_32_volume_id.root_directory_first_cluster[2] = volume_id_sector[45];
    fat_32_volume_id.root_directory_first_cluster[1] = volume_id_sector[46];
    fat_32_volume_id.root_directory_first_cluster[0] = volume_id_sector[47];
    
    // signature value should be 0x55AA or 0xAA55(if done backwards)
    fat_32_volume_id.volume_id_signature[1] = volume_id_sector[510];
    fat_32_volume_id.volume_id_signature[0] = volume_id_sector[511];

    // verify signature, check both ordering since documentation is often mixed
    const bool valid_signature = (fat_32_volume_id.volume_id_signature[0] == 0x55 && fat_32_volume_id.volume_id_signature[1] == 0xAA) ||
                                    (fat_32_volume_id.volume_id_signature[0] == 0xAA && fat_32_volume_id.volume_id_signature[1] == 0x55);

    // print all values
    // xpd_puts("sectors per fat\n");
    // for (uint16_t i=36; i<40;i++){
    //     xpd_echo_int(volume_id_sector[i], XPD_Flag_UnsignedDecimal); // 0x1 GOOD
    // xpd_putc('\n');
    // }
    // xpd_putc('\n');

    // xpd_puts("root directoy first cluster\n");
    // for (uint16_t i=44; i<48;i++){
    //     xpd_echo_int(volume_id_sector[i], XPD_Flag_UnsignedDecimal); // 0x1 GOOD
    // xpd_putc('\n');
    // }

    return valid_signature;
}

void FileSystem::add_4_byte_numbers(const uint16_t &num1_byte1, const uint16_t &num1_byte2, const uint16_t &num1_byte3, const uint16_t &num1_byte4,
                        const uint16_t &num2_byte1, const uint16_t &num2_byte2, const uint16_t &num2_byte3, const uint16_t &num2_byte4,
                        uint16_t &result_byte1, uint16_t &result_byte2, uint16_t &result_byte3, uint16_t &result_byte4)
{
    // TODO refactor to optimize 

    uint16_t carry_byte4 = 0;
    result_byte4 = num1_byte4 + num2_byte4;

    if (result_byte4 > 0xFF)
    {
        result_byte4-=256;
        carry_byte4 = 1;
    }

    uint16_t carry_byte3 = 0;
    result_byte3 = num1_byte3 + num2_byte3 + carry_byte4;
    
    if (result_byte3 > 0xFF)
    {
        result_byte3-=256;
        carry_byte3 = 1;
    }

    uint16_t carry_byte2 = 0;
    result_byte2 = num1_byte2 + num2_byte2 + carry_byte3;
    
    if (result_byte2 > 0xFF)
    {
        result_byte2-=256;
        carry_byte2 = 1;
    }

    
    result_byte1 = num1_byte1 + num2_byte1 + carry_byte2;

    if (result_byte1 > 0xFF)
    {
        // discard overflow
        result_byte1-=256;
    }
}
