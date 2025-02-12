#include <curses.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <oda_decode.h>
#include <rds_spy_log_reader.h>
#include <rds_util.h>
#include <si470x.h>
#include <si470x_port.h>

struct si470x_t* g_tuner;
struct rds_oda_data* g_oda_data;

struct TunerDeleter {
  ~TunerDeleter() {
    if (g_tuner)
      si470x_delete(g_tuner);
    if (g_oda_data)
      delete_oda_data(g_oda_data);
  }
};

void show_options() {
  printf("[t] Tune to given frequency\n");
  printf("[u] Seek up\n");
  printf("[d] Seek down\n");
  printf("[v] Volume control\n");
  printf("[q] Quit\n");
}

void show_volume_options() {
  printf("\t[+] Volume up\n");
  printf("\t[-] Volume down\n");
  printf("\t[q] Quit volume\n");
}

int read_option() {
  int option;

  do {
    option = getchar();
  } while (isspace(option));

  return option;
}

int main() {
  int ret;
  g_oda_data = create_oda_data();

  struct si470x_port_t* port = port_create(false);

  const struct si470x_config_t config = {
      .port = port,
      .region = REGION_EUROPE,
      .advanced_ps_decoding = true,
      .gpio2_int_pin = 0,
      .reset_pin = 1,
      .i2c =
          {
              .bus = 1,
              .sdio_pin = 8,
              .sclk_pin = 9,
              .slave_addr = 0x10,
          },
  };
  g_tuner = si470x_create(&config);
  TunerDeleter tuner_deleter;

  if (!g_tuner) {
    fprintf(stderr, "Unable to create the tuner.\n");
    return 1;
  }

  auto power_on_tuner = [=]() {
    if (!si470x_power_on(g_tuner)) {
      fprintf(stderr, "Unable to power on tuner.\n");
      return 1;
    }

    return 0;
  };

  if ((ret = power_on_tuner()))
    return ret;

  int frequency = 94400000;
  if (!si470x_set_frequency(g_tuner, frequency)) {
    fprintf(stderr, "Unable to tune to frequency %d.\n", frequency);
    return 1;
  }

  int volume = 7;
  if (!si470x_set_volume(g_tuner, volume)) {
    fprintf(stderr, "Unable to set volume to %d.\n", volume);
    return 1;
  }

  si470x_set_mute(g_tuner, false);

  si470x_set_soft_mute(g_tuner, false);

  int option;
  bool done = false;

  while (!done) {
    show_options();

    bool reached_sfbl;

    printf("Enter option: ");
    option = read_option();
    switch (option) {
      case 't':
        float freq;
        printf("\tEnter frequency: ");
        scanf("%f", &freq);
        frequency = (int)(freq * 1000000);
        if (!si470x_set_frequency(g_tuner, frequency)) {
          fprintf(stderr, "Unable to tune to frequency %d.\n", frequency);
          return 1;
        }
        break;
      case 'u':
        si470x_seek_up(g_tuner, /*allow_wrap=*/true, &reached_sfbl);
        break;
      case 'd':
        si470x_seek_down(g_tuner, /*allow_wrap=*/true, &reached_sfbl);
        break;
      case 'v':
        while (true) {
          show_volume_options();
          printf("\tEnter option: ");
          int volume_option = read_option();

          if (volume_option == '+') {
            volume++;
          } else if (volume_option == '-') {
            volume--;
          } else if (volume_option == 'q') {
            break;
          }

          if (!si470x_set_volume(g_tuner, volume)) {
            fprintf(stderr, "Unable to set volume to %d.\n", volume);
            return 1;
          }
        }
        break;
      case 'q':
      case 'Q':
        done = true;
        break;
    }
  }
  return 0;
}