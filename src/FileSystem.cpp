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
    // Initialize SD card if its not already initalized??

    // read MBR
    read_fat32_master_boot_record(); 

    // read VolumeID
    read_fat_32_volume_id(fat_32_master_boot_record.primary_partition_1.lba_begin);

    // Calculate sector address (lba) of the beginning of FAT table (there should be two FAT tables #1 and #2 fyi)
    //==============================================================================================================================================
    // fat_begin_lba = Partition_LBA_Begin + Number_of_Reserved_Sectors;

    uint16_t fat_begin_lba[4] = {0x00, 0x00, 0x00, 0x00};
    add_4_byte_numbers(fat_32_master_boot_record.primary_partition_1.lba_begin, {0x00, 0x00, fat_32_volume_id.size_of_reserved_area_sectors[0], fat_32_volume_id.size_of_reserved_area_sectors[1]}, fat_begin_lba);
    //==============================================================================================================================================

    // Calculate the sector address (lba) of the first cluster
    //==============================================================================================================================================
    // cluster_begin_lba = Partition_LBA_Begin + Number_of_Reserved_Sectors + (Number_of_FATs * Sectors_Per_FAT);

    // multiply number_of_fats by sector_per_fat to get total fat sectors, perform multiplication w/ repeated addition
    uint16_t total_fat_sectors[4] = {0x00, 0x00, 0x00, 0x00};
    for (uint16_t i=0; i<fat_32_volume_id.number_of_fats; i++)
    {
        add_4_byte_numbers(fat_32_volume_id.sectors_per_fat, total_fat_sectors, total_fat_sectors);
    }

    uint16_t cluster_begin_lba[4] = {0x00, 0x00, 0x00, 0x00};
    add_4_byte_numbers(fat_begin_lba, total_fat_sectors, cluster_begin_lba);
    //==============================================================================================================================================

    // used to convert a cluster number to a block address (which we can send to the sd card driver)
    //==============================================================================================================================================
    // lba_addr = cluster_begin_lba + (cluster_number - 2) * sectors_per_cluster;
    //==============================================================================================================================================

    // Read root directory (one sector for now)
    //==============================================================================================================================================
    constexpr uint16_t bytes_per_entry = 32U; 
    constexpr uint16_t attribute_byte_offset = 11U;
    constexpr uint16_t file_size_offset = 28U;

    // tracks where we are in the file systems entry array
    uint16_t file_systems_entry_index = 0U;

    // tracks what sector we are reading (if root directory takes up multiple sectors)
    uint16_t root_directory_sector_addr_lba[4] = {};
    root_directory_sector_addr_lba[0] = cluster_begin_lba[0];
    root_directory_sector_addr_lba[1] = cluster_begin_lba[1];
    root_directory_sector_addr_lba[2] = cluster_begin_lba[2];
    root_directory_sector_addr_lba[3] = cluster_begin_lba[3];

    uint16_t root_directory_sector[512] ={};
    sd_card.send_cmd17(root_directory_sector, root_directory_sector_addr_lba);

    // 512/32 = 16 entries per sector
    bool end_of_root_directory_found = false; // when a first byte is 00 EOD is found

    while (!end_of_root_directory_found)
    {   
        for (uint16_t i = 0; i < 16; i++) //16 directory entries per sector
        {
            // Check first byte in 32 byte entry for end of root directory
            if (root_directory_sector[i*bytes_per_entry] ==  0x00)
            {
                end_of_root_directory_found = true;
                // xpd_puts("END OF DIRECTORY FOUND");
                xpd_putc('E');
                xpd_putc('O');
                xpd_putc('D');
                xpd_putc('F');
                xpd_putc(' ');
                xpd_echo_int(i*bytes_per_entry, XPD_Flag_UnsignedDecimal);
                xpd_putc('\n');

                // allows all contents to print to terminal without being cutoff
                for (int i = 0; i < 1000; ++i) {
                    sys_clock_wait(10000);
                }
                break; 
            }

            // ignore directory entries that are deleted (i.e., start with 0xE5)
            if (root_directory_sector[i*bytes_per_entry] == 0xE5)
            {
                continue;
            }

            // Ignore LFN entries
            if (root_directory_sector[i*bytes_per_entry + attribute_byte_offset] == 0xF)
            {
                continue;
            }

            // VALID ENTRY, copy contents into entrys array
            if (root_directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<3) // 1<<3 = 0x8 a.k.a volume label
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::VOLUME_LABEL;
            }
            else if (root_directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<4) // 1<<4 = 0x10 a.k.a directory
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::DIRECTORY_ENTRY;
            }
            else if (root_directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<5) // 1<<5 = 0x20 a.k.a file
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::FILE_ENTRY;
            }

            file_system_entrys[file_systems_entry_index].entry_in_use = true;
            file_system_entrys[file_systems_entry_index].attribute_byte = root_directory_sector[i*bytes_per_entry + attribute_byte_offset];
            
            for (uint16_t j = 0; j < 11; j++)
            {
                file_system_entrys[file_systems_entry_index].name_of_entry[j] = root_directory_sector[i*bytes_per_entry + j];   
                xpd_putc(file_system_entrys[file_systems_entry_index].name_of_entry[j]); 
            }
            xpd_putc('\n');

            // Cluster addr high order bytes stored at offset 0x14 in LITTLE ENDIAN
            // while low order bytes are stored at offset 0x1A in LITTLE ENDIAN
            file_system_entrys[file_systems_entry_index].starting_cluster_address[0] = root_directory_sector[i*bytes_per_entry + 21];//MSB
            file_system_entrys[file_systems_entry_index].starting_cluster_address[1] = root_directory_sector[i*bytes_per_entry + 20];
            file_system_entrys[file_systems_entry_index].starting_cluster_address[2] = root_directory_sector[i*bytes_per_entry + 27];
            file_system_entrys[file_systems_entry_index].starting_cluster_address[3] = root_directory_sector[i*bytes_per_entry + 26];//LSB

            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[0] = root_directory_sector[i*bytes_per_entry + file_size_offset + 3]; //MSB
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[1] = root_directory_sector[i*bytes_per_entry + file_size_offset + 2];
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[2] = root_directory_sector[i*bytes_per_entry + file_size_offset + 1];
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[3] = root_directory_sector[i*bytes_per_entry + file_size_offset];     //LSB

            // increment index as an entry has been added to the entrys array
            file_systems_entry_index++;
        }

        // only send another read command if the end of directory has not been found
        if (!end_of_root_directory_found)
        {
            // An entire sector has been processed without finding end of directory so read next sector
            uint16_t count_of_one[4] = {0x0, 0x0, 0x0, 0x1};
            add_4_byte_numbers(count_of_one, root_directory_sector_addr_lba, root_directory_sector_addr_lba);

            sd_card.send_cmd17(root_directory_sector, root_directory_sector_addr_lba);
            xpd_putc('\n');
        }



    }

    // for (uint16_t i=0; i<512; i++){
    //     if(i % 16 == 0)
    //     {
    //         xpd_putc('\n');
    //     }
    //     if(i%32 == 0)
    //     {
    //         xpd_putc('\n');
    //     }

    //     xpd_echo_int(root_directory_sector[i], XPD_Flag_Hex);
    //     xpd_putc(' ');
    // }
    xpd_echo_int(file_system_entrys[2].starting_cluster_address[0], XPD_Flag_UnsignedDecimal);
    xpd_putc('\n');
    xpd_echo_int(file_system_entrys[2].starting_cluster_address[1], XPD_Flag_UnsignedDecimal);
    xpd_putc('\n');
    xpd_echo_int(file_system_entrys[2].starting_cluster_address[2], XPD_Flag_UnsignedDecimal);
    xpd_putc('\n');
    xpd_echo_int(file_system_entrys[2].starting_cluster_address[3], XPD_Flag_UnsignedDecimal);
    //==============================================================================================================================================

    //read contents of folder inside root directory
    //==============================================================================================================================================
    // lba_addr = cluster_begin_lba + (cluster_number - 2) * sectors_per_cluster;

    // uint16_t lba_offset[4] = {0x00, 0x00, 0x00, 0x00};
    // uint16_t sectors_per_cluster_4_byte_addr[4] = {0x0, 0x0, 0x0, fat_32_volume_id.sectors_per_cluster};
    // for (uint16_t i=0; i<1; i++) // cluster number 7-2 = 5
    // {
    //     add_4_byte_numbers(sectors_per_cluster_4_byte_addr, lba_offset, lba_offset);
    // }

    // uint16_t folder_lba_addr[4] = {0x00, 0x00, 0x00, 0x00};
    
    // add_4_byte_numbers(cluster_begin_lba, lba_offset, folder_lba_addr);

    // uint16_t folder[512] ={};

    // sd_card.send_cmd17(folder, folder_lba_addr);

    // for (uint16_t i=0; i<512; i++){
    //     if(i % 16 == 0)
    //     {
    //         xpd_putc('\n');
    //     }
    //     if(i%32 == 0)
    //     {
    //         xpd_putc('\n');
    //     }

    //     xpd_echo_int(folder[i], XPD_Flag_Hex);
    //     xpd_putc(' ');
    // }

    //==============================================================================================================================================


    // store root directory entries
    //==============================================================================================================================================

    //==============================================================================================================================================

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

void FileSystem::add_4_byte_numbers(const uint16_t (&number_1)[4], const uint16_t (&number_2)[4], uint16_t (&result)[4])
{
    // TODO refactor to optimize 

    uint16_t carry_byte4 = 0;
    result[3] = number_1[3] + number_2[3];

    if (result[3] > 0xFF)
    {
        result[3]-=256;
        carry_byte4 = 1;
    }

    uint16_t carry_byte3 = 0;
    result[2] = number_1[2] + number_2[2] + carry_byte4;
    
    if (result[2] > 0xFF)
    {
        result[2]-=256;
        carry_byte3 = 1;
    }

    uint16_t carry_byte2 = 0;
    result[1] = number_1[1] + number_2[1] + carry_byte3;
    
    if (result[1] > 0xFF)
    {
        result[1]-=256;
        carry_byte2 = 1;
    }

    
    result[0] = number_1[0] + number_2[0] + carry_byte2;

    if (result[0] > 0xFF)
    {
        // discard overflow
        result[0]-=256;
    }
}
