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

    // perform addition and store result in cluster_begin_lba for later use
    add_4_byte_numbers(fat_begin_lba, total_fat_sectors, cluster_begin_lba);
    //==============================================================================================================================================

    // Recursively read file system into file_system_entrys[]
    //==============================================================================================================================================
    uint16_t root_directory_sector_begin_addr[4] = {0x0, 0x0, 0x0, 0x0};
    calculate_sector_address_from_cluster_number(fat_32_volume_id.root_directory_first_cluster, root_directory_sector_begin_addr);

    read_directory_recursive(root_directory_sector_begin_addr ,nullptr);
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

void FileSystem::subtract_4_byte_numbers(const uint16_t (&number_1)[4], const uint16_t (&number_2)[4], uint16_t (&result)[4])
{
    uint16_t borrow_byte4 = 0;
    int16_t result_3 = number_1[3] - number_2[3];

    if (result_3 < 0)
    {
        result_3 += 256;
        borrow_byte4 = 1;
    }

    result[3] = result_3;


    uint16_t borrow_byte3 = 0;
    int16_t result_2 = number_1[2] - number_2[2] - borrow_byte4;
    
    if (result_2 < 0)
    {
        result_2 += 256;
        borrow_byte3 = 1;
    }

    result[2] = result_2;


    uint16_t borrow_byte2 = 0;
    int16_t result_1 = number_1[1] - number_2[1] - borrow_byte3;

    if (result_1 < 0)
    {
        result_1 += 256;
        borrow_byte2 = 1;
    }

    result[1] = result_1;


    int16_t result_0 = number_1[0] - number_2[0] - borrow_byte2;

    if (result_0 < 0)
    {
        // discard underflow
        result_0 += 256;
    }

    result[0] = result_0;
}

bool FileSystem::read_directory_recursive(const uint16_t (&directory_begin_sector_addr)[4], FAT32FileSystemEntry *parent_directory)
{
    // Constants that identify information about 32 byte directory entries
    constexpr uint16_t bytes_per_entry = 32U; 
    constexpr uint16_t attribute_byte_offset = 11U;
    constexpr uint16_t file_size_offset = 28U;
    // TODO assumes sector size of 512 bytes, update to be sector size/ bytes per entry
    constexpr uint16_t directory_entrys_per_sector = 16U;

    // Make a copy of given sector address so we can add to it for directories that span multiple sectors
    uint16_t directory_sector_addr_lba[4] = {};
    directory_sector_addr_lba[0] = directory_begin_sector_addr[0];
    directory_sector_addr_lba[1] = directory_begin_sector_addr[1];
    directory_sector_addr_lba[2] = directory_begin_sector_addr[2];
    directory_sector_addr_lba[3] = directory_begin_sector_addr[3];

    // Read at minimum the first sector in the directory
    uint16_t directory_sector[512] ={};
    sd_card.send_cmd17(directory_sector, directory_sector_addr_lba);

    // flag set when the first byte of a directory entry is 0x00 (indicating no more entrys)
    bool end_of_directory_found = false;

    while (!end_of_directory_found)
    {   
        for (uint16_t i = 0; i < directory_entrys_per_sector; i++)
        {
            // Check first byte in 32 byte entry for end of directory
            if (directory_sector[i*bytes_per_entry] == 0x00)
            {
                end_of_directory_found = true;
                break; 
            }

            // ignore directory entries that are deleted (i.e., start with 0xE5)
            if (directory_sector[i*bytes_per_entry] == 0xE5)
            {
                continue;
            }

            // Ignore LFN entries
            if (directory_sector[i*bytes_per_entry + attribute_byte_offset] == 0xF)
            {
                continue;
            }

            // ignore system or hidden entrys
            if ((directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<1) || // 1<<1 = 0x2 (hidden)
                 (directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<2))  // 1<<2 = 0x4 (system)
            {
                continue;
            }

            // ignore an entry that is a directory that is named "." or ".." these two entries tell us info
            // about the current directory and the enclosing directory but we do not care for this info
            if (directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<4 &&
                directory_sector[i*bytes_per_entry] == 0x2E)
            {
                // check for a second byte that is 0x2E "." OR 0x20 " "
                if (directory_sector[i*bytes_per_entry+1] == 0x2E ||
                        directory_sector[i*bytes_per_entry+1] == 0x20)
                {
                    bool non_space_found = false;

                    // iterate over remainder of bytes in entry name
                    for (uint16_t j = 2; j < 11; j++)
                    {
                        if (directory_sector[i*bytes_per_entry+j] != 0x20)
                        {
                            non_space_found = true;
                            break;
                        }
                    }

                    if (non_space_found == false)
                    {
                        continue;
                    }
                }
            }

            // VALID ENTRY, copy contents into entrys array
            if (directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<3) // 1<<3 = 0x8 (volume label)
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::VOLUME_LABEL;
            }
            else if (directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<4) // 1<<4 = 0x10 (directory)
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::DIRECTORY_ENTRY;
            }
            else if (directory_sector[i*bytes_per_entry + attribute_byte_offset] & 1<<5) // 1<<5 = 0x20 (file)
            {
                file_system_entrys[file_systems_entry_index].entry_type = directory_entry_t::FILE_ENTRY;
            }

            file_system_entrys[file_systems_entry_index].entry_in_use = true;
            file_system_entrys[file_systems_entry_index].attribute_byte = directory_sector[i*bytes_per_entry + attribute_byte_offset];
            
            for (uint16_t j = 0; j < 11; j++)
            {
                file_system_entrys[file_systems_entry_index].name_of_entry[j] = directory_sector[i*bytes_per_entry + j];   
                xpd_putc(file_system_entrys[file_systems_entry_index].name_of_entry[j]); 
            }
            xpd_putc('\n');

            // save reference to parent directory
            file_system_entrys[file_systems_entry_index].parent_directory = parent_directory;

            // Cluster addr high order bytes stored at offset 0x14 in LITTLE ENDIAN
            // while low order bytes are stored at offset 0x1A in LITTLE ENDIAN
            file_system_entrys[file_systems_entry_index].starting_cluster_address[0] = directory_sector[i*bytes_per_entry + 21];//MSB
            file_system_entrys[file_systems_entry_index].starting_cluster_address[1] = directory_sector[i*bytes_per_entry + 20];
            file_system_entrys[file_systems_entry_index].starting_cluster_address[2] = directory_sector[i*bytes_per_entry + 27];
            file_system_entrys[file_systems_entry_index].starting_cluster_address[3] = directory_sector[i*bytes_per_entry + 26];//LSB

            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[0] = directory_sector[i*bytes_per_entry + file_size_offset + 3]; //MSB
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[1] = directory_sector[i*bytes_per_entry + file_size_offset + 2];
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[2] = directory_sector[i*bytes_per_entry + file_size_offset + 1];
            file_system_entrys[file_systems_entry_index].size_of_entry_in_bytes[3] = directory_sector[i*bytes_per_entry + file_size_offset];     //LSB

            // increment index as an entry has been added to the entrys array
            file_systems_entry_index++;


            // recursively read directory when discovered
            if (file_system_entrys[file_systems_entry_index-1].entry_type == directory_entry_t::DIRECTORY_ENTRY)
            {
                // convert cluster address to sector address!!!!!!!!!!!
                uint16_t sector_address[4] = {0x0, 0x0, 0x0, 0x0};

                calculate_sector_address_from_cluster_number(file_system_entrys[file_systems_entry_index-1].starting_cluster_address, sector_address);

                read_directory_recursive(sector_address, &file_system_entrys[file_systems_entry_index-1]);
            }

        }

        // only send another read command if the end of directory has not been found
        if (!end_of_directory_found)
        {
            // An entire sector has been processed without finding end of directory so read next sector
            uint16_t count_of_one[4] = {0x0, 0x0, 0x0, 0x1};
            add_4_byte_numbers(count_of_one, directory_sector_addr_lba, directory_sector_addr_lba);

            sd_card.send_cmd17(directory_sector, directory_sector_addr_lba);
            xpd_putc('\n');
        }



    }



    // TODO check for too deep of recursion and return false
    return true;


}

void FileSystem::calculate_sector_address_from_cluster_number(const uint16_t (&cluster_number)[4], uint16_t (&resulting_sector_address)[4])
{
    // lba_addr = cluster_begin_lba + (cluster_number - 2) * sectors_per_cluster;

    uint16_t lba_offset[4] = {0x00, 0x00, 0x00, 0x00}; 
    uint16_t sectors_per_cluster_4_byte_addr[4] = {0x0, 0x0, 0x0, fat_32_volume_id.sectors_per_cluster};

    uint16_t cluster_number_minus_offset[4] = {0x0, 0x0, 0x0, 0x0};
    uint16_t cluster_offset[4] = {0x0, 0x0, 0x0, 0x2};

    // assumes cluster_number >= 2
    subtract_4_byte_numbers(cluster_number, cluster_offset, cluster_number_minus_offset);

    bool repeated_additions_left_to_perform = true;

    while (repeated_additions_left_to_perform)
    {
        if (cluster_number_minus_offset[0] == 0x00 &&
            cluster_number_minus_offset[1] == 0x00 &&
            cluster_number_minus_offset[2] == 0x00 &&
            cluster_number_minus_offset[3] == 0x00)
        {
            // no more multiplication to perform so set exit condition of loop
            repeated_additions_left_to_perform = false;
        }
        else{
            add_4_byte_numbers(sectors_per_cluster_4_byte_addr, lba_offset, lba_offset);
            uint16_t one_loop_iteration[4] = {0x0, 0x0, 0x0, 0x1};
            subtract_4_byte_numbers(cluster_number_minus_offset, one_loop_iteration, cluster_number_minus_offset);
        }
    }

    add_4_byte_numbers(cluster_begin_lba, lba_offset, resulting_sector_address);
}
