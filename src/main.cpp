#include <SystemClock.h>
#include <XPD.h>
#include <GPIO.h>
#include <Thread.h>

#include "main.h"
#include "../inc/SDCard.h"
#include "../inc/FileSystem.h"

using namespace sd_driver;
using namespace file_system;

// sys_clock_wait can only wait a maximum of 65535 ticks
// use a loop to get a longer delay.
void long_wait()
{
  for (int i = 0; i < 5000; ++i) {
    sys_clock_wait(10000);
  }
}
void short_wait()
{
  for (int i = 0; i < 1000; ++i) {
    sys_clock_wait(10000);
  }
}

// main() runs in thread 0
int main(void)
{
    SDCard my_sdcard(true);
    xpd_putc('\n');
    xpd_putc('\n');
    xpd_echo_int(static_cast<uint16_t>(my_sdcard.initialize_sd_card()), XPD_Flag_UnsignedDecimal);
    xpd_putc('\n');
    xpd_putc('\n');

    FileSystem my_filesystem(my_sdcard, FileSystem::file_system_t::FAT32);
    xpd_putc('\n');
    xpd_putc('\n');

    xpd_echo_int(static_cast<uint16_t>(my_sdcard.get_sd_card_information().sd_card_version), XPD_Flag_UnsignedDecimal); // 0x1 GOOD
    xpd_putc('\n');
    xpd_echo_int(static_cast<uint16_t>(my_sdcard.get_sd_card_information().sd_card_standard), XPD_Flag_UnsignedDecimal); // 0x1 GOOD
    xpd_putc('\n');

    while(true){continue;}

  return 0;
}
